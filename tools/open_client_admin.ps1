$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Security
$keyPath = Join-Path $env:LOCALAPPDATA 'SlitherNativeManagement\admin-key.dpapi'
if (-not (Test-Path -LiteralPath $keyPath)) {
    throw '이 Windows 계정에 저장된 관리자 키가 없습니다. 관리 서버의 ADMIN_TOKEN 설정을 확인하세요.'
}
$encrypted = [IO.File]::ReadAllBytes($keyPath)
$plain = [Security.Cryptography.ProtectedData]::Unprotect($encrypted, $null, [Security.Cryptography.DataProtectionScope]::CurrentUser)
try {
    $key = [Text.Encoding]::UTF8.GetString($plain)
    Set-Clipboard -Value $key
    Start-Process 'https://slither-tag-share.sadxxx.workers.dev/admin'
    Write-Host '관리자 키를 클립보드에 복사했습니다.'
    Write-Host '브라우저의 관리자 키 입력칸에 붙여넣고 접속을 누르세요.'
} finally {
    [Array]::Clear($plain, 0, $plain.Length)
    $key = $null
}
