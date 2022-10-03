# PCW WiFi modem

This is a fork of the RetroWiFiModem project for building an Amstrad PCW8256/8512 Wifi Modem, most notably without the need for an additional RS232 interface such as the CPS8256.

Wiki is here: https://github.com/VapourSoft/PCWWiFiModem/wiki

The hardware design is here: https://github.com/VapourSoft/PCWWiFiModem/tree/master/schematic

For a ready made PCB, or even a full kit try: https://www.ebay.co.uk/sch/i.html?_nkw=pcw+wifi+modem

The hardware works with this firmware, but other firmware should also work providing it uses the D1 mini's TX/RX (and CTS/RTS if needed) pins.

## Building the hardware
Please refer to the wiki: https://github.com/VapourSoft/PCWWiFiModem/wiki/Hardware-Assembly

## Flashing the firmware
A quick guide is here: https://github.com/VapourSoft/PCWWiFiModem/wiki/Flashing-the-firmware

## First time setup

The default serial configuration is 9600bps, 8 data bits, no parity, 1
stop bit.

If using 9600 baud or higher it is recommended that you enable hardware flow control, and use CP/M 1.7 or later.

Here's the commands you need to set up the modem to automatically
connect to your WiFi network:

1. `AT$SSID=your WiFi network name` to set the WiFi network that the
modem will connect to when it powers up.
2. `AT$PASS=your WiFi network password` to set the password for the
network.
3. `ATC1` to connect to the network.
4. Optional stuff:
   * `AT$SB=speed` to set the default serial speed.
   * `AT$SU=dps` to set the data bits, parity and stop bits.
   * `ATNETn` to select whether or not to use Telnet protocol.
   * `AT&Kn` to use RTS/CTS flow control or not.
5. `AT&W` to save the settings.

Once you've done that, the modem will automatically connect to your WiFi
network on power up and will be ready to "dial up" a connection with
`ATDT`.  See: https://github.com/VapourSoft/PCWWiFiModem/wiki/Command-Reference


## References & Acknowledgements
Thanks to the cool RetroWifiModem project!
Please see the RetroWifiModem project for more information on the RetroWiFiModem including it's history, contributors and original hardware.  
