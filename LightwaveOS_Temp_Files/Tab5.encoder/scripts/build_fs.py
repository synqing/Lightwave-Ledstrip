import os
Import("env")

# Add mklittlefs to PATH for filesystem builds
def add_mklittlefs_to_path(source, target, env):
    tool_path = env.PioPlatform().get_package_dir("tool-mklittlefs")
    if tool_path and os.path.isdir(tool_path):
        env.PrependENVPath("PATH", tool_path)
        print(f"Added mklittlefs to PATH: {tool_path}")

env.AddPreAction("$BUILD_DIR/littlefs.bin", add_mklittlefs_to_path)
