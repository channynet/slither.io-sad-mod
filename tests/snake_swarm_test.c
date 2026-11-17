#include "../app/src/game/snake_swarm.h"
#include "../app/src/platform/swarm_worker.h"
#include "../app/src/game/death_review.h"
#include "../app/src/game/native_preferences.h"
#include "../app/src/user.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
// Rendering is verified by the actual ImGui preview. This test exercises input lifecycle.
void igSetNextItemWidth(float v){}
void igTextWrapped(const char* f,...){}
void igSeparatorText(const char* s){}
bool igButton(const char* s,const ImVec2 v){return false;}
bool igCheckbox(const char* s,bool* v){return false;}
bool igCombo_Str_arr(const char* s,int* v,const char* const items[],int n,int h){return false;}
bool igSliderInt(const char* s,int* v,int a,int b,const char* f,ImGuiSliderFlags flags){return false;}
void igBeginDisabled(bool v){}
void igEndDisabled(void){}
ImDrawList* igGetWindowDrawList(void){return NULL;}
ImU32 igColorConvertFloat4ToU32(const ImVec4 v){return 0;}
void ImDrawList_AddCircle(ImDrawList* d,const ImVec2 p,float r,ImU32 c,int n,float t){}
void ImDrawList_AddText_Vec2(ImDrawList* d,const ImVec2 p,ImU32 c,const char* s,const char* e){}
bool native_settings_open;
native_preferences native_prefs={.swarm_count=100};
death_review_state death_review;
static ImGuiIO io;
static snake me={.id=7};
static bool key,focused=true,target_captured;
static double mouse_x=600,mouse_y=300,now=1;
static int starts,stops,targets;
static float target_x,target_y;
static swarm_worker_state worker;
bool twindow_key_down(twindow* w,int k){(void)w;return k==GLFW_KEY_G&&key;}
int glfwGetWindowAttrib(GLFWwindow* w,int attribute){(void)w;(void)attribute;return focused;}
void glfwGetCursorPos(GLFWwindow* w,double* x,double* y){(void)w;*x=mouse_x;*y=mouse_y;}
double glfwGetTime(void){return now;}
ImGuiIO* igGetIO_Nil(void){return &io;}
snake* get_snake(game_data* g,int id){(void)g;return id==7?&me:NULL;}
bool target_marker_captured(void){return target_captured;}
bool swarm_worker_start(const char* server,float x,float y,int count){assert(!strcmp(server,"127.0.0.1:4444"));assert(count==native_prefs.swarm_count);starts++;target_x=x;target_y=y;worker.running=true;worker.active=worker.total=count;return true;}
void swarm_worker_stop(void){if(worker.running)stops++;worker.running=false;}
bool swarm_worker_control(float x,float y,int mode,double angle,bool boost){(void)mode;(void)angle;(void)boost;targets++;target_x=x;target_y=y;return true;}
bool ntl_mouse_captured(void){return native_settings_open;}
swarm_worker_state swarm_worker_poll(void){return worker;}
int main(void){
  static tuser_data usr;static tcontext ctx={.size={800,600}};twindow wnd={0};tenv env={.usr=&usr,.ctx=&ctx,.wnd=&wnd};
  env.config.running=true;
  struct mg_connection conn={0};usr.gdata.connection=&conn;usr.gdata.conn=CONNECTED;usr.gdata.curr_screen=PLAYING;
  usr.gdata.data.follow_view=true;usr.gdata.data.snake_id=7;usr.gdata.data.gsc=2;usr.gdata.data.view_xx=1000;usr.gdata.data.view_yy=1000;
  me.xx=1000;me.yy=1000;
  strcpy(usr.usrs.ipv4,"127.0.0.1:4444");snake_swarm_stop();snake_swarm_tick(&env);
  key=true;snake_swarm_tick(&env);assert(starts==1&&worker.running&&target_x==1100&&target_y==1000);
  snake_swarm_tick(&env);assert(starts==1&&worker.total==100);
  key=false;mouse_x=400;mouse_y=500;now+=.2;snake_swarm_tick(&env);
  assert(worker.running&&target_x==1000&&target_y==1100&&targets>0);
  assert(!usr.gdata.data.md&&!usr.gdata.data.wmd);
  // Opening UI or changing focus keeps the last mouse target and the worker alive.
  native_settings_open=true;mouse_x=700;now+=.2;snake_swarm_tick(&env);
  assert(worker.running&&target_x==1000&&target_y==1100);
  native_settings_open=false;focused=false;now+=.2;snake_swarm_tick(&env);assert(worker.running&&target_x==1000);
  focused=true;io.WantTextInput=true;key=true;now+=.2;snake_swarm_tick(&env);assert(worker.running&&starts==1);
  key=false;io.WantTextInput=false;snake_swarm_tick(&env);
  key=true;snake_swarm_tick(&env);assert(!worker.running&&stops==1);
  key=false;snake_swarm_tick(&env);native_prefs.swarm_count=1;native_prefs.swarm_mode=1;
  key=true;now+=.2;snake_swarm_tick(&env);assert(worker.running&&target_x==1000&&target_y==1000);
  key=false;me.xx=1200;me.yy=1300;now+=.2;snake_swarm_tick(&env);assert(target_x==1200&&target_y==1300);
  native_prefs.swarm_mode=4;now+=.2;snake_swarm_tick(&env);assert(target_x==1200&&target_y==1300);
  me.xx=1500;now+=.2;snake_swarm_tick(&env);assert(target_x==1200);
  usr.gdata.network_session++;snake_swarm_tick(&env);assert(!worker.running);
  key=true;now+=.2;snake_swarm_tick(&env);assert(worker.running);
  me.dead=true;snake_swarm_tick(&env);assert(!worker.running);
  me.dead=false;key=false;snake_swarm_tick(&env);
  usr.usrs.hotkeys[0].key=GLFW_KEY_G;key=true;snake_swarm_tick(&env);assert(!worker.running);
  // Moving before G must anchor rain/tornado to the actual launch position.
  usr.usrs.hotkeys[0].key=0;key=false;snake_swarm_tick(&env);
  me.xx=2100;me.yy=2200;key=true;now+=.2;snake_swarm_tick(&env);
  assert(worker.running&&target_x==2100&&target_y==2200);
  env.config.running=false;snake_swarm_tick(&env);assert(!worker.running);
  puts("Swarm UI: single press, world targets, chat/settings/focus, modes, session/death/exit cleanup passed");
}
