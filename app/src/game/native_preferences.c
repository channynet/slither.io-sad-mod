#include <stddef.h>
#include "native_preferences.h"
#include "../external/mongoose.h"
#include <GLFW/glfw3.h>
#include "../platform/compat.h"
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
  *out=(native_preferences){.version=2,.frame_cap=0,.render_scale=1,.team_panel=true,.team_status=true,.fullscreen=false,.snake_outline=true,.head_point=true,.death_freeze=true,.uniform_names=true,.team_name_emphasis=true,.wifi_low_latency=true,.bot_smart=true,.bot_length=2000,.missile_count=2,.gradient_count=2,.gradient_colors={{.05f,.85f,1},{.65f,.15f,1},{1,.75f,.1f},{1,.2f,.35f}},.name_color={.85f,.85f,.85f},.team_name_color={1,.8f,.2f},.fire_strength=1,.safe_wall=true,.safe_wall_script=2,.safe_wall_offset=0,.safe_wall_tight=true,.safe_wall_distance=1,.net_stats=true,.start_zoom=1,.wall_semaphore=true,.glass_border=false,.sos_chat=true,.skin_mode=0,.keep_own_skin=true,.others_opacity=1,.hug_gap=1};
  FILE* f=fopen(path,"rb"); if(!f) return false;
  char data[8192]; size_t n=fread(data,1,sizeof(data)-1,f); bool overflow=!feof(f); fclose(f);
  if(overflow) return false; data[n]=0;
  struct mg_str json=mg_str_n(data,n); native_preferences value=*out;
  if(!integer(json,"$.version",2,2,&value.version) || !integer(json,"$.frame_cap",0,2,&value.frame_cap) ||
     !integer(json,"$.render_scale",0,2,&value.render_scale) || !mg_json_get_bool(json,"$.team_panel",&value.team_panel) ||
     !mg_json_get_bool(json,"$.fullscreen",&value.fullscreen)) return false;
  mg_json_get_bool(json,"$.team_status",&value.team_status);
  // Optional fields preserve old version-2 preferences and custom hotkeys.
  mg_json_get_bool(json,"$.snake_outline",&value.snake_outline);
  mg_json_get_bool(json,"$.head_point",&value.head_point);
  mg_json_get_bool(json,"$.death_freeze",&value.death_freeze);
  mg_json_get_bool(json,"$.uniform_names",&value.uniform_names);
  mg_json_get_bool(json,"$.team_name_emphasis",&value.team_name_emphasis);
  mg_json_get_bool(json,"$.face_smile",&value.face_smile);
  mg_json_get_bool(json,"$.face_claws",&value.face_claws);
  mg_json_get_bool(json,"$.wifi_low_latency",&value.wifi_low_latency);
  mg_json_get_bool(json,"$.path_retry",&value.path_retry);
  integer(json,"$.missile_count",1,100,&value.missile_count);
  mg_json_get_bool(json,"$.bot_smart",&value.bot_smart);
  { double length; if(mg_json_get_num(json,"$.bot_length",&length) && length>=100 && length<=50000) value.bot_length=(int)length; }
  const char* colors[]={"$.name_r","$.name_g","$.name_b","$.team_name_r","$.team_name_g","$.team_name_b"};
  for(int i=0;i<6;i++) {
    double component;
    if(mg_json_get_num(json,colors[i],&component) && isfinite(component) && component>=0 && component<=1) {
      if(i<3) value.name_color[i]=(float)component;
      else value.team_name_color[i-3]=(float)component;
    }
  }
  mg_json_get_bool(json,"$.safe_wall",&value.safe_wall);
  integer(json,"$.safe_wall_script",1,2,&value.safe_wall_script);
  integer(json,"$.safe_wall_offset",-10,10,&value.safe_wall_offset);
  mg_json_get_bool(json,"$.safe_wall_tight",&value.safe_wall_tight);
  { double gap; if(mg_json_get_num(json,"$.safe_wall_distance",&gap) && isfinite(gap) && gap>=0 && gap<=20) value.safe_wall_distance=(float)gap; }
  mg_json_get_bool(json,"$.fire_skin",&value.fire_skin);
  mg_json_get_bool(json,"$.net_stats",&value.net_stats);
  { double zoom; if(mg_json_get_num(json,"$.start_zoom",&zoom) && isfinite(zoom) && zoom>=.2 && zoom<=3) value.start_zoom=(float)zoom; }
  mg_json_get_bool(json,"$.wall_semaphore",&value.wall_semaphore);
  mg_json_get_bool(json,"$.glass_border",&value.glass_border);
  mg_json_get_bool(json,"$.sos_chat",&value.sos_chat);
  integer(json,"$.skin_mode",0,2,&value.skin_mode);
  mg_json_get_bool(json,"$.keep_own_skin",&value.keep_own_skin);
  { double gap; if(mg_json_get_num(json,"$.hug_gap",&gap) && isfinite(gap) && gap>=0 && gap<=30) value.hug_gap=(float)gap; }
  { double opacity; if(mg_json_get_num(json,"$.others_opacity",&opacity) && isfinite(opacity) && opacity>=.15 && opacity<=1) value.others_opacity=(float)opacity; }
  { double strength; if(mg_json_get_num(json,"$.fire_strength",&strength) && isfinite(strength) && strength>=.2 && strength<=1.5) value.fire_strength=(float)strength; }
  mg_json_get_bool(json,"$.gradient_enabled",&value.gradient_enabled);
  integer(json,"$.gradient_count",2,4,&value.gradient_count);
  for(int i=0;i<4;i++) for(int c=0;c<3;c++) {
    char key[40]; snprintf(key,sizeof(key),"$.gradient_%d_%d",i,c);
    double component;
    if(mg_json_get_num(json,key,&component) && isfinite(component) && component>=0 && component<=1)
      value.gradient_colors[i][c]=(float)component;
  }
  *out=value; return true;
}
bool native_preferences_save(const char* path,const native_preferences* value) {
  char temp[MAX_PATH]; if(snprintf(temp,sizeof(temp),"%s.tmp",path)>=(int)sizeof(temp)) return false;
  FILE* f=fopen(temp,"wb"); if(!f) return false;
  char* json=mg_mprintf("{\n  \"version\": 2,\n  \"frame_cap\": %d,\n  \"render_scale\": %d,\n  \"team_panel\": %s,\n  \"team_status\": %s,\n  \"fullscreen\": %s,\n  \"snake_outline\": %s,\n  \"head_point\": %s,\n  \"death_freeze\": %s,\n  \"uniform_names\": %s,\n  \"team_name_emphasis\": %s,\n  \"face_smile\": %s,\n  \"face_claws\": %s,\n  \"wifi_low_latency\": %s,\n  \"path_retry\": %s,\n  \"bot_smart\": %s,\n  \"bot_length\": %d,\n  \"missile_count\": %d,\n  \"gradient_enabled\": %s,\n  \"gradient_count\": %d,\n  \"gradient_0_0\": %.9g,\n  \"gradient_0_1\": %.9g,\n  \"gradient_0_2\": %.9g,\n  \"gradient_1_0\": %.9g,\n  \"gradient_1_1\": %.9g,\n  \"gradient_1_2\": %.9g,\n  \"gradient_2_0\": %.9g,\n  \"gradient_2_1\": %.9g,\n  \"gradient_2_2\": %.9g,\n  \"gradient_3_0\": %.9g,\n  \"gradient_3_1\": %.9g,\n  \"gradient_3_2\": %.9g,\n  \"name_r\": %.9g,\n  \"name_g\": %.9g,\n  \"name_b\": %.9g,\n  \"team_name_r\": %.9g,\n  \"team_name_g\": %.9g,\n  \"team_name_b\": %.9g,\n  \"safe_wall\": %s,\n  \"safe_wall_script\": %d,\n  \"safe_wall_offset\": %d,\n  \"safe_wall_tight\": %s,\n  \"safe_wall_distance\": %.9g,\n  \"fire_skin\": %s,\n  \"fire_strength\": %.9g,\n  \"net_stats\": %s,\n  \"start_zoom\": %.9g,\n  \"wall_semaphore\": %s,\n  \"glass_border\": %s,\n  \"sos_chat\": %s,\n  \"skin_mode\": %d,\n  \"keep_own_skin\": %s,\n  \"others_opacity\": %.9g,\n  \"hug_gap\": %.9g\n}\n",value->frame_cap,value->render_scale,value->team_panel?"true":"false",value->team_status?"true":"false",value->fullscreen?"true":"false",value->snake_outline?"true":"false",value->head_point?"true":"false",value->death_freeze?"true":"false",value->uniform_names?"true":"false",value->team_name_emphasis?"true":"false",value->face_smile?"true":"false",value->face_claws?"true":"false",value->wifi_low_latency?"true":"false",value->path_retry?"true":"false",value->bot_smart?"true":"false",value->bot_length,value->missile_count,value->gradient_enabled?"true":"false",value->gradient_count,(double)value->gradient_colors[0][0],(double)value->gradient_colors[0][1],(double)value->gradient_colors[0][2],(double)value->gradient_colors[1][0],(double)value->gradient_colors[1][1],(double)value->gradient_colors[1][2],(double)value->gradient_colors[2][0],(double)value->gradient_colors[2][1],(double)value->gradient_colors[2][2],(double)value->gradient_colors[3][0],(double)value->gradient_colors[3][1],(double)value->gradient_colors[3][2],(double)value->name_color[0],(double)value->name_color[1],(double)value->name_color[2],(double)value->team_name_color[0],(double)value->team_name_color[1],(double)value->team_name_color[2],value->safe_wall?"true":"false",value->safe_wall_script,value->safe_wall_offset,value->safe_wall_tight?"true":"false",(double)value->safe_wall_distance,value->fire_skin?"true":"false",(double)value->fire_strength,value->net_stats?"true":"false",(double)value->start_zoom,value->wall_semaphore?"true":"false",value->glass_border?"true":"false",value->sos_chat?"true":"false",value->skin_mode,value->keep_own_skin?"true":"false",(double)value->others_opacity,(double)value->hug_gap);
  bool ok=json && fputs(json,f)>=0; mg_free(json); if(fclose(f)) ok=false;
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
float native_start_zoom(void) {
  float z=1.5f*native_prefs.start_zoom;     // 1.5: the client's normal zoom
  return z<.025f ? .025f : z>10 ? 10 : z;
}
