//
//   Retro WiFi modem: an ESP8266 based RS232<->WiFi modem
//   with Hayes style AT commands and blinking LEDs.
//
//   Originally based on
//   Original Source Copyright (C) 2016 Jussi Salin <salinjus@gmail.com>
//   Additions (C) 2018 Daniel Jameson, Stardot Contributors
//   Additions (C) 2018 Paul Rickards <rickards@gmail.com>
//
//   This program is free software: you can redistribute it and/or modify
//   it under the tertms of the GNU General Public License as published by
//   the Free Software Foundation, either version 3 of the License, or
//   (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

// see: https://github.com/mecparts/RetroWiFiModem 

/* REQUIRED TO PREVENT MODEM RESET BY WATCHDOG
What I did to quiet the watchdog down both when sending long strings at low baud rates and during long waits for RTS to come active again was to add a yield() call to the dead spin while loop, like so:

In cores/esp8266/uart.cpp

static void
uart_do_write_char(const int uart_nr, char c)
{
    while(uart_tx_fifo_full(uart_nr))
      yield();

    USF(uart_nr) = c;
}
This way, no matter how long the code has to wait for space in the transmit FIFO, the watchdog is kept well fed and quiet.
*/


#include <ESP8266WiFi.h>
#include <ESP_EEPROM.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <uart_register.h>
#include <uart.h>
#include <string.h>
#include "RetroWiFiModem.h"
#include "globals.h"
#include "support.h"
#include "at_basic.h"
#include "at_extended.h"
#include "at_proprietary.h"
#include "string.h"

// =============================================================
void setup(void) {

   pinMode(RI, OUTPUT);          //Not connected in current version
   digitalWrite(RI, !ACTIVE);    // not ringing

   //Ensure flow control is off while we try to connect automatically
   //This prevents our progress output from blocking the connection
   //If the computer is not ready
   setHardwareFlow(false);

   pinMode(RTS,OUTPUT);
   digitalWrite(RTS,!ACTIVE);    //SET RTS to not ready


   pinMode(NVRAM_RESET_PIN, INPUT_PULLUP);

   EEPROM.begin(sizeof(struct Settings));
   EEPROM.get(0, settings);

   //Force defaults if magic number (size of setings struct) is wrong, or NVRAM_RESET_PIN is LOW 
   if( settings.magicNumber !=  sizeof(struct Settings) || digitalRead(NVRAM_RESET_PIN) == LOW  ) {
      // reset or no valid data in EEPROM/NVRAM, populate with defaults
      factoryDefaults(NULL);
   }

   //Apply saved settings
   sessionTelnetType = settings.telnet;
   uint32_t baud = (settings.serialSpeed == 19200L) ? 17857L : settings.serialSpeed;

   //Initialize Serial
   Serial.begin(baud, getSerialConfig());
   
   //Uncomment for debugging
   //Serial.println(ESP.getResetInfo());   

   if( settings.startupWait ) {
      while( true ) {            // wait for a CR
         yield();
         if( Serial.available() ) {
            if( Serial.read() == CR ) {
               break;
            }
         }
      }
   }
   else //else because DI SERVER NOT COMPATIBLE WITH WAITING FOR A CR TO CONNECT!!!!
   {
      if (settings.diServer[0] != 0)
         enableLineBreakInterrupt();   
   }

   //Dont remember previous WifiConnection always load connection settings
   //from the modem settings
   WiFi.persistent(false);

   //Send TCP/IP data immediately
   WiFiClient::setDefaultNoDelay(true);  // disable Nalge algorithm by default

   
   //Try to connect automatically if ssid and password are set
   if( settings.ssid[0] && settings.wifiPassword[0] ) 
      doWiFiConnection();


   //Execute automatic command if it is set
   if( settings.autoExecute[0] ) {
      yield();
      strncpy(atCmd, settings.autoExecute, MAX_CMD_LEN);
      atCmd[MAX_CMD_LEN] = NUL;
      if( settings.echo ) {
         Serial.println(atCmd);
      }
      doAtCmds(atCmd);                  // auto execute command
   } else {
      sendResult(R_OK);
   }

   //Change flow control to the saved settings
   setHardwareFlow(settings.rtsCts);
}


//I added this to stop serial writes from blocking the loop - so other functions can happen
//although the patch to UART.cpp would stop watchdog on blocked serial out, why not get on with other stuff instead
//(eg sending or handling call disconnect) while waiting for tx buffer to clear.

/*
  Reference for uart_tx_fifo_available() and uart_tx_fifo_full():
  -Espressif Techinical Reference doc, chapter 11.3.7
  -tools/sdk/uart_register.h
  -cores/esp8266/esp8266_peri.h
  */
inline size_t
uart_tx_fifo_available(const int uart_nr)
{
    return (USS(uart_nr) >> USTXC) & 0xff;
}

inline bool
uart_tx_fifo_full(const int uart_nr)
{  
    return uart_tx_fifo_available(uart_nr) >= 0x7f;
}

void handleBreakCondition()
{

   if (settings.diServer[0] != 0)
   {
      //Ensure we dont sent any messages through serial
      bool prevQuiet = settings.quiet;
      int prevBaud = Serial.baudRate();

      settings.quiet = true;

      //End any call we may be in
      endCall();
    
      //This is important at these fast baud rates as 
      //else we get watchdog timer kick in!
      WiFiClient::setDefaultNoDelay(false);
      
      //Dont change serial config as it 
      //seems to rest uart remove the int handler.
      //Will assume the following setting are the default for now
      //settings.dataBits = 8;
      //settings.parity = 'N';
      //settings.stopBits = 1;
      //Serial.begin(31250L, getSerialConfig());

      //Set 31,250 Baud
      Serial.updateBaudRate(31250L);
            
      //Dial the DI server
      char number[MAX_SPEED_DIAL_LEN + 1];
      strncpy(number,settings.diServer,MAX_SPEED_DIAL_LEN + 1);
      dialNumber(number);

      //Enable hardware flow control
      setHardwareFlow(true);

      //On success send DI and version
      inDIMode = (state == ONLINE );          //Flag if are in DI mode

      if (inDIMode )
         Serial.print("DIOK");
      else{
         Serial.print("DIER");
         Serial.updateBaudRate(prevBaud);
         settings.quiet = prevQuiet;
      }
      
   }
   breakCondition = false;
}


// =============================================================
void loop(void) {

   if (breakCondition)
      handleBreakCondition();

   checkForIncomingCall();

   switch( state ) {

      case CMD_NOT_IN_CALL:
         if( WiFi.status() == WL_CONNECTED ) {
            ArduinoOTA.handle();
         }
         inAtCommandMode();
         break;

      case CMD_IN_CALL:
         inAtCommandMode();
         if( state == CMD_IN_CALL && !tcpClient.connected() ) {
            endCall();                    // hang up if not in a call
         }
         break;

      case PASSWORD:
         inPasswordMode();
         break;

      case ONLINE:
         if( Serial.available() ) {       // data from RS-232 to Wifi
            sendSerialData();
         }

         while( tcpClient.available() && !Serial.available() && !uart_tx_fifo_full(0) && !breakCondition ) { // data from WiFi to RS-232
            int c = receiveTcpData();
            if( c != -1 ) {
               Serial.write((char)c);
            }
         }

         if( !inDIMode &&  escCount == ESC_COUNT && millis() > guardTime ) {
            state = CMD_IN_CALL;          // +++ detected, back to command mode
            sendResult(R_OK);
            escCount = 0;
         }

         if( !tcpClient.connected() ) {   // no client?
            endCall();                    // then hang up
         }
         break;
   }
}

// =============================================================
void doAtCmds(char *atCmd) {
   size_t len;

   trim(atCmd);               // get rid of leading and trailing spaces
   if( atCmd[0] ) {
      // is it an AT command?
      if( strncasecmp(atCmd, "AT", 2) ) {
         sendResult(R_ERROR); // nope, time to die
      } else {
         // save command for possible future A/
         strncpy(lastCmd, atCmd, MAX_CMD_LEN);
         lastCmd[MAX_CMD_LEN] = NUL;
         atCmd += 2;          // skip over AT prefix
         len = strlen(atCmd);

         if( !atCmd[0] ) {
            // plain old AT
            sendResult(R_OK);
         } else {
            trim(atCmd);
            while( atCmd[0] ) {
               if( !strncasecmp(atCmd, "?", 1)  ) { // help message
                  // help
                  atCmd = showHelp(atCmd + 1);
               } else if( !strncasecmp(atCmd, "$SB", 3) ) {
                  // query/set serial speed
                  atCmd = doSpeedChange(atCmd + 3);
               } else if( !strncasecmp(atCmd, "$SU", 3) ) {
                  // query/set serial data configuration
                  atCmd = doDataConfig(atCmd + 3);
               } else if( !strncasecmp(atCmd, "$SSID", 5) ) {
                  // query/set WiFi SSID
                  atCmd = doSSID(atCmd + 5);
               } else if( !strncasecmp(atCmd, "$PASS", 5) ) {
                  // query/set WiFi password
                  atCmd = doWiFiPassword(atCmd + 5);
               } else if( !strncasecmp(atCmd, "C", 1) ) {
                  // connect/disconnect to WiFi
                  atCmd = wifiConnection(atCmd + 1);
               } else if( !strncasecmp(atCmd, "D", 1) && len > 2 && strchr("TPI", toupper(atCmd[1])) ) {
                  // dial a number
                  atCmd = dialNumber(atCmd + 2);
               } else if( !strncasecmp(atCmd, "DS", 2) && len == 3 ) {
                  // speed dial a number
                  atCmd = speedDialNumber(atCmd + 2);
               } else if( !strncasecmp(atCmd, "H", 1) || !strncasecmp(atCmd, "H0", 2) ) {
                  // hang up call
                  atCmd = hangup(atCmd + 1);
               } else if( !strncasecmp(atCmd, "&Z", 2) && isDigit(atCmd[2]) ) {
                  // speed dial query or set
                  atCmd = doSpeedDialSlot(atCmd + 2);
               } else if( !strncasecmp(atCmd, "O", 1) ) {
                  // go online
                  atCmd = goOnline(atCmd + 1);
               } else if( !strncasecmp(atCmd, "GET", 3) ) {
                  // get a web page (http only, no https)
                  atCmd = httpGet(atCmd + 3);
               } else if( settings.listenPort && !strncasecmp(atCmd, "A", 1) && tcpServer.hasClient() ) {
                  // manually answer incoming connection
                  atCmd = answerCall(atCmd + 1);
               } else if( !strncasecmp(atCmd, "S0", 2) ) {
                  // query/set auto answer
                  atCmd = doAutoAnswerConfig(atCmd + 2);
               } else if( !strncasecmp(atCmd, "$SP", 3) ) {
                  // query set inbound TCP port
                  atCmd = doServerPort(atCmd + 3);
               } else if( !strncasecmp(atCmd, "$BM", 3) ) {
                  // query/set busy message
                  atCmd = doBusyMessage(atCmd + 3);
               } else if( !strncasecmp(atCmd, "&R", 2) ) {
                  // query/set require password
                  atCmd = doServerPassword(atCmd + 2);
               } else if( !strncasecmp(atCmd, "I", 1) ) {
                  // show network information
                  atCmd = showNetworkInfo(atCmd + 1);
               } else if( !strncasecmp(atCmd, "Z", 1) ) {
                  // reset to NVRAM
                  atCmd = resetToNvram(atCmd + 1);
               } else if( !strncasecmp(atCmd, "&V", 2) ) {
                  // display current and stored settings
                  atCmd = displayAllSettings(atCmd + 2);
               } else if( !strncasecmp(atCmd, "&W", 2) ) {
                  // write settings to EEPROM
                  atCmd = updateNvram(atCmd + 2);
               } else if( !strncasecmp(atCmd, "&F", 2) ) {
                  // factory defaults
                  atCmd = factoryDefaults(atCmd);
               } else if( !strncasecmp(atCmd, "E", 1) ) {
                  // query/set command mode echo
                  atCmd = doEcho(atCmd + 1);
               } else if( !strncasecmp(atCmd, "Q", 1) ) {
                  // query/set quiet mode
                  atCmd = doQuiet(atCmd + 1);
               } else if( !strncasecmp(atCmd, "RD", 2)
                       || !strncasecmp(atCmd, "RT", 2) ) {
                  // read time and date
                  atCmd = doDateTime(atCmd + 2);
               } else if( !strncasecmp(atCmd, "V", 1) ) {
                  // query/set verbose mode
                  atCmd = doVerbose(atCmd + 1);
               } else if( !strncasecmp(atCmd, "X", 1) ) {
                  // query/set extended result codes
                  atCmd = doExtended(atCmd + 1);
               } else if( !strncasecmp(atCmd, "$W", 2) ) {
                  // query/set startup wait
                  atCmd = doStartupWait(atCmd + 2);
               } else if( !strncasecmp(atCmd, "$RST", 4) ) {
                  // show last reset/crash reason
                  atCmd = doResetReason(atCmd + 4);
               } else if( !strncasecmp(atCmd, "NET", 3) ) {
                  // query/set telnet mode
                  atCmd = doTelnetMode(atCmd + 3);
               } else if( !strncasecmp(atCmd, "$AE", 3) ) {
                  // do auto execute commands
                  atCmd = doAutoExecute(atCmd + 3);
               } else if( !strncasecmp(atCmd, "$TTY", 4) ) {
                  // do telnet terminal type
                  atCmd = doTerminalType(atCmd + 4);
               } else if( !strncasecmp(atCmd, "$TTL", 4) ) {
                  // do telnet location
                  atCmd = doLocation(atCmd + 4);
               } else if( !strncasecmp(atCmd, "$TTS", 4) ) {
                  // do telnet location
                  atCmd = doWindowSize(atCmd + 4);
               } else if( !strncasecmp(atCmd, "&K", 2) ) {
                  // do RTS/CTS flow control
                  atCmd = doFlowControl(atCmd + 2);
               } else if( !strncasecmp(atCmd, "$MDNS", 5) ) {
                  // handle mDNS name
                  atCmd = doMdnsName(atCmd + 5);
               } else if( !strncasecmp(atCmd, "$DI", 3) ) {
                  // handle mDNS name
                  atCmd = diServerAutoConnect(atCmd + 3);
               } else {
                  // unrecognized command
                  sendResult(R_ERROR);
               }
               trim(atCmd);
            }
         }
      }
   }
}