"""Exercise production settings snapshots and HTTP upload handlers with synthetic data."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[1]
out = root / '.pio/settings_backup_native'
out.mkdir(parents=True, exist_ok=True)
common = ['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-g',
          '-fsanitize=address,undefined', f'-I{root / "tools/native_settings_backup"}',
          f'-I{root / "include"}']
subprocess.run(common + [str(root / 'src/settings_backup.cpp'),
                        str(root / 'tools/native_settings_backup/storage.cpp'),
                        '-o', str(out / 'storage')], check=True)
subprocess.run([str(out / 'storage')], check=True)
web = (root / 'src/web_server.cpp').read_text()
state = web[web.index('SettingsBackup::Buffer settingsUpload;'):web.index('bool webStationConnected')]
handlers = web[web.index('void clearSettingsUpload()'):web.index('void failFirmwareUpload(')]
(out / 'web.inc').write_text(state + handlers)
subprocess.run(common + [f'-iquote{out}', str(root / 'src/settings_backup.cpp'),
                        str(root / 'tools/native_settings_backup/web.cpp'),
                        '-o', str(out / 'web')], check=True)
subprocess.run([str(out / 'web')], check=True)
