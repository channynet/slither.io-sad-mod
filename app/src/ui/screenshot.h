#ifndef UI_SCREENSHOT_H
#define UI_SCREENSHOT_H
// Z: the next frame as shown (game and UI) is saved as a PNG in the "screenshots" folder.
#include <thermite.h>
#include <stddef.h>
void screenshot_request(tenv* env);
void screenshot_frame(tenv* env);       // once a frame, before rendering: hands a captured frame to the saver
// "screenshots/slither-YYYYMMDD-HHMMSS.png" (with -2, -3 ... if that name is taken in the same second)
void screenshot_name(char* out, size_t capacity, int year, int month, int day, int hour, int minute, int second, int serial);
#endif
