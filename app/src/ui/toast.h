#ifndef UI_TOAST_H
#define UI_TOAST_H
// Short notices at the top centre of the screen (screenshot saved, SOS, zoom), a few at a time.
#include <thermite.h>
void toast_show(const char* text, float r, float g, float b, double seconds);
void toast_draw(tenv* env);
#endif
