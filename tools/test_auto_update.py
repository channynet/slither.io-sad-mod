"""Exercise the real Windows launcher/worker handoff in an isolated installation."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import tempfile
import time
import zipfile
from build_windows import ROOT, BIN, run

with tempfile.TemporaryDirectory(prefix='slither-update-') as temporary:
    base = Path(temporary) / 'install with spaces \ud14c\uc2a4\ud2b8'
    base.mkdir()
    source = Path(temporary) / 'fixture.c'
    source.write_text('''#include <stdio.h>
#include "auto_update.h"
int main(void) { auto_update_startup(); FILE* f=fopen("started.txt","w");
if(!f) return 1; fputs(APP_VERSION,f); fclose(f); return 0; }
''')
    for version, name in [('2.6-ntl0.90', 'SlitherNative.exe'), ('2.6-ntl0.91', 'new.exe')]:
        run([BIN/'x86_64-w64-mingw32-clang.exe', '-static', '-mwindows',
             f'-DAPP_VERSION="{version}"', '-I'+str(ROOT/'app/src/platform'), source,
             ROOT/'app/src/platform/auto_update.c', '-o', base/name])
    core = (ROOT/'tools/update.ps1').read_text(encoding='utf-8')
    (base/'update-core.ps1').write_text(core, encoding='utf-8')
    (base/'user.dat').write_bytes(b'private settings')
    package = base/'fixture.zip'
    prefix = 'SlitherNative-2.6-ntl0.91-Windows-x64/'
    with zipfile.ZipFile(package,'w') as archive:
        archive.write(base/'new.exe', prefix+'SlitherNative.exe')
        archive.writestr(prefix+'update.ps1', core)
        archive.writestr(prefix+'app/res/shaders/bin/test.spv', b'new resource')
    release = {'tag_name':'v2.6-ntl0.91','prerelease':False,'draft':False,'assets':[{
        'name':'SlitherNative-2.6-ntl0.91-Windows-x64.zip', 'size':package.stat().st_size,
        'digest':'sha256:'+hashlib.sha256(package.read_bytes()).hexdigest(),
        'browser_download_url':'https://github.com/channynet/slither.io-sad-mod/releases/download/v2.6-ntl0.91/SlitherNative-2.6-ntl0.91-Windows-x64.zip'}]}
    (base/'release.json').write_text(json.dumps(release))
    parameters = 'param([string]$CurrentVersion,[int]$ParentId,[string]$ReadyEvent,[switch]$Repair,[switch]$SkipCheck)\n'
    mocks = '''. "$PSScriptRoot/update-core.ps1" -CurrentVersion $CurrentVersion -ParentId $ParentId -ReadyEvent $ReadyEvent -Repair:$Repair -SkipCheck:$SkipCheck
function Invoke-RestMethod { Get-Content -Raw -LiteralPath "$PSScriptRoot/release.json" | ConvertFrom-Json }
function Invoke-WebRequest { param($Uri,$OutFile,$TimeoutSec,[switch]$UseBasicParsing)
Copy-Item -LiteralPath "$PSScriptRoot/fixture.zip" -Destination $OutFile }
Invoke-SlitherUpdate
'''
    env = os.environ.copy()
    env.pop('SLITHER_SKIP_UPDATE_ONCE', None)
    # Emulate a launcher inheriting PowerShell Core or custom module paths.
    modules = base/'incompatible-modules'
    utility = modules/'Microsoft.PowerShell.Utility'
    utility.mkdir(parents=True)
    (utility/'Microsoft.PowerShell.Utility.psd1').write_text("@{RootModule='bad.psm1';ModuleVersion='99.0';PowerShellVersion='99.0';FunctionsToExport=@('Get-FileHash');CmdletsToExport=@()}")
    (utility/'bad.psm1').write_text("function Get-FileHash { throw 'Incompatible inherited module' }")
    windows_modules = Path(os.environ['WINDIR'])/'System32/WindowsPowerShell/v1.0/Modules'
    env['PSModulePath'] = str(modules)+';'+str(windows_modules)
    # A failed check must return to the original application.
    (base/'update.ps1').write_text(parameters+'''. "$PSScriptRoot/update-core.ps1" -CurrentVersion $CurrentVersion -ParentId $ParentId -ReadyEvent $ReadyEvent -Repair:$Repair -SkipCheck:$SkipCheck
function Invoke-RestMethod { throw 'simulated offline' }
Invoke-SlitherUpdate
''')
    subprocess.run([base/'SlitherNative.exe'], cwd=base, env=env, check=True, timeout=20)
    assert (base/'started.txt').read_text() == '2.6-ntl0.90'
    assert 'simulated offline' in (base/'.update/status.txt').read_text()
    (base/'started.txt').unlink()
    legacy_worker = base/'app/res/snake-missile/worker.exe'
    legacy_worker.parent.mkdir(parents=True)
    legacy_worker.write_bytes(b'old engine fixture')
    # The old parent exits, the real worker replaces its EXE, and the new EXE starts once.
    (base/'update.ps1').write_text(parameters+mocks)
    subprocess.run([base/'SlitherNative.exe'], cwd=base, env=env, check=True, timeout=20)
    deadline = time.monotonic()+20
    while not (base/'started.txt').exists() and time.monotonic()<deadline:
        time.sleep(.1)
    assert (base/'started.txt').read_text() == '2.6-ntl0.91', (base/'.update/status.txt').read_text()
    assert (base/'user.dat').read_bytes() == b'private settings'
    assert (base/'app/res/shaders/bin/test.spv').read_bytes() == b'new resource'
    assert not legacy_worker.exists()
    assert (base/'.update/retired/app/res/snake-missile/worker.exe').read_bytes() == b'old engine fixture'
    # A same-version check must not reinstall the removed engine.
    time.sleep(.5)
    (base/'started.txt').unlink()
    (base/'update.ps1').write_text(parameters+mocks)
    subprocess.run([base/'SlitherNative.exe'], cwd=base, env=env, check=True, timeout=20)
    assert (base/'started.txt').read_text() == '2.6-ntl0.91', (base/'.update/status.txt').read_text()
    assert 'Already up to date' in (base/'.update/status.txt').read_text()
    assert not legacy_worker.exists()
    # The restart performed by the 0.97 updater skips the next network check,
    # but must still retire its engine using the newly installed script.
    legacy_worker.write_bytes(b'old engine fixture')
    (base/'started.txt').unlink()
    env['SLITHER_SKIP_UPDATE_ONCE'] = '1'
    subprocess.run([base/'SlitherNative.exe'], cwd=base, env=env, check=True, timeout=20)
    assert (base/'started.txt').read_text() == '2.6-ntl0.91'
    assert not legacy_worker.exists()
    # Give the worker/new application time to release their process image handles.
    time.sleep(.5)
    print('Updater handoff passed: offline fallback, spaces/Unicode paths, parent exit, EXE/resource replacement, automatic restart, retired engine backup, no engine repair, preserved settings.')
