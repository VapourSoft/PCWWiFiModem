import subprocess

Import("env")

def get_firmware_specifier_build_flag():

    #Ensure no changes
    ret = subprocess.run(["git", "diff --quiet"], stdout=subprocess.PIPE, text=True) #Uses only annotated tags
    if ret.returncode == 1 :
        print ("ERROR: Cant build release - There are local or untracked changes")
        env.Exit(1);

    ret = subprocess.run(["git", "describe"], stdout=subprocess.PIPE, text=True) #Uses only annotated tags
    #ret = subprocess.run(["git", "describe", "--tags"], stdout=subprocess.PIPE, text=True) #Uses any tags
    build_version = ret.stdout.strip()
    
    #Stop release unless the codebase has been tagged
    if build_version == "" :
        print ("ERROR: Cant build release - Ensure there is a release tag and no local modifications")
        env.Exit(1);
    
    build_flag = "-D AUTO_VERSION=\\\"" + build_version + "\\\""
    print ("Building Release: " + build_version)
    return (build_flag)

env.Append(
    BUILD_FLAGS=[get_firmware_specifier_build_flag()]
)
