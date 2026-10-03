from build_windows import ROOT, BIN, BUILD, run
import subprocess
import tempfile
BUILD.mkdir(parents=True, exist_ok=True)
test = BUILD / 'ntl_protocol_test.exe'
run([BIN / 'x86_64-w64-mingw32-clang.exe', '-std=gnu11', '-O1', '-static',
     ROOT / 'tests/ntl_protocol_test.c', ROOT / 'app/src/network/ntl_protocol.c',
     ROOT / 'app/src/external/mongoose.c', '-lws2_32', '-o', test])
print(run([test]))
prefs_test = BUILD / 'native_preferences_test.exe'
flags = ['-DGLFW_INCLUDE_NONE','-DGLFW_INCLUDE_VULKAN','-DSETTINGS_VERSION="1.3"']
for folder in ['glfw/include','thermite/include','thermite/src','.tools/Vulkan-Headers/include']:
    flags += ['-I',str(ROOT / folder)]
run([BIN / 'x86_64-w64-mingw32-clang.exe','-std=gnu11','-O1','-static',*flags,
     ROOT / 'tests/native_preferences_test.c', ROOT / 'app/src/game/native_preferences.c',
     ROOT / 'app/src/game/user_settings.c', ROOT / 'app/src/external/mongoose.c',
     '-lws2_32','-o',prefs_test])
with tempfile.TemporaryDirectory(prefix='preferences-test-', dir=BUILD) as sandbox:
    result = subprocess.run([str(prefs_test)],cwd=sandbox,check=True,capture_output=True,text=True,encoding="utf-8",errors="replace")
    print(result.stdout)

for name, sources, extra in [
    ('bot_steering', ['app/src/game/input.c'], [*flags,'-DCIMGUI_NO_EXPORT','-ffunction-sections','-fdata-sections','-Wl,--gc-sections']),
    ('sbot_integration', ['app/src/game/sbot.c','app/src/game/defensive_planner.c','app/src/game/body_loop.c','thermite/src/util/tdarray.c'], [*flags,'-DCIMGUI_NO_EXPORT']),
    ('sbot_live_loop', ['app/src/game/sbot.c','app/src/game/defensive_planner.c','app/src/game/body_loop.c','thermite/src/util/tdarray.c'], [*flags,'-DCIMGUI_NO_EXPORT']),
    ('defensive_planner', ['app/src/game/defensive_planner.c','app/src/game/body_loop.c'], []),
    ('credential_store', ['app/src/network/credential_store.c'], ['-lcrypt32']),
    ('cosmetic_geometry', [], []),
    ('body_loop', ['app/src/game/body_loop.c','app/src/game/defensive_planner.c'], []),
    ('tag_attachment', [], []),
    ('image_library', ['app/src/ui/image_library.c','app/src/external/mongoose.c'], ['-lws2_32']),
    ('head_point_geometry', [], []),
    ('input_timing', [], []),
    ('server_status', ['app/src/network/server_status.c','app/src/network/server_history.c','app/src/external/mongoose.c'], ['-lws2_32']),
    ('server_history', ['app/src/network/server_history.c'], []),
    ('team_minimap', [], []),
    ('gameplay_review', ['thermite/src/util/tdarray.c'], [*flags,'-DCIMGUI_NO_EXPORT','-ffunction-sections','-fdata-sections','-Wl,--gc-sections']),
    ('network_pump', ['app/src/external/mongoose.c'], ['-lws2_32']),
    ('present_policy', [], ['-I',str(ROOT / '.tools/Vulkan-Headers/include')]),
    ('frame_acquire', ['thermite/src/graphics/tframe_acquire.c'], [*flags,'-ffunction-sections','-fdata-sections','-Wl,--gc-sections']),
    ('perf_trace', ['app/src/game/perf_trace.c'], []),
    ('minimap_update', [], ['-O2']),
    ('team_membership', [], []),
    ('name_layout', [], []),
    ('minimap_upload', ['app/src/rendering/mm_renderer.c'], [*flags,'-ffunction-sections','-fdata-sections','-Wl,--gc-sections']),
    ('window_refresh', ['thermite/src/framework/twindow.c'], [*flags,'-ffunction-sections','-fdata-sections','-Wl,--gc-sections']),
    ('tag_image', ['app/src/ui/tag_image.c','thermite/src/external/stb/stb_image.c'], ['-I',str(ROOT / 'thermite/src')]),
]:
    executable = BUILD / f'{name}_test.exe'
    run([BIN / 'x86_64-w64-mingw32-clang.exe','-std=gnu11','-O1','-static',
         ROOT / f'tests/{name}_test.c',*[ROOT / s for s in sources],*extra,'-o',executable])
    with tempfile.TemporaryDirectory(prefix=f'{name}-test-',dir=BUILD) as sandbox:
        args = [str(executable)]
        if name == 'tag_image': args.append(str(ROOT / 'app/res/personal/black-wings.png'))
        result = subprocess.run(args,cwd=sandbox,check=True,capture_output=True,text=True,encoding="utf-8",errors="replace")
        print(result.stdout)
