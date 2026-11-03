#ifndef NATIVE_PREFERENCES_H
#define NATIVE_PREFERENCES_H
#include <stddef.h>
#include "user_settings.h"
typedef struct {
  int version, frame_cap, render_scale;
  bool team_panel, team_status, fullscreen;
  bool snake_outline, head_point, death_freeze;
  bool uniform_names;
  bool team_name_emphasis;
  bool face_smile, face_claws;
  bool wifi_low_latency;
  bool path_retry;
  bool bot_smart;      // NTL "Smart bot": grow first, then circle. Off: circle at once.
  int bot_length;      // NTL "bot length": the length at which growing turns into circling
  int missile_count;   // Total Snake Missile connections, 1..100 (not per proxy).
  bool gradient_enabled;
  int gradient_count;
  float gradient_colors[4][3];
  float name_color[3], team_name_color[3];
  bool safe_wall;      // NTL "Safewall": near the wall, steer along it instead of into it
  int safe_wall_script;  // NTL "Safewall script": 1 linear (mouse), 2 speed/angle/width (default)
  int safe_wall_offset;  // NTL "Border assist offset" -10..10: more = further from the wall (script 2)
  bool safe_wall_tight;  // use safe_wall_distance instead of NTL's offset (default on)
  float safe_wall_distance; // 0..20: head centre this far inside the wall (1 = flush)
  bool fire_skin;      // burning body (my screen only)
  float fire_strength; // 0.2..1.5: flame size and glow
  bool net_stats;      // FPS and ping always shown at the bottom of the screen
  float start_zoom;    // NTL "Start zoom": zoom at the start of every game (1 = normal), 0.2..3
  bool wall_semaphore; // NTL "Dynamic border semaphore": wall red shrinking, orange still, green growing
  bool glass_border;   // NTL "Glass Border": a faint wall line
  bool sos_chat;       // NTL "Send chat alert when SOS": turning SOS on also sends "Help me!"
  int skin_mode;       // NTL "Graphics": 0 skins, 1 skinless (one plain colour each), 2 high visibility
  bool keep_own_skin;  // the graphics mode leaves my own snake as it is
  float others_opacity;// other snakes' opacity, 0.15..1
  float hug_gap;       // body hug (E): how far the nose keeps outside the enemy's hitbox, 0..30
} native_preferences;
extern native_preferences native_prefs;
bool native_preferences_load(const char* path, native_preferences* out);
bool native_preferences_save(const char* path, const native_preferences* value);
void native_preferences_init(user_settings* settings);
void native_preferences_tick(user_settings* settings, bool force);
void ntl_hotkey_defaults(user_settings* settings);
const char* native_key_label(int key, char* buffer, size_t size);
float native_start_zoom(void);   // the game's zoom value for native_prefs.start_zoom
#endif
