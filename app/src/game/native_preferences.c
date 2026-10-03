#include <stddef.h>
#include "native_preferences.h"
#include "../external/mongoose.h"
#include <GLFW/glfw3.h>
#include <windows.h>
#include <stdio.h>
#include <math.h>

native_preferences native_prefs;
static native_preferences saved_prefs;
static user_settings saved_settings;
static ULONGLONG last_check;
static const char* prefs_path="native-settings.json";

static bool integer(struct mg_str json,const char* key,int low,int high,int* target) {
  double n;
  if(!mg_json_get_num(json,key,&n) || !isfinite(n) || floor(n)!=n || n<low || n>high) return false;
  *target=(int)n; return true;
}
bool native_preferences_load(const char* path,native_preferences* out) {
  *out=(native_preferences){.version=2,.frame_cap=0,.render_scale=1,.team_panel=true,.fullscreen=false,.snake_outline=true,.head_point=true,.death_freeze=true};
  FILE* f=fopen(path,"rb"); if(!f) return false;
  char data[2048]; size_t n=fread(data,1,sizeof(data)-1,f); bool overflow=!feof(f); fclose(f);
  if(overflow) return false; data[n]=0;
  struct mg_str json=mg_str_n(data,n); native_preferences value=*out;
  if(!integer(json,"$.version",2,2,&value.version) || !integer(json,"$.frame_cap",0,2,&value.frame_cap) ||
     !integer(json,"$.render_scale",0,2,&value.render_scale) || !mg_json_get_bool(json,"$.team_panel",&value.team_panel) ||
     !mg_json_get_bool(json,"$.fullscreen",&value.fullscreen)) return false;
  // Optional fields preserve old version-2 preferences and custom hotkeys.
  mg_json_get_bool(json,"$.snake_outline",&value.snake_outline);
  mg_json_get_bool(json,"$.head_point",&value.head_point);
  mg_json_get_bool(json,"$.death_freeze",&value.death_freeze);
  *out=value; return true;
}
bool native_preferences_save(const char* path,const native_preferences* value) {
  char temp[MAX_PATH]; if(snprintf(temp,sizeof(temp),"%s.tmp",path)>=(int)sizeof(temp)) return false;
  FILE* f=fopen(temp,"wb"); if(!f) return false;
  int count=fprintf(f,"{\n  \"version\": 2,\n  \"frame_cap\": %d,\n  \"render_scale\": %d,\n  \"team_panel\": %s,\n  \"fullscreen\": %s,\n  \"snake_outline\": %s,\n  \"head_point\": %s,\n  \"death_freeze\": %s\n}\n",value->frame_cap,value->render_scale,value->team_panel?"true":"false",value->fullscreen?"true":"false",value->snake_outline?"true":"false",value->head_point?"true":"false",value->death_freeze?"true":"false");
  bool ok=count>0; if(fclose(f)) ok=false;
  return ok && MoveFileExA(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
void ntl_hotkey_defaults(user_settings* s) {
  s->hotkeys[HOTKEY_HUD]=(hotkey){GLFW_KEY_V,true,2,"HUD (double)"};
  s->hotkeys[HOTKEY_SHOW_NAMES]=(hotkey){GLFW_KEY_1,true,2,"Names (double)"};
  s->hotkeys[HOTKEY_BIG_FOOD]=(hotkey){GLFW_KEY_F6,false,0,"Big food only"};
  s->hotkeys[HOTKEY_ASSIST]=(hotkey){GLFW_KEY_R,false,1,"Assist (K K locks)"};
  s->hotkeys[HOTKEY_BOT]=(hotkey){GLFW_KEY_T,false,2,"Bot (double)"};
  s->hotkeys[HOTKEY_MENU]=(hotkey){GLFW_KEY_F9,true,0,"Hotkey guide"};
  s->hotkeys[HOTKEY_RESTART]=(hotkey){GLFW_KEY_0,false,2,"Restart (double, <1000)"};
  s->hotkeys[HOTKEY_QUIT]=(hotkey){GLFW_KEY_9,false,2,"Quit (double)"};
}
static user_settings normalized(const user_settings* settings) {
  user_settings s=*settings;
  s.score=0; s.kills=0; s.play_time=0;
  s.hotkeys[HOTKEY_BOT].active=false;
  s.hotkeys[HOTKEY_ASSIST].active=false;
  s.hotkeys[HOTKEY_RESTART].active=false;
  s.hotkeys[HOTKEY_QUIT].active=false;
  return s;
}
void native_preferences_init(user_settings* settings) {
  bool loaded=native_preferences_load(prefs_path,&native_prefs);
  if(!loaded) {
    // Migrate the first native version once; keep nickname, skin and other settings.
    ntl_hotkey_defaults(settings);
    for(int i=0;i<2;i++) { settings->modes[i].show_background=true; settings->modes[i].bg_scale=1; }
    save_user_settings(settings);
    native_preferences_save(prefs_path,&native_prefs);
  }
  settings->hotkeys[HOTKEY_BOT].active=settings->hotkeys[HOTKEY_ASSIST].active=false;
  settings->hotkeys[HOTKEY_RESTART].active=settings->hotkeys[HOTKEY_QUIT].active=false;
  saved_settings=normalized(settings); saved_prefs=native_prefs;
}
void native_preferences_tick(user_settings* settings,bool force) {
  ULONGLONG now=GetTickCount64(); if(!force && now-last_check<750) return;
  last_check=now; user_settings current=normalized(settings);
  if(force || memcmp(&saved_settings,&current,sizeof(current))) { save_user_settings(settings); saved_settings=current; }
  if(force || memcmp(&saved_prefs,&native_prefs,sizeof(native_prefs))) {
    if(native_preferences_save(prefs_path,&native_prefs)) saved_prefs=native_prefs;
  }
}
const char* native_key_label(int key,char* buffer,size_t size) {
  if(key>=GLFW_KEY_F1 && key<=GLFW_KEY_F25) snprintf(buffer,size,"F%d",key-GLFW_KEY_F1+1);
  else if(key>=32 && key<=126) snprintf(buffer,size,"%c",key);
  else snprintf(buffer,size,"?");
  return buffer;
}
