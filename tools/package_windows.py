"""Build a clean Windows ZIP from explicit runtime assets, never user data."""
from pathlib import Path
import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
import zipfile
import build_windows as build

ROOT = build.ROOT
VERSION = re.search(r'APP_VERSION="([^"]+)"', (ROOT / 'tools/build_windows.py').read_text(encoding='utf-8')).group(1)
NAME = f'SlitherNative-{VERSION}-Windows-x64'
OUTPUT = ROOT / 'build' / 'releases'

README = f'''Slither Native {VERSION} · Windows x64

1. ZIP을 폴더에 모두 풀어 주세요.
2. SlitherNative.exe를 실행하세요. app 폴더를 함께 유지해 주세요.
3. 닉네임을 입력하고 서버 주소를 확인한 뒤 Play를 누르세요.
4. F8 → 설정에서 본인의 NTL 인증키/팀 ID를 입력할 수 있습니다.

주요 조작: 마우스 이동, Space 가속, 휠 확대/축소, F11 전체 화면.
H 두 번 또는 F8: 팀 채팅. T 두 번: BOT. V 두 번: 화면 정보.
오른쪽 아래 고양이 애니메이션은 메뉴에서만 표시됩니다.

이 배포본에는 기존 사용자의 닉네임·팀키·서버 기록·설정·개인 이미지·로그가 없습니다.
닉네임은 빈 상태이고 서버 주소는 프로그램 공통 기본값입니다.
처음 실행하면 새 설정과 기본 태그/cosmetic 목록이 자동 생성됩니다.
같은 Windows 계정에서 이전에 저장한 팀키가 있다면 해당 PC의 보관함에서 다시 읽을 수 있습니다.
이는 ZIP에 포함된 키가 아닙니다. 팀키는 LOCALAPPDATA/SlitherNative에 보관됩니다.

필요 환경: Windows x64, Vulkan을 지원하는 그래픽 드라이버.
동봉한 source 폴더에는 이 실행 파일의 소스와 빌드 스크립트가 있습니다.
LICENSE 및 THIRD-PARTY-NOTICES.md와 licenses 폴더를 함께 보관하세요.
'''

def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    # Fresh staging directories prevent files from an earlier personal run leaking in.
    with tempfile.TemporaryDirectory(prefix='clean-package-', dir=build.BUILD) as temp:
        staging = Path(temp).resolve()
        if staging.parent != build.BUILD.resolve():
            raise RuntimeError("Staging must stay inside the build directory")
        build.DIST = staging / 'compiled'
        build.main()
        package = staging / NAME
        package.mkdir()
        shutil.copy2(build.DIST / 'SlitherNative.exe', package / 'SlitherNative.exe')
        patterns = ['fonts/*.ttf', 'fonts/*.txt', 'textures/*.png',
                    'shaders/bin/*.spv', 'personal/library/*.png', 'menu/cat-knock-*.png']
        for pattern in patterns:
            for source in (ROOT / 'app/res').glob(pattern):
                target = package / source.relative_to(ROOT)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, target)
        shutil.copytree(build.DIST / 'licenses', package / 'licenses')
        for name in ['LICENSE', 'THIRD-PARTY-NOTICES.md']:
            shutil.copy2(ROOT / name, package / name)
        (package / 'README-KO.txt').write_text(README, encoding='utf-8-sig')

        # Corresponding source, without Git history, developer notes, build outputs or credentials.
        tracked = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode().split('\0')
        sources = set(filter(None, tracked)) | {'tools/package_windows.py'}
        for relative in sorted(sources):
            if relative in {'README.md', 'README-NATIVE.md'}:
                continue
            path = Path(relative)
            if path.parts[0] in {'.git', '.tools', 'build', 'dist'} or path.suffix.lower() in {'.dat', '.ini', '.csv', '.rgba'}:
                raise RuntimeError(f'Unexpected private/generated source entry: {relative}')
            target = package / 'source' / path
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / path, target)
        (package / 'source/README.md').write_text(
            f'# Slither Native {VERSION}\n\nWindows x64 build from this source snapshot:\n\n'
            '1. Install Python 3 and Git.\n2. Run `python tools/prepare_windows.py`.\n'
            '3. Run `python tools/build_windows.py`.\n4. Output: `dist/SlitherNative.exe`.\n\n'
            'Build dependencies are pinned in tools/prepare_windows.py. '
            'The package contains no Git history or personal configuration.\n', encoding='utf-8')

        # Scan for the current Windows profile and build directory without reading any secrets.
        private_strings = [str(ROOT), ROOT.as_posix(), str(Path.home()), Path.home().as_posix()]
        if os.environ.get('USERNAME'):
            private_strings.append(os.environ['USERNAME'])
        needles = [value.encode(enc) for value in private_strings for enc in ('utf-8', 'utf-16le') if len(value) >= 3]
        manifest = {}
        for file in sorted(package.rglob('*')):
            if not file.is_file():
                continue
            data = file.read_bytes()
            if any(needle in data for needle in needles):
                raise RuntimeError(f'Local personal path/name found: {file.relative_to(package)}')
            manifest[file.relative_to(package).as_posix()] = hashlib.sha256(data).hexdigest()
        (package / 'SHA256SUMS.txt').write_text(''.join(f'{sha}  {name}\n' for name, sha in manifest.items()), encoding='utf-8')
        archive = OUTPUT / f'{NAME}.zip'
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
            for file in sorted(package.rglob('*')):
                if file.is_file():
                    z.write(file, file.relative_to(staging).as_posix())
        with zipfile.ZipFile(archive) as z:
            bad = z.testzip()
            if bad:
                raise RuntimeError(f'ZIP validation failed: {bad}')
        print(json.dumps({'zip': str(archive), 'bytes': archive.stat().st_size,
                          'files': len(manifest) + 1, 'sha256': hashlib.sha256(archive.read_bytes()).hexdigest()}, ensure_ascii=False), flush=True)

if __name__ == '__main__':
    main()
