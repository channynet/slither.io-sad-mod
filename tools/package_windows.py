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
4. F8 → 채팅창 아래 … → 팀 연결 / 설정에서 본인의 NTL 인증키/팀 ID를 입력할 수 있습니다.
   NTL처럼 이름을 붙여 여러 팀키를 저장하고 "저장된 팀키" 목록에서 골라 연결합니다(연결 중에도 바꾸기 가능).

주요 조작: 마우스 이동, Space 가속, 휠 확대/축소, F11 전체 화면.
Q 누르는 동안: 마우스로 방향을 조절하며 돌진. 빨간 선과 원은 조향 방향입니다. Q를 놓으면 기존 조작으로 돌아갑니다.
H 두 번 또는 F8: 팀 채팅. T 두 번: BOT. V 두 번: 화면 정보.
오른쪽 아래 고양이 애니메이션은 메뉴에서만 표시됩니다.
팀원 현황 창에는 FPS·핑·점수·참가 서버가 표시됩니다. 채팅창 아래 … → 팀원 현황에서 켜고 끕니다.
NTL 팀 연결이 필요하며, 수치가 없거나 오래된 경우 —로 표시합니다. 창 위치·크기·표시 여부는 저장됩니다.
채팅은 입력창을 클릭해서 시작합니다. Enter는 전송, Esc 또는 입력창 밖 클릭은 게임으로 복귀입니다.
채팅 중에도 지렁이는 마우스 방향을 따라갑니다. 채팅 입력·클릭으로는 가속하지 않습니다.
입력하지 않을 때는 일반 게임 단축키가 동작합니다. Enter/Tab으로 채팅이 자동 활성화되지 않습니다.
입력창에서 Ctrl+A/C/X/V 및 우클릭 복사·붙여넣기를 사용할 수 있습니다.
받은 메시지는 우클릭하여 복사할 수 있습니다. 채팅 글자는 18px이며 한글 수신 처리가 개선되었습니다.
Ctrl+F3: 팀원 닉네임 강조. 팀 연결 / 설정 → 이름 색상에서도 변경할 수 있습니다.
스킨 설정의 태그 / cosmetic 창 → 얼굴에서 웃는 얼굴·집게 모양 눈을 선택할 수 있습니다.
원래 게임의 웃는 얼굴·빨간 눈자루 모양입니다. 모든 몸 색상과 조합하며 두 가지를 함께 켤 수 있습니다. 얼굴 장식은 내 화면에만 표시됩니다.
같은 창의 그라데이션 탭에서 2~4개 색을 골라 머리부터 꼬리까지 이어지는 스킨을 만들 수 있습니다.
Ctrl+F2로 켜고 끕니다. 색상·사용 여부는 자동 저장되며 내 화면에만 표시됩니다.
같은 창의 불꽃 탭(또는 채팅창 아래 … → 설정 → 불꽃 스킨)에서 몸이 불타는 스킨을 켤 수 있습니다. 내 화면에만 보입니다.
설정 → 안전 벽: NTL의 Safewall과 같은 방식입니다(기본 켜짐). 벽 가까이에서 마우스가 맵 밖을 가리키면 벽을 따라 미끄러집니다.
  방식 1·2(NTL 기본 2)와 벽 오프셋 -10~10(NTL Border assist offset, 기본 0)을 고릅니다. 벽에 자주 죽으면 오프셋을 올리세요.
  "벽과 거리"(기본 1 = 완전 밀착, 0~20)로 벽에서 얼마나 안쪽을 탈지 정합니다. 핑이 높아 벽에 죽으면 올리세요.
내 FPS와 핑은 V 두 번(화면 정보)으로 볼 수 있습니다.
내 FPS·핑은 팀 연결 시 팀원에게도 보입니다(NTL 사용 팀원은 NTL 설정 "Show player details"를 켜야 표시).
S 두 번(플레이 중): SOS. 팀원 화면에 빨간색으로 보이고 "Help me!"를 보냅니다(NTL과 같음). 팀원의 SOS는 화면 위에 알림으로 뜹니다.
벽 상태: 벽이 줄어들면 빨강, 멈추면 주황, 넓어지면 초록으로 벽을 그립니다.
F1/F2: 반시계/시계 방향으로 계속 돌기(같은 키로 끄기, NTL과 같음).
- 두 번: 팀 채팅 기록 지우기.
NTL 태그: 다른 NTL 사용자의 태그가 보이고, Skin editor → 태그 탭에서 내 NTL 태그(무료 태그, 또는 NTL에서 받은 비밀번호 태그)를 고를 수 있습니다.
J 두 번: 모든 태그 숨기기/보이기. 태그 탭 맨 위에서 내 태그와 다른 사람 태그의 크기·줄 길이·줄 굵기·불투명도를 따로 정합니다.
태그 공유: tag-share-worker 폴더의 deploy.cmd 로 공유 서버를 올리고 그 주소를 태그 탭 "태그 공유 서버"에 넣으면, 같은 주소를 쓰는 이 클라이언트 사용자끼리 이미지 태그와 비밀번호 없이 쓰는 태그가 서로 보입니다.
Z: 스크린샷 (screenshots 폴더에 PNG). X: 시작 줌으로 되돌리기. 설정 → "시작 줌"으로 게임 시작 줌을 정합니다.
E 누르는 동안: 가장 가까운 적의 몸에 히트박스에 맞춰 붙어서 따라가기(지연·회전 계산, 빠져나갈 길을 남김). 가속 중에는 빠져나갈 길이 없거나 머리를 앞지르게 되면 가속을 놓습니다. 거리는 설정 → "몸 붙기".
7 두 번: 그래픽 (기본 스킨 / 스킨 없음 / 고대비). 설정 → 그래픽에서 내 스킨 유지, 다른 지렁이 불투명도도 바꿉니다.
팀원 현황 창에서 팀원이 있는 서버 옆 "참가"를 누르면 그 서버로 바로 들어갑니다(게임 중이면 나갔다가 다시 접속).
시작 화면 서버 주소 옆 ▼ 목록에도 팀원이 있는 서버가 맨 위에 나옵니다.
서버 주소 칸에는 IP:포트 외에 온라인 주소(예: xxx.trycloudflare.com)도 넣을 수 있습니다.

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
        shutil.copy2(build.DIST / 'update.ps1', package / 'update.ps1')
        tracked = set(filter(None, subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode().split('\0')))
        patterns = ['fonts/*.ttf', 'fonts/*.txt', 'textures/*.png',
                    'shaders/bin/*.spv', 'personal/library/*.png', 'menu/cat-knock-*.png',
                    'ntl-tags/tags.txt', 'ntl-tags/README.txt', 'ntl-tags/t*.png',
                    'ntl-tags-online/*.txt', 'ntl-tags-online/t*.png']
        for pattern in patterns:
            for source in (ROOT / 'app/res').glob(pattern):
                if source.relative_to(ROOT).as_posix() not in tracked:
                    continue  # Never include a newly imported personal image.
                target = package / source.relative_to(ROOT)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, target)
        # Original built-in face artwork: explicit allowlist, never user imports.
        face_assets = {'app/res/faces/jmou3.png', 'app/res/faces/sest5.png'}
        for relative in sorted(face_assets):
            target = package / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / relative, target)
        shutil.copytree(build.DIST / 'licenses', package / 'licenses')
        for name in ['LICENSE', 'THIRD-PARTY-NOTICES.md']:
            shutil.copy2(ROOT / name, package / name)
        (package / 'README-KO.txt').write_text(README, encoding='utf-8-sig')

        # Corresponding source, without Git history, developer notes, build outputs or credentials.
        # Include current, uncommitted code too: the corresponding source must
        # match this executable, including newly added IME and chat modules.
        added = subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard', '-z',
                                         '--', 'app/src', 'tests', 'tools'], cwd=ROOT).decode().split('\0')
        sources = tracked | face_assets | {name for name in added if name and Path(name).suffix.lower() in {'.c', '.cpp', '.h', '.py'}}
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
