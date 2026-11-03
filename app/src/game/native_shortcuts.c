#include "native_shortcuts.h"
#include "native_preferences.h"
#include "../user.h"
#include "../network/ntl.h"
#include "../ui/personal_tag.h"
#include "../ui/ntl_tags_ui.h"
#include "death_review.h"
#include "target_marker.h"
#include "input.h"
#include "../ui/screenshot.h"
#include "../ui/toast.h"
#include <stdio.h>
bool native_settings_open;
static native_tap_state taps;
static bool assist_lock;
void native_shortcuts(tenv* env) {
  user_settings* s=&env->usr->usrs;
  s->hotkeys[HOTKEY_RESTART].active=s->hotkeys[HOTKEY_QUIT].active=false;
  bool typing=igGetIO_Nil()->WantTextInput;
  bool in_game=env->usr->gdata.curr_screen==PLAYING;
  // Body hug: while E is held; not while typing.
  body_hug_active=in_game && !typing && twindow_key_down(env->wnd,GLFW_KEY_E);
  if(twindow_key_down(env->wnd,GLFW_KEY_E) && !body_hug_active)
    body_hug_log(6,"E held but %s",!in_game ? "not in a game" : "text input is active (chat or a text box has the keyboard): E types there");
  if(body_hug_active) body_hug_log(7,"E held");
  if(typing) { taps.armed=false; s->hotkeys[HOTKEY_ASSIST].active=assist_lock; return; }
  if(death_review.frozen) { taps.armed=false; return; }
  bool ctrl=twindow_key_down(env->wnd,GLFW_KEY_LEFT_CONTROL)||twindow_key_down(env->wnd,GLFW_KEY_RIGHT_CONTROL);
#ifdef __APPLE__
  // macOS takes control+F2..F8 for itself by default (keyboard focus shortcuts): option works too.
  ctrl=ctrl||twindow_key_down(env->wnd,GLFW_KEY_LEFT_ALT)||twindow_key_down(env->wnd,GLFW_KEY_RIGHT_ALT);
#endif
  bool doubled[GLFW_KEY_LAST+1]={0};
  for(int i=0;i<tdarray_length(env->kb->keys_pressed);i++) {
    int key=env->kb->keys_pressed[i];
    if(key>=0 && key<=GLFW_KEY_LAST) {
      ctrl |= (env->kb->press_modifiers[key]&GLFW_MOD_CONTROL)!=0;
#ifdef __APPLE__
      ctrl |= (env->kb->press_modifiers[key]&GLFW_MOD_ALT)!=0;
#endif
      doubled[key]=native_double_tap(&taps,key,mg_millis());
    }
  }
  if(!native_settings_open && target_marker_input(env,!ctrl && doubled[GLFW_KEY_BACKSPACE])) {
    taps.armed=false;
    return;
  }
  if(ctrl) {
    taps.armed=false;
    if(tkeyboard_key_pressed(env->kb,GLFW_KEY_F2)) native_prefs.gradient_enabled=!native_prefs.gradient_enabled;
    if(tkeyboard_key_pressed(env->kb,GLFW_KEY_F3)) native_prefs.team_name_emphasis=!native_prefs.team_name_emphasis;
    if(tkeyboard_key_pressed(env->kb,GLFW_KEY_F4)) native_prefs.death_freeze=!native_prefs.death_freeze;
    if(tkeyboard_key_pressed(env->kb,GLFW_KEY_F5)) native_prefs.frame_cap=(native_prefs.frame_cap+1)%3;
    if(tkeyboard_key_pressed(env->kb,GLFW_KEY_F6)) native_prefs.render_scale=(native_prefs.render_scale+1)%3;
    if(tkeyboard_key_pressed(env->kb,GLFW_KEY_F7)) ntl_lightweight_settings(s);
    if(tkeyboard_key_pressed(env->kb,GLFW_KEY_F8)) personal_tag_toggle();
    if(tkeyboard_key_pressed(env->kb,GLFW_KEY_F9)) personal_cosmetic_toggle();
    if(tkeyboard_key_pressed(env->kb,GLFW_KEY_F10)) native_prefs.snake_outline=!native_prefs.snake_outline;
    if(tkeyboard_key_pressed(env->kb,GLFW_KEY_F11)) native_prefs.uniform_names=!native_prefs.uniform_names;
    if(tkeyboard_key_pressed(env->kb,GLFW_KEY_F12)) native_prefs.head_point=!native_prefs.head_point;
    return;
  }
  if(tkeyboard_key_pressed(env->kb,GLFW_KEY_Z)) screenshot_request(env);          // NTL: Z
  // NTL: F1 / F2 coil left / right (turning on and on), the same key again stops it.
  if(in_game && (tkeyboard_key_pressed(env->kb,GLFW_KEY_F1) || tkeyboard_key_pressed(env->kb,GLFW_KEY_F2))) {
    if(tkeyboard_key_pressed(env->kb,GLFW_KEY_F1)) native_coil=native_coil>=0 ? -1 : 0;
    else native_coil=native_coil<=0 ? 1 : 0;
  }
  if(tkeyboard_key_pressed(env->kb,GLFW_KEY_F8)||doubled[GLFW_KEY_H]) native_prefs.team_panel=!native_prefs.team_panel;
  if(doubled[GLFW_KEY_PERIOD]) {
    if(env->usr->gdata.curr_screen==PLAYING) native_settings_open=!native_settings_open;
    else if(env->usr->gdata.curr_screen==TITLE_SCREEN) env->usr->gdata.curr_screen=SETTINGS;
    else if(env->usr->gdata.curr_screen==SETTINGS) env->usr->gdata.curr_screen=TITLE_SCREEN;
  }
  if(native_settings_open && tkeyboard_key_pressed(env->kb,GLFW_KEY_ESCAPE)) native_settings_open=false;
  if(native_settings_open || env->usr->gdata.curr_screen==SETTINGS) return;
  if(doubled[GLFW_KEY_3]) {
    gameplay_mode* mode=s->modes+s->hotkeys[HOTKEY_ASSIST].active;
    mode->food_type=(mode->food_type+1)%2;
  }
  if(doubled[GLFW_KEY_8]) s->instant_restart=!s->instant_restart;
  if(doubled[GLFW_KEY_MINUS] || doubled[GLFW_KEY_KP_SUBTRACT]) ntl_clear_chat();   // NTL: - -
  if(doubled[GLFW_KEY_J]) ntl_tags_hidden=!ntl_tags_hidden;                     // J J: tags hidden / shown
  for(int i=0;i<NUM_HOTKEYS;i++) {
    hotkey* hk=&s->hotkeys[i]; if(hk->key<0 || hk->key>GLFW_KEY_LAST) continue;
    if(hk->mode==1) hk->active=twindow_key_down(env->wnd,hk->key);
    else if(hk->mode==2 ? doubled[hk->key] : tkeyboard_key_pressed(env->kb,hk->key)) hk->active=!hk->active;
  }
  if(doubled[GLFW_KEY_B]) native_prefs.bot_smart=!native_prefs.bot_smart;   // NTL: B twice
  bool playing=env->usr->gdata.curr_screen==PLAYING;
  if(playing && doubled[GLFW_KEY_S]) ntl_toggle_sos();                       // NTL: S twice
  if(playing && tkeyboard_key_pressed(env->kb,GLFW_KEY_X)) {                // NTL: X resets the zoom
    env->usr->gdata.data.ms_zoom=native_start_zoom();
    char text[64]; snprintf(text,sizeof(text),"줌 초기화 (시작 줌 %.2f)",native_prefs.start_zoom); toast_show(text,1,1,1,1.5);
  }
  if(doubled[GLFW_KEY_7]) {                                                  // NTL: 7 twice cycles the graphics
    native_prefs.skin_mode=(native_prefs.skin_mode+1)%3;
    static const char* names[]={"그래픽: 기본 스킨","그래픽: 스킨 없음 (단색)","그래픽: 고대비 스킨"};
    toast_show(names[native_prefs.skin_mode],1,1,1,2);
  }
  if(doubled[GLFW_KEY_K]) assist_lock=!assist_lock;
  if(tkeyboard_key_released(env->kb,GLFW_KEY_R)) assist_lock=false;
  s->hotkeys[HOTKEY_ASSIST].active|=assist_lock;
  if(env->usr->gdata.data.score>=1000) s->hotkeys[HOTKEY_RESTART].active=false;
}
