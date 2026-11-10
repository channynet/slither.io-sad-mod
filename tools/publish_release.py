"""Publish both platforms together, keeping incomplete uploads in a draft release."""
import json
import os
from pathlib import Path
import re
import subprocess


def publish(root: Path, tag: str, run=subprocess.run):
    version = re.search(r'APP_VERSION="([^"]+)"',
                        (root / 'tools/build_windows.py').read_text(encoding='utf-8')).group(1)
    if tag != 'v' + version:
        raise ValueError('Tag does not match application version')
    names = [f'SlitherNative-{version}-Windows-x64.zip',
             f'SlitherNative-{version}-macOS-Universal.zip', 'SlitherNative-source.zip']
    files = [root / 'release' / name for name in names]
    if any(not path.is_file() or path.stat().st_size == 0 for path in files):
        raise ValueError('Both platform packages and corresponding source are required')
    result = run(['gh', 'release', 'view', tag, '--json', 'isDraft'],
                 capture_output=True, text=True)
    if result.returncode == 0:
        if not json.loads(result.stdout)['isDraft']:
            raise ValueError('Refusing to replace an already published release')
    else:
        run(['gh', 'release', 'create', tag, '--draft', '--verify-tag',
             '--title', f'SlitherNative {version}', '--generate-notes'], check=True)
    run(['gh', 'release', 'upload', tag, *map(str, files), '--clobber'], check=True)
    result = run(['gh', 'release', 'view', tag, '--json', 'assets'],
                 check=True, capture_output=True, text=True)
    uploaded = {asset['name']: asset['size'] for asset in json.loads(result.stdout)['assets']}
    if any(uploaded.get(path.name) != path.stat().st_size for path in files):
        raise ValueError('Uploaded release is incomplete; leaving it as a draft')
    run(['gh', 'release', 'edit', tag, '--draft=false', '--latest'], check=True)


if __name__ == '__main__':
    publish(Path(__file__).resolve().parents[1], os.environ['GITHUB_REF_NAME'])
