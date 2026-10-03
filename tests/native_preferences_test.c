#include "../app/src/game/native_preferences.h"
#include "../app/src/game/native_shortcuts.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  native_tap_state tap={0};
  assert(!native_double_tap(&tap,'0',100));
  assert(native_double_tap(&tap,'0',1100));
  assert(!native_double_tap(&tap,'0',1150));
  assert(!native_double_tap(&tap,'0',2151));
  assert(!native_double_tap(&tap,'9',2200));
  assert(!native_double_tap(&tap,'0',2300));
  assert(native_double_tap(&tap,'0',2400));
  tap.armed=false;
  assert(!native_double_tap(&tap,'0',2450));
  native_preferences p,q;
  assert(!native_preferences_load("missing.json",&p));
  assert(p.frame_cap==0 && p.render_scale==1 && p.team_panel && !p.fullscreen);
  assert(p.snake_outline && p.head_point && p.death_freeze);
  assert(p.uniform_names && p.name_color[0]==.85f && p.team_name_color[0]==1);
  p.uniform_names=false; p.name_color[0]=.125f;
  p.team_name_color[0]=.25f; p.team_name_color[1]=.75f; p.team_name_color[2]=.5f;
  p.death_freeze=false;
  p.snake_outline=false; p.head_point=false;
  p.frame_cap=2; p.render_scale=0; p.team_panel=false; p.fullscreen=true;
  assert(native_preferences_save("roundtrip.json",&p));
  assert(native_preferences_load("roundtrip.json",&q));
  assert(q.frame_cap==2 && q.render_scale==0 && !q.team_panel && q.fullscreen);
  assert(!q.snake_outline && !q.head_point && !q.death_freeze);
  assert(!q.uniform_names && q.name_color[0]==.125f);
  assert(!memcmp(q.team_name_color,p.team_name_color,sizeof(p.team_name_color)));
  FILE* legacy=fopen("legacy.json","wb");
  fputs("{\"version\":2,\"frame_cap\":1,\"render_scale\":2,\"team_panel\":false,\"fullscreen\":true}",legacy); fclose(legacy);
  assert(native_preferences_load("legacy.json",&q));
  assert(q.snake_outline && q.head_point && q.death_freeze && q.fullscreen && !q.team_panel && q.frame_cap==1 && q.render_scale==2);
  assert(q.uniform_names && q.name_color[0]==.85f && q.team_name_color[0]==1);
  FILE* f=fopen("invalid.json","wb"); fputs("{\"version\":2,\"frame_cap\":99,\"render_scale\":1,\"team_panel\":true,\"fullscreen\":false}",f); fclose(f);
  assert(!native_preferences_load("invalid.json",&q));
  f=fopen("truncated.json","wb"); fputs("{\"version\":2",f); fclose(f);
  assert(!native_preferences_load("truncated.json",&q));
  user_settings settings={0}; user_settings_default(&settings);
  strcpy(settings.nickname,"saved-name"); settings.modes[0].show_shadows=true;
  native_preferences_init(&settings);
  assert(!strcmp(settings.nickname,"saved-name") && settings.modes[0].show_shadows);
  assert(settings.hotkeys[HOTKEY_RESTART].key==GLFW_KEY_0 && settings.hotkeys[HOTKEY_RESTART].mode==2);
  assert(settings.hotkeys[HOTKEY_QUIT].key==GLFW_KEY_9 && settings.hotkeys[HOTKEY_SHOW_NAMES].key==GLFW_KEY_1);
  assert(settings.hotkeys[HOTKEY_ASSIST].key==GLFW_KEY_R && settings.hotkeys[HOTKEY_ASSIST].mode==1);
  for(int i=0;i<NUM_HOTKEYS;i++) for(int j=i+1;j<NUM_HOTKEYS;j++) assert(settings.hotkeys[i].key!=settings.hotkeys[j].key);
  settings.modes[0].show_background=false; settings.modes[0].bg_scale=1.7f;
  settings.hotkeys[HOTKEY_SHOW_NAMES].key=GLFW_KEY_P;
  native_prefs.frame_cap=1; native_prefs.render_scale=2; native_prefs.team_panel=false;
  native_preferences_tick(&settings,true);
  user_settings restored={0}; read_user_settings(&restored); native_preferences_init(&restored);
  assert(!strcmp(restored.nickname,"saved-name"));
  assert(restored.modes[0].show_shadows && !restored.modes[0].show_background && restored.modes[0].bg_scale==1.7f);
  assert(restored.hotkeys[HOTKEY_SHOW_NAMES].key==GLFW_KEY_P);
  assert(native_prefs.frame_cap==1 && native_prefs.render_scale==2 && !native_prefs.team_panel);
  puts("Native settings tests passed: persistence, one-time migration, validation, hotkeys, double-press boundaries.");
}
