# Third-party notices

This is a local modification of Vlither 2.6, based on commit `8612bc12186547fc385ea38a399b144973338384`, https://github.com/for-loop9/vlither. The upstream project declares GPL-3.0; its LICENSE and existing notices are retained. The changes in this project cover the Windows build, NTL interoperability, rendering scale, frame pacing, input capture and diagnostic UI.

The black feathered wings in `app/res/personal/black-wings.png` were generated for this project with the built-in image generation tool on 2026-10-02. The prompt is recorded in `BLACK-WINGS.txt`. This example is not copied from NTL. Custom image cosmetics and image tags are rendered only on the local client.

The source tree includes these components and their original license texts:

| Component | Original notice |
| --- | --- |
| GLFW | glfw/LICENSE.md |
| cimgui | app/src/cimgui/LICENSE |
| Dear ImGui | app/src/cimgui/imgui/LICENSE.txt |
| Mongoose | app/src/external/LICENSE.txt and mongoose.h |
| Vulkan Memory Allocator | thermite/src/external/vma/LICENSE.txt |
| stb | thermite/src/external/stb/LICENSE.txt |
| cglm | thermite/include/cglm/LICENSE.txt |
| Bundled fonts | app/res/fonts/LICENSE.txt |

Mongoose identifies its terms as GPL-2.0-only or commercial. These are retained as found in upstream; this local build does not change or resolve upstream licensing terms. Distribution bundles include the corresponding source snapshot and retained license texts.

Windows Malgun Gothic is read from the user's existing Windows installation for Korean text. It is not copied or redistributed. Build tools llvm-mingw and Slang are local build dependencies, not part of the application package. Vulkan-Headers are pinned to `3c65a01745e4a1134d32b9c2c456472212dba16d`.

Game artwork, trademarks and game content remain the property of their respective owners. This application is not affiliated with or endorsed by Slither.io or NTL. The NTL interoperability adapter contains no NTL game source or artwork. It sends team requests only after the user selects Connect with their own entered or locally restored credentials. Credentials are stored using Windows current-user DPAPI. Personal image tags are selected by the user and rendered locally; they are not included in game or team requests.

Original face artwork: `app/res/faces/jmou3.png` and `sest5.png` are the original Slither.io skin 40 mouth and skin 41 eye stalks. Retrieved from the public NTL 9.68 extension archive (`https://ntl-slither.com/ntlmod-pub.zip`, `s/jmou3.webp` and `s/sest5.webp`) on 2026-10-05 and converted to PNG without changing RGBA pixels. The original game source references these as jmou3 and sest5. These game assets remain the property of their respective owners; no additional license is asserted. The smile eye path and layout use the original game drawing geometry.

The body-follow join improvement was informed by inspecting NTL 9.68's older-body selection and forward tangent behavior. The native planner remains this project's bounded C implementation; no NTL bot source is included.

From 2.6-ntl0.48 the bot (`app/src/game/ntl_bot_core.c`, `ntl_bot_core.h`) is a C port of the bot in the NTL mod, which is itself based on "Slither.io Bot Championship Edition" (https://github.com/j-c-m/Slither.io-bot, Mozilla Public License 2.0). Those two files are provided under the Mozilla Public License 2.0 (http://mozilla.org/MPL/2.0/); their source is in this project and must stay available to anyone the program is given to. This replaces the earlier statement that no NTL bot code is included.
