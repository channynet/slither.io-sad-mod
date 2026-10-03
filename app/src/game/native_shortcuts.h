#ifndef NATIVE_SHORTCUTS_H
#define NATIVE_SHORTCUTS_H
#include <stdbool.h>
#include <stdint.h>
typedef struct { int last_key; uint64_t last_time; bool armed; } native_tap_state;
// NTL default: two consecutive presses within one second. Repeats are not presses.
static inline bool native_double_tap(native_tap_state* state,int key,uint64_t now) {
  if(state->armed && state->last_key==key && now>=state->last_time && now-state->last_time<=1000) {
    state->armed=false; return true;
  }
  state->last_key=key; state->last_time=now; state->armed=true; return false;
}
#include <thermite.h>
void native_shortcuts(tenv* env);
extern bool native_settings_open;
#endif
