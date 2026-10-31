#ifndef TARGET_MARKER_H
#define TARGET_MARKER_H
#include <thermite.h>
void target_marker_reset(void);
bool target_marker_input(tenv* env, bool toggle);
bool target_marker_captured(void);
void target_marker_draw(tenv* env);
void target_marker_receive(tenv* env,const char* wire);
void target_marker_report(tenv* env,char* out,size_t capacity);
void target_marker_clear_team(void);
#endif
