#ifndef TARGET_MARKER_H
#define TARGET_MARKER_H
#include <thermite.h>
void target_marker_reset(void);
bool target_marker_input(tenv* env, bool toggle);
bool target_marker_captured(void);
void target_marker_draw(tenv* env);
#endif
