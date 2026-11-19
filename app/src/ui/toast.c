#include "toast.h"

// Central transient messages are disabled; actions and panel errors are unchanged.
void toast_show(const char* text, float r, float g, float b, double seconds) {
  (void)text; (void)r; (void)g; (void)b; (void)seconds;
}
void toast_draw(tenv* env) { (void)env; }
