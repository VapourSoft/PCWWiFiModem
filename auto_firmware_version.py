import subprocess

Import("env")

isRelease = env.GetProjectOption("isRelease")

def get_firmware_specifier_build_flag():

    if isRelease == 'true':
        #Ensure no changes
        ret = subprocess.run(["git", "diff","--quiet"], stdout=subprocess.PIPE, text=True) 
        if ret.returncode == 1 :
            print ("ERROR: Cant build release - There are local or untracked changes")
            env.Exit(1);
        ret = subprocess.run(["git", "describe"], stdout=subprocess.PIPE, text=True) #Uses only annotated tags
    else:
        ret = subprocess.run(["git", "describe", "--tags"], stdout=subprocess.PIPE, text=True) #Uses any tags

    build_version = ret.stdout.strip()

    if (isRelease == 'true' and build_version.find('-')):
        print ("ERROR: Cant build release - Tag is not on current commit!")
        env.Exit(1);

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
