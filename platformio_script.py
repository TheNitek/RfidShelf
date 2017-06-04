Import("env")
import os
import shutil

def after_build(source, target, env):
    shutil.copy(target[0].path, "./build/latest.bin")


env.AddPostAction("$BUILD_DIR/firmware.bin", after_build)