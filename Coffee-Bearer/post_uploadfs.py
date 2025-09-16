# This script tells PlatformIO to upload the main firmware
# right after the file system has been uploaded.

Import("env") # type: ignore

def after_uploadfs(source, target, env):
    print(">>> File system uploaded successfully.")
    print(">>> Now uploading main firmware for the '{}' environment...".format(env["PIOENV"]))
    env.Execute("$PYTHONEXE -m platformio run --target upload --environment " + env["PIOENV"])

env.AddPostAction("uploadfs", after_uploadfs) # type: ignore