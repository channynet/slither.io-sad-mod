"""Build the vendored Snake Missile worker; no Go installation is needed by players."""
from pathlib import Path
import json
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'third_party/snake-missile'

def go_tool():
    local = ROOT / '.tools/go-sdk/go/bin/go.exe'
    return str(local) if local.exists() else (shutil.which('go') or 'go')

def build_worker(output, goos='windows', goarch='amd64'):
    output = Path(output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env.update(GOOS=goos, GOARCH=goarch, CGO_ENABLED='0', GOTOOLCHAIN='local')
    flags = '-s -w' + (' -H=windowsgui' if goos == 'windows' else '')
    subprocess.run([go_tool(), 'build', '-trimpath', '-buildvcs=false', '-ldflags='+flags,
                    '-o', str(output), './cmd/native-worker'], cwd=SOURCE, env=env, check=True)

def copy_licenses(destination):
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    shutil.copy2(SOURCE/'LICENSE', destination/'LICENSE')
    goroot = Path(subprocess.check_output([go_tool(), 'env', 'GOROOT'], text=True, encoding='utf-8').strip())
    shutil.copy2(goroot/'LICENSE', destination/'Go-LICENSE')
    listing = subprocess.check_output([go_tool(), 'list', '-m', '-json', 'all'], cwd=SOURCE, text=True, encoding='utf-8')
    decoder = json.JSONDecoder()
    while listing.strip():
        module, length = decoder.raw_decode(listing.lstrip())
        listing = listing.lstrip()[length:]
        if module.get('Main') or 'Dir' not in module:
            continue
        name = module['Path'].replace('/', '_')+'-'+module['Version']
        for pattern in ('LICENSE*', 'COPYING*', 'NOTICE*'):
            for source in Path(module['Dir']).glob(pattern):
                if source.is_file():
                    shutil.copy2(source, destination/(name+'-'+source.name))

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', required=True)
    parser.add_argument('--os', default='windows')
    parser.add_argument('--arch', default='amd64')
    args = parser.parse_args()
    build_worker(args.output, args.os, args.arch)
