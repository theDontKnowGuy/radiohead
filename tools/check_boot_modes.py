"""Check production mode persistence/HTTP handlers with simulated NVS and requests."""
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / ".pio/boot_mode_native"
out.mkdir(parents=True, exist_ok=True)
web = (root / "src/web_server.cpp").read_text()
fields = re.search(r"String bootModeStateFieldsJson\(\) \{.*?\n\}", web, re.S)
routes = re.findall(
    r'    server\.on\("/api/device/boot-mode", HTTP_(?:GET|POST), \[\] \{.*?\n    \}\);',
    web, re.S,
)
assert fields and len(routes) == 2, "Production boot mode handlers missing"
(out / "routes.inc").write_text(
    fields.group() + "\nvoid registerBootModeRoutes() {\n" + "\n".join(routes) + "\n}\n"
)
exe = out / "mode"
subprocess.run([
    "clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
    "-fsanitize=address,undefined",
    f'-I{root / "tools/native_boot_mode"}', f'-I{root / "tools/native_ui"}',
    f'-I{root / "include"}', f"-I{out}",
    str(root / "tools/native_boot_mode/mode.cpp"),
    str(root / "src/settings_boot_mode.cpp"), "-o", str(exe),
], check=True)
subprocess.run([str(exe)], check=True)
