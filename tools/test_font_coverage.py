"""Check the production font setup with the actual ImGui glyph rasterizer."""
from build_windows import ROOT, BIN, BUILD, run

flags = ['-DGLFW_INCLUDE_NONE', '-DGLFW_INCLUDE_VULKAN', '-DCIMGUI_NO_EXPORT']
for folder in ['glfw/include', 'thermite/include', 'thermite/src', '.tools/Vulkan-Headers/include']:
    flags += ['-I', str(ROOT / folder)]
obj = BUILD / 'font_coverage_test.o'
run([BIN / 'x86_64-w64-mingw32-clang.exe', '-std=gnu11', '-O1',
     '-ffunction-sections', '-fdata-sections', *flags,
     '-c', ROOT / 'tests/font_coverage_test.c', '-o', obj])
objects = [BUILD / f'app_src_cimgui_imgui_{name}.cpp.o' for name in
           ['imgui', 'imgui_draw', 'imgui_tables', 'imgui_widgets', 'imgui_demo']]
objects += [BUILD / 'app_src_cimgui_cimgui.cpp.o',
            BUILD / 'app_src_cimgui_cimgui_impl.cpp.o',
            BUILD / 'app_src_cimgui_imgui_imgui_impl_glfw.cpp.o',
            BUILD / 'app_src_cimgui_imgui_imgui_impl_vulkan.cpp.o',
            BUILD / 'app_src_ui_text_input_ime.c.o']
objects += sorted(BUILD.glob('glfw_src_*.c.o'))
exe = BUILD / 'font_coverage_test.exe'
run([BIN / 'x86_64-w64-mingw32-clang++.exe', '-static', '-Wl,--gc-sections',
     obj, *objects, BUILD / 'libvulkan-1.a', '-limm32', '-lgdi32', '-ldwmapi', '-lwinmm', '-o', exe])
print(run([exe]))
