#!/bin/bash
# Slither Native - 맥 빌드: 더블클릭하면 이 폴더의 소스로 SlitherNative.app과 배포용 ZIP을 만듭니다.
cd "$(dirname "$0")" || exit 1
clear
echo "Slither Native - macOS 빌드"
echo "=========================="
if ! xcode-select -p >/dev/null 2>&1 || ! xcrun --find clang >/dev/null 2>&1; then
  echo
  echo "Apple 명령어 도구(Command Line Tools)가 필요합니다 (무료, 한 번만)."
  echo "곧 뜨는 창에서 '설치'를 누르고, 설치가 끝나면 이 파일을 다시 더블클릭하세요."
  xcode-select --install 2>/dev/null
  echo
  read -n1 -r -p "아무 키나 누르면 닫힙니다." _
  exit 1
fi
echo "빌드 중입니다. 처음에는 2~5분 걸립니다..."
echo
if /usr/bin/python3 source/tools/build_unix.py --out "$PWD/dist" --obj "$PWD/source/build/mac"; then
  echo
  echo "완료! dist 폴더에 있습니다."
  echo "  SlitherNative.app                          ← 바로 실행"
  echo "  SlitherNative-*-macOS-Universal.zip        ← 다른 맥에 나눠 줄 배포용"
  open dist
else
  echo
  echo "빌드에 실패했습니다. 이 창의 내용을 복사해서 보내 주세요."
fi
echo
read -n1 -r -p "아무 키나 누르면 닫힙니다." _
