"""Run against the actual built Windows GLFW/ImGui objects (build client first)."""
from build_windows import ROOT, BIN, BUILD, run

objects = sorted(BUILD.glob('glfw_src_*.c.o'))
objects += [BUILD / f'app_src_cimgui_imgui_{name}.cpp.o' for name in
            ['imgui', 'imgui_draw', 'imgui_tables', 'imgui_widgets', 'imgui_impl_glfw']]
objects += [BUILD / 'app_src_cimgui_cimgui.cpp.o', BUILD / 'app_src_ui_chat_input.c.o',
            BUILD / 'app_src_ui_text_input_ime.c.o',
            BUILD / 'app_src_cimgui_imgui_imgui_demo.cpp.o']
assert objects and all(p.exists() for p in objects), 'Build the client first'
config = ROOT / 'app/src/cimgui/imgui/imconfig.h'
assert all(p.stat().st_mtime >= config.stat().st_mtime for p in objects
           if 'cimgui' in p.name), 'Rebuild the client after changing ImGui configuration'
test = BUILD / 'korean_input_test.exe'
run([BIN / 'x86_64-w64-mingw32-clang++.exe', '-std=c++17', '-O1', '-static',
     '-I', ROOT / 'glfw/include', ROOT / 'tests/korean_input_test.cpp', *objects,
     '-limm32', '-lgdi32', '-ldwmapi', '-lwinmm', '-o', test])
print(run([test]))
