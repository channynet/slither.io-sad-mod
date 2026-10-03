#ifndef NATIVE_PREFERENCES_H
#define NATIVE_PREFERENCES_H
#include <stddef.h>
#include "user_settings.h"
typedef struct {
  int version, frame_cap, render_scale;
  bool team_panel, fullscreen;
  bool snake_outline, head_point, death_freeze;
  bool uniform_names;
  float name_color[3], team_name_color[3];
} native_preferences;
extern native_preferences native_prefs;
bool native_preferences_load(const char* path, native_preferences* out);
bool native_preferences_save(const char* path, const native_preferences* value);
void native_preferences_init(user_settings* settings);
void native_preferences_tick(user_settings* settings, bool force);
void ntl_hotkey_defaults(user_settings* settings);
const char* native_key_label(int key, char* buffer, size_t size);
#endif
