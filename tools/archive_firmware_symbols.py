"""Immutable, hash-addressed local build archives (no device data or credentials)."""
import argparse
import json
from pathlib import Path
import shutil
import tempfile

from firmware_artifacts import digest, validate_export


def archive_symbols(output, sdkconfig, destination, variant):
    manifest = validate_export(output)
    build_id = manifest['sha256']['firmware.elf']
    destination.mkdir(parents=True, exist_ok=True)
    target = destination / build_id
    if target.exists():
        validate_export(target)
        metadata = json.loads((target / 'symbols.json').read_text())
        if (metadata['build'] != build_id or metadata['configSha256'] != digest(sdkconfig)
                or metadata['configSha256'] != digest(target / 'sdkconfig') or metadata['variant'] != variant
                or any(digest(target / name) != value for name, value in metadata['maps'].items())):
            raise ValueError('Existing symbol archive metadata differs')
        return target
    with tempfile.TemporaryDirectory(prefix='.pending-', dir=destination) as temp:
        staged = Path(temp) / 'image'
        staged.mkdir()
        for name in manifest['sha256']:
            path = staged / name
            path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(output / name, path)
        shutil.copyfile(output / 'artifacts.json', staged / 'artifacts.json')
        shutil.copyfile(sdkconfig, staged / 'sdkconfig')
        root = Path(__file__).resolve().parents[1]
        # These maps must remain paired with the numeric breadcrumbs of this build.
        for name in ('web_diagnostic_routes.h', 'radio_diagnostics.h'):
            shutil.copyfile(root / 'include' / name, staged / name)
        metadata = {'build': build_id, 'variant': variant, 'configSha256': digest(sdkconfig),
                    'maps': {name: digest(staged / name) for name in ('web_diagnostic_routes.h', 'radio_diagnostics.h')}}
        (staged / 'symbols.json').write_text(json.dumps(metadata, indent=2) + '\n')
        validate_export(staged)
        staged.rename(target)
    return target


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('artifacts', type=Path)
    parser.add_argument('sdkconfig', type=Path)
    parser.add_argument('--destination', type=Path, default=Path('.pio/symbols'))
    parser.add_argument('--variant', default='radio')
    args = parser.parse_args()
    print(archive_symbols(args.artifacts, args.sdkconfig, args.destination, args.variant))
