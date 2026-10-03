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

Mongoose identifies its terms as GPL-2.0-only or commercial. These are retained as found in upstream; this local build does not change or resolve upstream licensing terms. No public binary publication is part of this task.

Windows Malgun Gothic is read from the user's existing Windows installation for Korean text. It is not copied or redistributed. Build tools llvm-mingw and Slang are local build dependencies, not part of the application package. Vulkan-Headers are pinned to `3c65a01745e4a1134d32b9c2c456472212dba16d`.

Game artwork, trademarks and game content remain the property of their respective owners. This application is not affiliated with or endorsed by Slither.io or NTL. The NTL interoperability adapter contains no NTL game source or artwork. It sends team requests only after the user selects Connect with their own entered or locally restored credentials. Credentials are stored using Windows current-user DPAPI. Personal image tags are selected by the user and rendered locally; they are not included in game or team requests.
