from os.path import join, isfile
from os.path import sep

Import("env")


FRAMEWORK_DIR = env.PioPlatform().get_package_dir("framework-arduinoespressif8266")
patchflag_path = join(FRAMEWORK_DIR, ".patching-done")

print ("Checking if patching needed...")

# patch file only if we didn't do it before
if not isfile(join(FRAMEWORK_DIR, ".patching-done")):
    print ("Patching framework")

    def pjoin(*args, **kwargs):
        return join(*args, **kwargs).replace(sep, '/').replace("C:","/mnt/c")

    original_file = pjoin(FRAMEWORK_DIR, "cores", "esp8266", "uart.cpp")
    patched_file = pjoin("patches", "uart.cpp.patch")

    p = env.Execute("bash -c 'patch %s %s'" % (original_file, patched_file))

    if ( p == 0 ) : 
        def _touch(path):
            with open(path, "w") as fp:
                fp.write("")

        env.Execute(lambda *args, **kwargs: _touch(patchflag_path))
    else:
        print ("Patching Failed !!!!!")

    # cmd = '%s %s "%s" "%s"' % (sys.executable, "patch.ph"", original_file, patched_file)

    # env.Execute("touch " + patchflag_path)
else:
    print ("Patching already done")
