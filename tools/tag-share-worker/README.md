# Slither Native 태그 공유 중계 서버

같은 클라이언트(Slither Native)를 쓰는 사람끼리 비밀번호 태그(내 화면에만 쓰는 것 포함)와 직접 추가한 이미지 태그를
서로 볼 수 있게 해 주는 Cloudflare Worker입니다. 무료 플랜으로 돌아갑니다.

## 배포 (한 번만)

1. Node.js가 있어야 합니다.
2. 이 폴더의 `deploy.cmd`를 더블클릭합니다(Cloudflare 로그인 → 배포). 직접 하려면 이 폴더에서:
   ```
   npx wrangler login
   npx wrangler deploy
   ```
3. 마지막에 나오는 주소(`https://slither-tag-share.<계정>.workers.dev`)를 클라이언트의
   태그 설정 → "태그 공유 서버"에 넣습니다.

## 저장하는 것

- 게임 서버 주소별로, 그 서버의 지렁이 번호와 그 지렁이가 쓰는 태그(12초 동안만, 메모리에만).
- 올라온 이미지 태그 그림(PNG, 120KB 이하, 내용의 해시가 이름).
- 닉네임·계정·IP는 저장하지 않습니다.
