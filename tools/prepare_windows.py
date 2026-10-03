"""Fetch pinned official build tools locally; no system installation or PATH edits."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import hashlib
import urllib.request
import zipfile
import subprocess

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / '.tools'
ASSETS = [
    ('llvm-mingw.zip', 'https://github.com/mstorsjo/llvm-mingw/releases/download/20260922/llvm-mingw-20260922-ucrt-x86_64.zip', 'e3ad77d117a4bea19a7a3b333341824d79a5a371004a10e25b8504e7b3047666', TOOLS / 'llvm'),
    ('slang.zip', 'https://github.com/shader-slang/slang/releases/download/v2026.19/slang-2026.19-windows-x86_64.zip', 'fc922f214b5d115f663932e59e80e0f6b6cc6fae140979cf307da6459b6f8f33', TOOLS / 'slang'),
]

def fetch(asset):
    name, url, sha, target = asset
    marker = target / '.verified'
    if marker.exists() and marker.read_text() == sha:
        print(f'{name}: ready', flush=True)
        return
    archive = TOOLS / name
    if not archive.exists():
        print(f'{name}: downloading official release', flush=True)
        urllib.request.urlretrieve(url, archive)
    if hashlib.file_digest(archive.open('rb'), 'sha256').hexdigest() != sha:
        raise RuntimeError(f'{name}: checksum mismatch; remove this archive before retrying')
    with zipfile.ZipFile(archive) as z:
        for member in z.infolist():
            path = (target / member.filename).resolve()
            if not path.is_relative_to(target.resolve()):
                raise RuntimeError('Archive path outside target')
        z.extractall(target)
    marker.write_text(sha)
    print(f'{name}: verified and extracted', flush=True)

if __name__ == '__main__':
    TOOLS.mkdir(exist_ok=True)
    with ThreadPoolExecutor(max_workers=2) as pool:
        list(pool.map(fetch, ASSETS))
    headers = TOOLS / 'Vulkan-Headers'
    revision = '3c65a01745e4a1134d32b9c2c456472212dba16d'
    if not headers.exists():
        subprocess.run(['git', 'init', str(headers)], check=True)
        subprocess.run(['git', '-C', str(headers), 'fetch', '--depth=1', 'https://github.com/KhronosGroup/Vulkan-Headers.git', revision], check=True)
        subprocess.run(['git', '-C', str(headers), 'checkout', '--detach', 'FETCH_HEAD'], check=True)
    actual = subprocess.check_output(['git','-C',str(headers),'rev-parse','HEAD'], text=True).strip()
    if actual != revision:
        raise RuntimeError('Vulkan-Headers revision differs from the pinned build')
    print('Vulkan headers ready', flush=True)
