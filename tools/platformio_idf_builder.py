"""PlatformIO convenience targets; all compilation belongs to ESP-IDF."""
import subprocess
import sys
from pathlib import Path
from SCons.Script import AlwaysBuild, Default, DefaultEnvironment, COMMAND_LINE_TARGETS

env = DefaultEnvironment()
project = Path(env.subst("$PROJECT_DIR"))


def build_native(target, source, env):
    subprocess.run([sys.executable, str(project / "tools/build_firmware.py"),
                    "--prepared"], cwd=project, check=True)


def upload_native(target, source, env):
    env.AutodetectUploadPort()
    subprocess.run([sys.executable, str(project / "tools/flash_built_firmware.py"),
                    "--port", env.subst("$UPLOAD_PORT"),
                    "--baud", env.subst("$UPLOAD_SPEED")], check=True)


build = env.Alias("buildprog", [], build_native)
AlwaysBuild(build)
env.Alias("nobuild")
upload = env.Alias("upload", [] if "nobuild" in COMMAND_LINE_TARGETS else build,
                   upload_native)
AlwaysBuild(upload)
Default(build)
