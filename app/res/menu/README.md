# Menu cat animation

`slp-cat-knock.gif` is the original user-provided animation.
`cat-knock-0.png` through `cat-knock-3.png` are its four composited RGBA frames, decoded with Pillow (`seek(i); convert("RGBA").save(...)`). They preserve the original pixels and alpha, without generated or repainted content. All four original frame delays are 200 ms.

The GIF already has a transparent background. The client uses these PNG frames because its stb animated GIF decoder left an opaque white border/background on this particular file. PNG avoids that decoder artifact and keeps the existing animation. All textures are uploaded once on startup and are only drawn on the title screen.

Playback uses GLFW's raw monotonic timer (`glfwGetTimerValue` / `glfwGetTimerFrequency`), independent of the resettable game clock. Resetting that clock on Play can otherwise stall ImGui time and freeze the cat after returning to the menu. The original four 200 ms poses repeat every 800 ms, including after reconnects. After building, run `python tools/test_menu_animation.py` for the real GLFW timer reset regression check.
