"""Run after build_windows.py; uses the actual GLFW objects from the client."""
from build_windows import ROOT, BIN, BUILD, run

objects = sorted(BUILD.glob('glfw_src_*.c.o'))
if not objects or not (BUILD / 'glfw_src_input.c.o').exists():
    raise RuntimeError('Build the Windows client first to test its real GLFW timer')
exe = BUILD / 'menu_animation_test.exe'
run([BIN / 'x86_64-w64-mingw32-clang.exe', '-std=gnu11', '-O1', '-static',
     '-I', ROOT / 'glfw/include', ROOT / 'tests/menu_animation_test.c', *objects,
     '-lgdi32', '-lwinmm', '-ldwmapi', '-o', exe])
print(run([exe]))
