"""Select the native delegate before PlatformIO loads its application builder."""
from pathlib import Path
import shutil

Import("env")

project = Path(env.subst("$PROJECT_DIR"))
env.Replace(BUILD_SCRIPT=str(project / "tools/platformio_idf_builder.py"))
# PlatformIO handles clean before evaluating BUILD_SCRIPT or SCons clean nodes.
if env.IsCleanTarget():
    shutil.rmtree(project / ".pio/idf-build", ignore_errors=True)
