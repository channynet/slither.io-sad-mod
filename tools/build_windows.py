"""Local Windows build using pinned llvm-mingw and Slang tools."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import argparse
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / '.tools'
LLVM = next((TOOLS / 'llvm').glob('llvm-mingw-*'))
BIN = LLVM / 'bin'
BUILD = ROOT / 'build' / 'native-windows'
DIST = ROOT / 'dist'

def run(args):
    p = subprocess.run([str(a) for a in args], cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
    if p.returncode:
        raise RuntimeError(f'{args[0]} failed\n{p.stdout}\n{p.stderr}')
    return p.stdout

def shaders():
    out = ROOT / 'app/res/shaders/bin'
    out.mkdir(parents=True, exist_ok=True)
    compiler = TOOLS / 'slang/bin/slangc.exe'
    for src in (ROOT / 'app/res/shaders/src').glob('*.slang'):
        for suffix, stage, entry in [('v', 'vertex', 'vs_main'), ('f', 'fragment', 'fs_main')]:
            dst = out / f'{src.stem}{suffix}.spv'
            if dst.exists() and dst.stat().st_mtime > max(p.stat().st_mtime for p in (ROOT / 'app/res/shaders/src').rglob('*.slang')):
                continue
            run([compiler, src, '-profile', 'spirv_1_0', '-target', 'spirv', '-capability', 'GLSL_330', '-o', dst, '-stage', stage, '-entry', entry, '-emit-spirv-via-glsl'])
    print('Shaders ready', flush=True)

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--exe-name',default='SlitherNative.exe')
    options=parser.parse_args()
    if Path(options.exe_name).name!=options.exe_name or not options.exe_name.lower().endswith('.exe'):
        raise ValueError('exe-name must be an executable file name')
    BUILD.mkdir(parents=True, exist_ok=True); DIST.mkdir(exist_ok=True)
    shaders()
    # Build import names from pinned headers; a CI runner needs no graphics driver.
    imports = BUILD / 'libvulkan-1.a'
    if not imports.exists():
        headers = TOOLS / 'Vulkan-Headers/include/vulkan'
        names = sorted({name for header in headers.glob('*.h')
                        for name in re.findall(r'VKAPI_ATTR[^;{}]*?VKAPI_CALL\s+(vk\w+)\(', header.read_text(encoding='utf-8'))})
        if not names: raise RuntimeError('No Vulkan prototypes found in pinned headers')
        definition = BUILD / 'vulkan-1.def'
        definition.write_text('LIBRARY vulkan-1.dll\nEXPORTS\n' + '\n'.join(names) + '\n')
        run([BIN / 'llvm-dlltool.exe', '-m', 'i386:x86-64', '-d', definition, '-l', imports])
    glfw = ['context', 'init', 'input', 'monitor', 'platform', 'vulkan', 'window', 'egl_context', 'osmesa_context', 'null_init', 'null_monitor', 'null_window', 'null_joystick', 'wgl_context', 'win32_init', 'win32_joystick', 'win32_module', 'win32_thread', 'win32_time', 'win32_window', 'win32_monitor']
    sources = [ROOT / f'glfw/src/{name}.c' for name in glfw]
    for folder in ['thermite/src', 'app/src']:
        sources += list((ROOT / folder).rglob('*.c')) + list((ROOT / folder).rglob('*.cpp'))
    common = ['-O2', '-g0', '-D_GLFW_WIN32', '-D_WIN32_WINNT=0x0A00', '-DGLFW_INCLUDE_NONE', '-DGLFW_INCLUDE_VULKAN', '-DCGLM_FORCE_DEPTH_ZERO_TO_ONE', '-DCIMGUI_NO_EXPORT', '-DAPP_VERSION="2.6-ntl0.92"', '-DSETTINGS_VERSION="1.3"']
    # Keep local filesystem paths out of distributed assertion/source strings.
    common += [f'-ffile-prefix-map={ROOT}=.', f'-ffile-prefix-map={ROOT.as_posix()}=.']
    for path in ['glfw/include', 'thermite/include', 'thermite/src', '.tools/Vulkan-Headers/include']:
        common += ['-I', str(ROOT / path)]
    headers = [p for folder in ['thermite/src','app/src','thermite/include'] for p in (ROOT / folder).rglob('*.h')]
    header_time = max(p.stat().st_mtime for p in headers)
    def compile(src):
        obj = BUILD / (str(src.relative_to(ROOT)).replace('\\','_').replace('/','_') + '.o')
        if obj.exists() and obj.stat().st_mtime > max(src.stat().st_mtime, header_time, Path(__file__).stat().st_mtime):
            return obj
        cpp = src.suffix == '.cpp'
        flags = [f for f in common if src.relative_to(ROOT).parts[0] != 'glfw' or f not in ['-DGLFW_INCLUDE_NONE', '-DGLFW_INCLUDE_VULKAN']]
        run([BIN / ('x86_64-w64-mingw32-clang++.exe' if cpp else 'x86_64-w64-mingw32-clang.exe'), '-std=c++17' if cpp else '-std=gnu11', *flags, '-c', src, '-o', obj])
        print('Built ' + str(src.relative_to(ROOT)), flush=True)
        return obj
    with ThreadPoolExecutor(max_workers=2) as pool:
        objects = list(pool.map(compile, sources))
    args = [*objects, imports, '-static', '-mwindows', '-lwinhttp', '-lbcrypt', '-lcrypt32', '-lcomdlg32', '-lgdi32', '-limm32', '-lws2_32', '-lwinmm', '-ldwmapi', '-liphlpapi', '-lwlanapi', '-lole32', '-lwindowscodecs', '-luuid', '-o', DIST / options.exe_name]
    response = BUILD / 'link.rsp'
    response.write_text('\n'.join('"' + str(arg).replace('\\','/') + '"' for arg in args), encoding='utf-8')
    run([BIN / 'x86_64-w64-mingw32-clang++.exe', '@' + str(response)])
    shutil.copytree(ROOT / 'app/res', DIST / 'app/res', dirs_exist_ok=True)
    shutil.copy2(ROOT / 'LICENSE', DIST / 'LICENSE')
    shutil.copy2(ROOT / 'tools/update.ps1', DIST / 'update.ps1')
    for source in ['glfw/LICENSE.md','app/src/cimgui/LICENSE','app/src/cimgui/imgui/LICENSE.txt','app/src/external/LICENSE.txt','thermite/src/external/vma/LICENSE.txt','thermite/src/external/stb/LICENSE.txt','thermite/include/cglm/LICENSE.txt']:
        target = DIST / 'licenses' / source
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / source, target)
    for name in ['README-NATIVE.md', 'THIRD-PARTY-NOTICES.md']:
        if (ROOT / name).exists(): shutil.copy2(ROOT / name, DIST / name)
    print('Ready: ' + str(DIST / options.exe_name), flush=True)

if __name__ == '__main__': main()
