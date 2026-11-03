param([string]$CurrentVersion, [int]$ParentId, [string]$ReadyEvent, [switch]$CheckOnly)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
# A Windows PowerShell child may inherit PowerShell Core's incompatible module path.
# Resolve built-in cmdlets from this interpreter before any inherited modules.
$env:PSModulePath = "$PSHOME\Modules;$env:PSModulePath"
$Repository = 'channynet/slither.io-sad-mod'

function Get-VersionNumber([string]$Value) {
    if ($Value -notmatch '^v?(\d+)\.(\d+)-ntl(\d+)\.(\d+)$') { throw 'Unsupported version' }
    return [version]("{0}.{1}.{2}.{3}" -f $Matches[1],$Matches[2],$Matches[3],$Matches[4])
}
function Get-UpdateAsset($Release, [string]$InstalledVersion) {
    if ($Release.draft -or $Release.prerelease) { return $null }
    if ((Get-VersionNumber $Release.tag_name) -le (Get-VersionNumber $InstalledVersion)) { return $null }
    $version = $Release.tag_name -replace '^v',''
    $name = "SlitherNative-$version-Windows-x64.zip"
    $assets = @($Release.assets | Where-Object { $_.name -ceq $name })
    if ($assets.Count -ne 1) { throw 'Release must have exactly one Windows package' }
    $asset = $assets[0]
    $expected = "https://github.com/$Repository/releases/download/$($Release.tag_name)/$name"
    if ($asset.browser_download_url -cne $expected -or $asset.digest -notmatch '^sha256:[a-fA-F0-9]{64}$') {
        throw 'Invalid release URL or missing SHA-256 digest'
    }
    if ($asset.size -le 0 -or $asset.size -gt 536870912) { throw 'Invalid package size' }
    return $asset
}
function Get-SafePath([string]$Root, [string]$Relative) {
    if (!$Relative -or $Relative.Contains('\') -or $Relative -match '(^/|:|(^|/)\.\.?(/|$))') { throw 'Unsafe path' }
    $base = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    $path = [IO.Path]::GetFullPath((Join-Path $base $Relative))
    if (!$path.StartsWith($base+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Path outside installation' }
    $cursor = $path
    while ($cursor.Length -ge $base.Length) {
        if ((Test-Path -LiteralPath $cursor) -and ((Get-Item -Force -LiteralPath $cursor).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'Links are not allowed in update paths'
        }
        $cursor = [IO.Path]::GetDirectoryName($cursor)
    }
    return $path
}
function Test-RuntimePath([string]$Path) {
    return $Path -cmatch '^(SlitherNative\.exe|update\.ps1|LICENSE|THIRD-PARTY-NOTICES\.md|README-KO\.txt|licenses/[a-zA-Z0-9_./-]+|app/res/(fonts|textures|shaders/bin|personal/library|menu|ntl-tags|ntl-tags-online|faces)/[a-zA-Z0-9_.-]+)$'
}
function Expand-VerifiedPackage([string]$Archive, [string]$Digest, [string]$Stage, [string]$Version) {
    if ((Get-FileHash -LiteralPath $Archive -Algorithm SHA256).Hash -ine $Digest) { throw 'Package hash mismatch' }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead($Archive)
    $prefix = "SlitherNative-$Version-Windows-x64/"
    $seen = @{}; $files = @(); [long]$total = 0
    try {
        foreach ($entry in $zip.Entries) {
            if (!$entry.FullName.StartsWith($prefix,[StringComparison]::Ordinal)) { throw 'Wrong package root' }
            $relative = $entry.FullName.Substring($prefix.Length)
            if (!$relative -or $relative.EndsWith('/')) { continue }
            $target = Get-SafePath $Stage $relative
            if ($seen.ContainsKey($relative)) { throw 'Duplicate archive path' }
            $seen[$relative] = $true
            $total += $entry.Length
            if ($total -gt 1073741824) { throw 'Expanded package is too large' }
            if (!(Test-RuntimePath $relative)) { continue } # source and checksum files are not installed
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$false)
            $files += $relative
        }
        if (!('SlitherNative.exe' -cin $files) -or !('update.ps1' -cin $files)) { throw 'Incomplete runtime package' }
        return $files
    } finally { $zip.Dispose() }
}
function Restore-Update([string]$Root) {
    $journal = Get-SafePath $Root '.update/transaction.json'
    if (!(Test-Path -LiteralPath $journal)) { return }
    $items = ConvertFrom-Json -InputObject (Get-Content -Raw -LiteralPath $journal)
    foreach ($item in $items) {
        if (!(Test-RuntimePath $item.path)) { throw 'Invalid recovery path' }
        $dest = Get-SafePath $Root $item.path
        if ($item.existed) {
            $backup = Get-SafePath $Root ('.update/backup/'+$item.path)
            if ((Test-Path -LiteralPath $dest) -and (Get-FileHash -LiteralPath $dest).Hash -eq (Get-FileHash -LiteralPath $backup).Hash) { continue }
            Copy-Item -LiteralPath $backup -Destination $dest -Force
        } elseif (Test-Path -LiteralPath $dest) { Remove-Item -LiteralPath $dest -Force }
    }
    Remove-Item -LiteralPath $journal
}
function Install-Update([string]$Root, [string]$Stage, [string[]]$Files) {
    $journal = Get-SafePath $Root '.update/transaction.json'
    if (Test-Path -LiteralPath $journal) { throw 'Recovery required before installation' }
    $items = @()
    # Back up every destination before changing any installed file.
    foreach ($relative in $Files) {
        if (!(Test-RuntimePath $relative)) { throw 'Invalid installation path' }
        $dest = Get-SafePath $Root $relative
        $backup = Get-SafePath $Root ('.update/backup/'+$relative)
        $existed = Test-Path -LiteralPath $dest -PathType Leaf
        if ($existed) {
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($backup)) | Out-Null
            Copy-Item -LiteralPath $dest -Destination $backup -Force
        }
        $items += [pscustomobject]@{path=$relative;existed=$existed}
    }
    [IO.File]::WriteAllText($journal,(ConvertTo-Json -InputObject $items))
    try {
        foreach ($relative in $Files) {
            $source = Get-SafePath $Stage $relative
            $dest = Get-SafePath $Root $relative
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($dest)) | Out-Null
            if (Test-Path -LiteralPath $dest) { [IO.File]::Replace($source,$dest,[NullString]::Value) }
            else { [IO.File]::Move($source,$dest) }
        }
        Remove-Item -LiteralPath $journal
    } catch { Restore-Update $Root; throw }
}
function Invoke-SlitherUpdate {
    $root = $PSScriptRoot
    $state = Get-SafePath $root '.update/state'
    [IO.Directory]::CreateDirectory($state) | Out-Null
    $lock = $null; $handoff = $false; $recovered = $false
    $log = Get-SafePath $root '.update/status.txt'
    try {
        $lock = [IO.File]::Open((Get-SafePath $root '.update/lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        $journal = Get-SafePath $root '.update/transaction.json'
        if (!(Test-Path -LiteralPath $journal)) {
            $release = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repository/releases/latest" -Headers @{'User-Agent'='SlitherNative-Updater';Accept='application/vnd.github+json'} -TimeoutSec 8
            $asset = Get-UpdateAsset $release $CurrentVersion
            if ($CheckOnly) {
                [pscustomobject]@{installed=$CurrentVersion;latest=$release.tag_name;updateAvailable=($null -ne $asset)} | ConvertTo-Json -Compress
                return
            }
            if (!$asset) { [IO.File]::WriteAllText($log,'Already up to date: '+$CurrentVersion); return }
            $version = $release.tag_name -replace '^v',''
            $archive = Get-SafePath $root '.update/package.zip'
            Invoke-WebRequest -UseBasicParsing -Uri $asset.browser_download_url -OutFile $archive -TimeoutSec 120
            if ((Get-Item -LiteralPath $archive).Length -ne $asset.size) { throw 'Incomplete download' }
            $stage = Get-SafePath $root ('.update/stage-'+[guid]::NewGuid().ToString('N'))
            $files = @(Expand-VerifiedPackage $archive $asset.digest.Substring(7) $stage $version)
        } elseif ($CheckOnly) { throw 'Interrupted update requires recovery' }
        else { $recovered = $true }
        # Capture the parent process handle before signalling. Never replace a running EXE.
        $parent = [Diagnostics.Process]::GetProcessById($ParentId)
        $null = $parent.Handle
        $ready = [Threading.EventWaitHandle]::OpenExisting($ReadyEvent)
        try { $null = $ready.Set() } finally { $ready.Dispose() }
        if (!$parent.WaitForExit(15000)) { throw 'Application did not exit; installation cancelled' }
        $handoff = $true
        if ($recovered) { Restore-Update $root }
        else { Install-Update $root $stage $files }
        [IO.File]::WriteAllText($log, $(if ($recovered) {'Interrupted update restored'} else {'Installed '+$version}))
    } catch {
        [IO.File]::WriteAllText($log,('Update failed; keeping the installed version. '+$_.Exception.Message))
    } finally {
        if ($stage -and !(Test-Path -LiteralPath (Get-SafePath $root '.update/transaction.json'))) {
            # Only the unique staging directory created by this invocation may be removed.
            $expectedPrefix = (Get-SafePath $root '.update/stage-')
            if ($stage.StartsWith($expectedPrefix,[StringComparison]::OrdinalIgnoreCase) -and
                (Split-Path $stage -Leaf) -match '^stage-[a-f0-9]{32}$') {
                Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
            }
        }
        if ($archive -and $lock) { Remove-Item -LiteralPath $archive -Force -ErrorAction SilentlyContinue }
        if ($lock) { $lock.Dispose() }
        if ($handoff) {
            # Recovery failure must not launch a potentially mixed installation.
            if (!(Test-Path -LiteralPath (Get-SafePath $root '.update/transaction.json'))) {
                $env:SLITHER_SKIP_UPDATE_ONCE = '1'
                Start-Process -FilePath (Join-Path $root 'SlitherNative.exe') -WorkingDirectory $root
            } else {
                Add-Type -AssemblyName System.Windows.Forms
                [Windows.Forms.MessageBox]::Show('Update recovery could not finish. Close other copies of SlitherNative and run it again. Details: .update/status.txt','SlitherNative update') | Out-Null
            }
        }
    }
}
if ($MyInvocation.InvocationName -ne '.') { Invoke-SlitherUpdate }
