param(
    [switch]$VerifyOnly,
    [string]$ServiceUrl,
    [ValidateRange(1, 60)][int]$Attempts = 24
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
Push-Location -LiteralPath $PSScriptRoot
try {
    $expectedHash = (& node --input-type=module -e "import {adminPage} from './src/admin.js'; import {createHash} from 'node:crypto'; console.log(createHash('sha256').update(adminPage).digest('hex'));").Trim()
    if ($LASTEXITCODE -ne 0 -or $expectedHash -notmatch '^[a-f0-9]{64}$') { throw 'Cannot calculate administrator page hash.' }
    if (!$VerifyOnly) {
        & node --test tests/access.test.mjs
        if ($LASTEXITCODE -ne 0) { throw 'Management server tests failed; deployment cancelled.' }
        # A unique staging directory avoids inherited project configuration and preserves prior runs.
        $stage = Join-Path $env:TEMP ('slither-tag-deploy-' + [guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path (Join-Path $stage 'src') | Out-Null
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'wrangler.toml') -Destination $stage
        Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'src') -Filter '*.js' | Copy-Item -Destination (Join-Path $stage 'src')
        Push-Location -LiteralPath $stage
        try {
            # Windows PowerShell treats native stderr warnings as ErrorRecords.
            # Wrangler's exit code, not the presence of stderr, determines upload failure.
            $savedPreference = $ErrorActionPreference
            $ErrorActionPreference = 'Continue'
            try {
                $output = @(& npx.cmd --yes wrangler@4.159.0 deploy --durable-objects-code-update-mode immediate --config (Join-Path $stage 'wrangler.toml') 2>&1 | ForEach-Object {
                    Write-Host $_
                    $_.ToString()
                })
                $deployExitCode = $LASTEXITCODE
            } finally { $ErrorActionPreference = $savedPreference }
            if ($deployExitCode -ne 0) { throw 'Cloudflare upload failed.' }
        } finally { Pop-Location }
        $addresses = [regex]::Matches(($output -join "`n"), 'https://[a-z0-9-]+\.[a-z0-9-]+\.workers\.dev')
        if ($addresses.Count -eq 0) { throw 'Cannot identify the deployed service address.' }
        $deployedUrl = $addresses[$addresses.Count - 1].Value
        if ($ServiceUrl -and $ServiceUrl.TrimEnd('/') -ne $deployedUrl) { throw 'Requested verification address differs from deployed address.' }
        $ServiceUrl = $deployedUrl
    }
    $url = $null
    if (![uri]::TryCreate($ServiceUrl, [UriKind]::Absolute, [ref]$url) -or $url.Scheme -ne 'https' -or
        $url.UserInfo -or $url.Query -or $url.Fragment -or $url.AbsolutePath -ne '/') {
        throw 'Provide an HTTPS service origin using -ServiceUrl.'
    }
    $ServiceUrl = $ServiceUrl.TrimEnd('/')
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        for ($attempt = 1; $attempt -le $Attempts; $attempt++) {
            try {
                $nonce = [guid]::NewGuid().ToString('N')
                $page = Invoke-WebRequest -UseBasicParsing -Uri "$ServiceUrl/admin?verify=$nonce" -Headers @{'Cache-Control'='no-cache'} -TimeoutSec 15
                $actualHash = ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($page.Content)))).Replace('-', '').ToLowerInvariant()
                if ($actualHash -ne $expectedHash) { throw 'The live administrator page differs from this source.' }
                $health = Invoke-RestMethod -Uri "$ServiceUrl/v1/access?verify=$nonce" -Headers @{'Cache-Control'='no-cache'} -TimeoutSec 15
                if ($health.schema -ne 2) { throw 'The live access storage implementation is outdated.' }
                Write-Host "Verified deployment: $ServiceUrl (administrator page matches; access schema 2)."
                exit 0
            } catch {
                Write-Host "Verification $attempt/$Attempts failed: $($_.Exception.Message)"
                if ($attempt -eq $Attempts) { throw 'Upload may have succeeded, but live verification failed. Deployment is NOT confirmed.' }
                Start-Sleep -Seconds 5
            }
        }
    } finally { $sha.Dispose() }
} finally { Pop-Location }
