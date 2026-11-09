@echo off
chcp 65001 >nul
rem Slither Native tag sharing relay: deploys this folder to your Cloudflare account (free plan).
rem (From a copy in %TEMP%, so wrangler does not pick up the settings of a project this folder sits in.)
set "WORK=%TEMP%\slither-tag-share"
if exist "%WORK%" rmdir /s /q "%WORK%"
mkdir "%WORK%\src"
copy /y "%~dp0wrangler.toml" "%WORK%\" >nul
copy /y "%~dp0src\*.js" "%WORK%\src\" >nul
cd /d "%WORK%"
echo Cloudflare에 로그인되어 있지 않으면 로그인 창이 열립니다.
call npx --yes wrangler@4 deploy --config "%WORK%\wrangler.toml"
echo.
echo 위에 나온 https://....workers.dev 주소를 클라이언트 태그 설정의 "태그 공유 서버"에 넣으세요.
pause
