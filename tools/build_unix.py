#!/usr/bin/env python3
"""Builds Slither Native for macOS (a Universal SlitherNative.app for Apple Silicon and Intel Macs)
or for Linux (used to test the non-Windows code).

  python3 tools/build_unix.py            # on a Mac: dist/SlitherNative.app and the release ZIP
  python3 tools/build_unix.py --no-zip   # just the app

Needs only the compiler of the platform: on a Mac the Command Line Tools (xcode-select --install),
which also bring Python 3 and libcurl. MoltenVK, the Vulkan driver for Metal, comes with the
source in third_party/moltenvk. Runs on the Python 3.9 that macOS provides."""
import argparse, os, platform, plistlib, re, shutil, subprocess, sys, zipfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MAC = sys.platform == 'darwin'
VERSION = re.search(r'APP_VERSION="([^"]+)"', (ROOT / 'tools/build_windows.py').read_text(encoding='utf-8')).group(1)
MIN_MACOS = '11.0'          # Big Sur: every Mac from 2020 on runs it or newer

ap = argparse.ArgumentParser()
ap.add_argument('--out', default=str(ROOT / 'dist'))
ap.add_argument('--obj', default=str(ROOT / 'build' / ('mac' if MAC else 'linux')))
ap.add_argument('--arch', default='arm64,x86_64' if MAC else '', help='Mac architectures (default: both, Universal)')
ap.add_argument('--no-zip', action='store_true')
ap.add_argument('--jobs', type=int, default=os.cpu_count() or 4)
args = ap.parse_args()
OUT, OBJ = Path(args.out), Path(args.obj)
OUT.mkdir(parents=True, exist_ok=True); OBJ.mkdir(parents=True, exist_ok=True)

def first_dir(*cands):
    for c in cands:
        if (ROOT / c).is_dir(): return ROOT / c
    raise SystemExit('missing: ' + ' or '.join(cands))

VULKAN_INCLUDE = first_dir('third_party/vulkan/include', '.tools/Vulkan-Headers/include')
RES = first_dir('app/res')
MOLTENVK = ROOT / 'third_party/moltenvk/libMoltenVK.dylib'

glfw_common = ['context', 'init', 'input', 'monitor', 'platform', 'vulkan', 'window', 'egl_context', 'osmesa_context',
               'null_init', 'null_monitor', 'null_window', 'null_joystick']
if MAC:
    glfw = glfw_common + ['cocoa_init.m', 'cocoa_joystick.m', 'cocoa_monitor.m', 'cocoa_window.m', 'cocoa_time',
                          'nsgl_context.m', 'posix_module', 'posix_thread']
    platform_define = '-D_GLFW_COCOA'
else:
    glfw = glfw_common + ['x11_init', 'x11_monitor', 'x11_window', 'xkb_unicode', 'glx_context', 'linux_joystick',
                          'posix_module', 'posix_poll', 'posix_thread', 'posix_time']
    platform_define = '-D_GLFW_X11'
sources = [ROOT / 'glfw/src' / (n if '.' in n else n + '.c') for n in glfw]
for folder in ['thermite/src', 'app/src']:
    sources += sorted((ROOT / folder).rglob('*.c')) + sorted((ROOT / folder).rglob('*.cpp'))

common = ['-O2', '-g0', platform_define, '-DGLFW_INCLUDE_NONE', '-DGLFW_INCLUDE_VULKAN', '-DCGLM_FORCE_DEPTH_ZERO_TO_ONE',
          '-DCIMGUI_NO_EXPORT', f'-DAPP_VERSION="{VERSION}"', '-DSETTINGS_VERSION="1.3"', f'-ffile-prefix-map={ROOT}=.',
          '-Wno-deprecated-declarations']
for p in ['glfw/include', 'thermite/include', 'thermite/src']: common += ['-I', str(ROOT / p)]
common += ['-I', str(VULKAN_INCLUDE)]
arch = []
if MAC:
    for a in filter(None, args.arch.split(',')): arch += ['-arch', a]
    arch += [f'-mmacosx-version-min={MIN_MACOS}']
CC = os.environ.get('CC', 'clang' if MAC or shutil.which('clang') else 'cc')
CXX = os.environ.get('CXX', 'clang++' if MAC or shutil.which('clang++') else 'c++')

headers = [p for f in ['thermite/src', 'app/src', 'thermite/include', 'glfw/src', 'glfw/include'] for p in (ROOT / f).rglob('*.h')]
newest_header = max(p.stat().st_mtime for p in headers)
stamp = ' '.join(common + arch + [CC, CXX])
stamp_file = OBJ / 'flags.txt'
if not stamp_file.exists() or stamp_file.read_text() != stamp:     # other flags: rebuild everything
    for o in OBJ.glob('*.o'): o.unlink()
    stamp_file.write_text(stamp)

def compile_one(src):
    rel = src.relative_to(ROOT)
    obj = OBJ / (str(rel).replace('/', '_') + '.o')
    if obj.exists() and obj.stat().st_mtime > max(src.stat().st_mtime, newest_header): return obj
    flags = [f for f in common if rel.parts[0] != 'glfw' or f not in ('-DGLFW_INCLUDE_NONE', '-DGLFW_INCLUDE_VULKAN')]
    if src.suffix == '.cpp': cmd = [CXX, '-std=c++17', *flags]
    elif src.suffix == '.m': cmd = [CC, '-x', 'objective-c', *flags]
    else: cmd = [CC, '-std=gnu11', *flags]
    p = subprocess.run(cmd + arch + ['-c', str(src), '-o', str(obj)], capture_output=True, text=True)
    if p.returncode:
        print('FAILED', rel, '\n', p.stderr[-4000:]); raise SystemExit(1)
    noisy = any(x in str(rel) for x in ('cimgui', 'external', 'glfw'))
    if p.stderr.strip() and not noisy: print('warning', rel, '\n', p.stderr[:2000])
    return obj

print(f'Slither Native {VERSION}: compiling {len(sources)} files ({"macOS " + args.arch if MAC else "Linux"})...', flush=True)
with ThreadPoolExecutor(max_workers=args.jobs) as pool: objects = list(pool.map(compile_one, sources))

exe = OBJ / 'SlitherNative'
if MAC:
    if not MOLTENVK.exists(): raise SystemExit(f'missing {MOLTENVK}')
    link = [CXX, *arch, *map(str, objects), str(MOLTENVK), '-framework', 'Cocoa', '-framework', 'IOKit',
            '-framework', 'CoreFoundation', '-framework', 'QuartzCore', '-framework', 'Metal', '-lcurl',
            '-Wl,-rpath,@executable_path/../Frameworks', '-o', str(exe)]
else:
    vulkan = next((str(c) for c in [Path('/usr/lib/x86_64-linux-gnu/libvulkan.so.1'), Path('/usr/lib/aarch64-linux-gnu/libvulkan.so.1'),
                                     Path('/usr/lib/libvulkan.so.1')] if c.exists()), '-lvulkan')
    link = [CXX, *map(str, objects), vulkan, '-lcurl', '-lpthread', '-ldl', '-lm', '-o', str(exe)]
p = subprocess.run(link, capture_output=True, text=True)
if p.returncode: print('LINK FAILED\n', p.stderr[-4000:]); raise SystemExit(1)

def copy_resources(target):
    if target.exists(): shutil.rmtree(target)
    skip = ['*.glsl', '*.vert', '*.frag', '.DS_Store', 'README*', 'background_4k.png']   # (not used by the program)
    if (RES / 'textures/tex_atlas_4k.png').exists():
        skip.append('tex_atlas_8k.png')            # the half-size atlas is loaded when present
    shutil.copytree(RES, target / 'res', ignore=shutil.ignore_patterns(*skip))

if not MAC:
    folder = OUT / 'SlitherNative-linux'
    folder.mkdir(parents=True, exist_ok=True)
    shutil.copy2(exe, folder / 'SlitherNative')
    copy_resources(folder / 'app')
    print('built', folder / 'SlitherNative')
    raise SystemExit(0)

# SlitherNative.app
app = OUT / 'SlitherNative.app'
if app.exists(): shutil.rmtree(app)
contents = app / 'Contents'
(contents / 'MacOS').mkdir(parents=True); (contents / 'Frameworks').mkdir(); (contents / 'Resources').mkdir()
shutil.copy2(exe, contents / 'MacOS/SlitherNative')
shutil.copy2(MOLTENVK, contents / 'Frameworks/libMoltenVK.dylib')
copy_resources(contents / 'Resources/app')
icon = ROOT / 'tools/mac/SlitherNative.icns'
if icon.exists(): shutil.copy2(icon, contents / 'Resources/SlitherNative.icns')
short = re.sub(r'[^0-9.]', '', VERSION.split('-')[0]) or '1.0'
with open(contents / 'Info.plist', 'wb') as f:
    plistlib.dump({
        'CFBundleName': 'Slither Native', 'CFBundleDisplayName': 'Slither Native',
        'CFBundleIdentifier': 'io.github.slithernative.client', 'CFBundleExecutable': 'SlitherNative',
        'CFBundlePackageType': 'APPL', 'CFBundleShortVersionString': short, 'CFBundleVersion': VERSION,
        'CFBundleIconFile': 'SlitherNative.icns', 'LSMinimumSystemVersion': MIN_MACOS,
        'NSHighResolutionCapable': True, 'LSApplicationCategoryType': 'public.app-category.games',
        'NSHumanReadableCopyright': 'Slither Native - see LICENSE', 'CFBundleDevelopmentRegion': 'ko',
    }, f)
(contents / 'PkgInfo').write_text('APPL????')
# Apple Silicon runs only signed code: an ad-hoc signature (no Apple account needed).
subprocess.run(['codesign', '--force', '--sign', '-', str(contents / 'Frameworks/libMoltenVK.dylib')], check=True)
subprocess.run(['codesign', '--force', '--sign', '-', '--timestamp=none', str(app)], check=True)
subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)], check=True)
print('built', app)
if args.no_zip: raise SystemExit(0)

name = f'SlitherNative-{VERSION}-macOS-Universal'
stage = OBJ / name
if stage.exists(): shutil.rmtree(stage)
stage.mkdir()
# ditto keeps the bundle exactly (permissions, signature); cp -R would too, shutil may not.
subprocess.run(['ditto', str(app), str(stage / 'SlitherNative.app')], check=True)
for extra in ['README-MAC-KO.txt', 'LICENSE', 'THIRD-PARTY-NOTICES.md']:
    src = ROOT / extra if (ROOT / extra).exists() else ROOT / 'tools/mac' / extra
    if src.exists(): shutil.copy2(src, stage / extra)
if (ROOT / 'licenses').is_dir(): shutil.copytree(ROOT / 'licenses', stage / 'licenses')
archive = OUT / f'{name}.zip'
if archive.exists(): archive.unlink()
subprocess.run(['ditto', '-c', '-k', '--sequesterRsrc', '--keepParent', str(stage), str(archive)], check=True)
print('release', archive, archive.stat().st_size, 'bytes')
