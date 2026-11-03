#include "../app/src/game/snake_missile.h"
#include "../app/src/platform/missile_worker.h"
#include "../app/src/game/death_review.h"
#include "../app/src/user.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
bool native_settings_open;
death_review_state death_review;
static ImGuiIO io;
static snake me={.id=7};
static bool key,focused=true,target_captured;
static double mouse_x=600,mouse_y=300,now=1;
static int starts,stops,targets;
static float target_x,target_y;
static missile_worker_state worker;
bool twindow_key_down(twindow* w,int k){(void)w;return k==GLFW_KEY_G&&key;}
int glfwGetWindowAttrib(GLFWwindow* w,int attribute){(void)w;(void)attribute;return focused;}
void glfwGetCursorPos(GLFWwindow* w,double* x,double* y){(void)w;*x=mouse_x;*y=mouse_y;}
double glfwGetTime(void){return now;}
ImGuiIO* igGetIO_Nil(void){return &io;}
snake* get_snake(game_data* g,int id){(void)g;return id==7?&me:NULL;}
bool target_marker_captured(void){return target_captured;}
bool missile_worker_start(const char* server,float x,float y){assert(!strcmp(server,"127.0.0.1:4444"));starts++;target_x=x;target_y=y;worker.running=true;worker.active=worker.total=2;return true;}
void missile_worker_stop(void){if(worker.running)stops++;worker.running=false;}
void missile_worker_target(float x,float y){targets++;target_x=x;target_y=y;}
missile_worker_state missile_worker_poll(void){return worker;}
int main(void){
  static tuser_data usr;static tcontext ctx={.size={800,600}};twindow wnd={0};tenv env={.usr=&usr,.ctx=&ctx,.wnd=&wnd};
  struct mg_connection conn={0};usr.gdata.connection=&conn;usr.gdata.conn=CONNECTED;usr.gdata.curr_screen=PLAYING;
  usr.gdata.data.follow_view=true;usr.gdata.data.snake_id=7;usr.gdata.data.gsc=2;usr.gdata.data.view_xx=1000;usr.gdata.data.view_yy=1000;
  strcpy(usr.usrs.ipv4,"127.0.0.1:4444");snake_missile_reset();snake_missile_input(&env);
  key=true;snake_missile_input(&env);assert(starts==1&&snake_missile_active&&target_x==1100&&target_y==1000);
  snake_missile_input(&env);assert(starts==1);
  key=false;mouse_x=400;mouse_y=500;now+=.2;snake_missile_input(&env);
  assert(snake_missile_active&&target_x==1000&&target_y==1100&&targets>0);
  assert(!usr.gdata.data.md&&!usr.gdata.data.wmd); // G cannot accelerate the player.
  key=true;snake_missile_input(&env);assert(!snake_missile_active&&stops==1);
  for(int guard=0;guard<8;guard++){
    key=false;snake_missile_input(&env);key=true;snake_missile_input(&env);assert(snake_missile_active);
    key=false;snake_missile_input(&env);io.WantTextInput=guard==0;native_settings_open=guard==1;focused=guard!=2;
    target_captured=guard==3;me.dead=guard==4;conn.is_closing=guard==5;death_review.frozen=guard==6;
    if(guard==7)usr.usrs.hotkeys[0].key=GLFW_KEY_G;
    snake_missile_input(&env);assert(!snake_missile_active&&!worker.running);
    io.WantTextInput=false;native_settings_open=false;focused=true;target_captured=false;me.dead=false;conn.is_closing=false;death_review.frozen=false;usr.usrs.hotkeys[0].key=0;
    snake_missile_input(&env);assert(!snake_missile_active);
  }
  key=true;snake_missile_input(&env);assert(snake_missile_active);snake_missile_reset();snake_missile_input(&env);assert(!snake_missile_active);
  puts("G: starts the separate bot engine once, keeps it running, updates world target, stops on next G/reset/guards, never boosts the player");
}
