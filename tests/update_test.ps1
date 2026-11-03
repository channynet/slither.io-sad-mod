$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../tools/update.ps1"
function Assert($Value, [string]$Message) { if (!$Value) { throw $Message } }
function Reject([scriptblock]$Action) {
    $rejected = $false
    try { & $Action } catch { $rejected = $true }
    Assert $rejected 'Expected rejection'
}
$temp = Join-Path ([IO.Path]::GetTempPath()) ('slither-update-test-'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($temp) | Out-Null
try {
    $release = [pscustomobject]@{tag_name='v2.6-ntl0.91';draft=$false;prerelease=$false;assets=@(
        [pscustomobject]@{name='SlitherNative-2.6-ntl0.91-Windows-x64.zip';size=100;digest=('sha256:'+('a'*64));browser_download_url='https://github.com/channynet/slither.io-sad-mod/releases/download/v2.6-ntl0.91/SlitherNative-2.6-ntl0.91-Windows-x64.zip'})}
    Assert ($null -ne (Get-UpdateAsset $release '2.6-ntl0.90')) 'Missing update'
    Assert ($null -eq (Get-UpdateAsset $release '2.6-ntl0.91')) 'Equal version update'
    Assert ($null -ne (Get-UpdateAsset $release '2.6-ntl0.91' -Repair)) 'Missing component repair rejected'
    Assert ($null -eq (Get-UpdateAsset $release '2.6-ntl0.100' -Repair)) 'Repair must not downgrade'
    Assert ($null -eq (Get-UpdateAsset $release '2.6-ntl0.100')) 'Downgrade'
    $release.prerelease=$true
    Assert ($null -eq (Get-UpdateAsset $release '2.6-ntl0.90')) 'Preview release accepted'
    $release.prerelease=$false
    $release.assets[0].browser_download_url='https://example.org/unknown.zip'
    Reject { Get-UpdateAsset $release '2.6-ntl0.90' }
    Reject { Get-SafePath $temp '../outside' }
    Reject { Get-SafePath $temp 'C:/outside' }
    Reject { Get-SafePath $temp 'file:alternate' }

    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive=Join-Path $temp 'package.zip'
    $zip=[IO.Compression.ZipFile]::Open($archive,[IO.Compression.ZipArchiveMode]::Create)
    foreach ($relative in @('SlitherNative.exe','update.ps1','app/res/shaders/bin/test.spv','source/private.txt','user.dat')) {
        $entry=$zip.CreateEntry('SlitherNative-2.6-ntl0.91-Windows-x64/'+$relative)
        $writer=New-Object IO.StreamWriter($entry.Open());$writer.Write('new '+$relative);$writer.Dispose()
    }
    $zip.Dispose()
    $stage=Join-Path $temp 'stage'
    $hash=(Get-FileHash -LiteralPath $archive).Hash
    Reject { Expand-VerifiedPackage $archive ('0'*64) $stage '2.6-ntl0.91' }
    $files=@(Expand-VerifiedPackage $archive $hash $stage '2.6-ntl0.91')
    Assert ($files.Count -eq 3) 'Personal data or source included in installation'
    $root=Join-Path $temp 'install with spaces'
    [IO.Directory]::CreateDirectory((Join-Path $root '.update')) | Out-Null
    [IO.File]::WriteAllText((Join-Path $root 'SlitherNative.exe'),'old exe')
    [IO.File]::WriteAllText((Join-Path $root 'user.dat'),'my settings')
    Install-Update $root $stage $files
    Assert ((Get-Content -Raw -LiteralPath (Join-Path $root 'SlitherNative.exe')) -eq 'new SlitherNative.exe') 'EXE replacement failed'
    Assert ((Get-Content -Raw -LiteralPath (Join-Path $root 'user.dat')) -eq 'my settings') 'Settings changed'
    Assert ((Get-Content -Raw -LiteralPath (Join-Path $root '.update/backup/SlitherNative.exe')) -eq 'old exe') 'Missing backup'
    Assert (!(Test-Path -LiteralPath (Join-Path $root '.update/transaction.json'))) 'Uncommitted transaction'

    # A failure after replacing the EXE must restore the entire prior installation.
    [IO.File]::WriteAllText((Join-Path $stage 'SlitherNative.exe'),'broken update')
    Reject { Install-Update $root $stage @('SlitherNative.exe','update.ps1') } # second source was already moved
    Assert ((Get-Content -Raw -LiteralPath (Join-Path $root 'SlitherNative.exe')) -eq 'new SlitherNative.exe') 'Rollback failed'
    Assert (!(Test-Path -LiteralPath (Join-Path $root '.update/transaction.json'))) 'Rollback journal remains'

    # Recover a journal left by a process crash, including files newly added by the update.
    [IO.File]::WriteAllText((Join-Path $root 'SlitherNative.exe'),'partial update')
    [IO.File]::WriteAllText((Join-Path $root 'README-KO.txt'),'new unwanted file')
    $journal=@([pscustomobject]@{path='SlitherNative.exe';existed=$true},[pscustomobject]@{path='README-KO.txt';existed=$false})
    [IO.File]::WriteAllText((Join-Path $root '.update/transaction.json'),(ConvertTo-Json -InputObject $journal))
    Restore-Update $root
    Assert ((Get-Content -Raw -LiteralPath (Join-Path $root 'SlitherNative.exe')) -eq 'new SlitherNative.exe') 'Crash recovery failed'
    Assert (!(Test-Path -LiteralPath (Join-Path $root 'README-KO.txt'))) 'Added file not rolled back'

    # Archive traversal is rejected even if the archive digest is valid.
    $bad=Join-Path $temp 'bad.zip';$zip=[IO.Compression.ZipFile]::Open($bad,[IO.Compression.ZipArchiveMode]::Create)
    $null=$zip.CreateEntry('SlitherNative-2.6-ntl0.91-Windows-x64/../outside');$zip.Dispose()
    Reject { Expand-VerifiedPackage $bad (Get-FileHash -LiteralPath $bad).Hash (Join-Path $temp 'bad-stage') '2.6-ntl0.91' }
    'Updater passed: versions, release origin, digest, path checks, runtime allowlist, replacement, rollback, crash recovery, settings preservation.'
} finally {
    $resolved=[IO.Path]::GetFullPath($temp)
    if ($resolved.StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()),[StringComparison]::OrdinalIgnoreCase) -and (Split-Path $resolved -Leaf) -like 'slither-update-test-*') {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
