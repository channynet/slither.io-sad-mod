#ifndef PLATFORM_MAC_DIALOG_H
#define PLATFORM_MAC_DIALOG_H
#include <stdbool.h>
#include <stddef.h>
// macOS open panel, run inside the app (so it shows over the game, also in full screen, and the
// chosen file may be read without a separate privacy prompt). UTF-8 path; false if cancelled.
bool mac_choose_image(const char* message, char* path, size_t capacity);
#endif
