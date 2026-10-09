"""Exercise production Wi-Fi scan ownership/roaming with a simulated driver."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / ".pio/wifi_native"
out.mkdir(parents=True, exist_ok=True)
exe = out / "roaming"
subprocess.run([
    "clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
    "-fsanitize=address,undefined",
    f'-I{root / "tools/native_wifi"}', f'-I{root / "tools/native_ui"}',
    f'-I{root / "include"}', str(root / "src/wifi_network.cpp"),
    str(root / "tools/native_wifi/roaming.cpp"), "-o", str(exe),
], check=True)
for scenario in ("selection", "margin", "abandoned", "consumed", "failure", "setup", "wrap"):
    subprocess.run([str(exe), scenario], check=True)

