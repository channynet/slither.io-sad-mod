#include "../app/src/user.h"
#include "../app/src/game/input.h"
#include <assert.h>
#include <stdio.h>
static tuser_data usr;
static snake me={.id=7};
static ImGuiIO io;
static bool captured,focused=true,held;
static int steering,boost;
double glfwGetTime(void) { return 1; }
int glfwGetWindowAttrib(GLFWwindow* w,int a) { (void)w;(void)a;return focused; }
void glfwGetCursorPos(GLFWwindow* w,double* x,double* y) { (void)w;*x=500;*y=500; }
ImGuiIO* igGetIO_Nil(void) { return &io; }
bool ntl_mouse_captured(void) { return captured; }
bool twindow_key_down(twindow* w,int k) { (void)w;(void)k;return held; }
bool twindow_button_down(twindow* w,int k) { (void)w;(void)k;return held; }
snake* get_snake(game_data* g,int id) { (void)g;return id==7?&me:NULL; }
size_t mg_ws_send(struct mg_connection* c,const void* data,size_t n,int op) {
  (void)c;(void)op; unsigned char b=*(const unsigned char*)data;
  if(b<251) steering++; if(b==253) boost++; return n;
}
size_t _tdarray_length(void* a) { (void)a; return 0; }
bool target_marker_captured(void) { return false; }
int tkeyboard_key_pressed(tkeyboard* k,int key) { (void)k;(void)key;return false; }
void igSetMouseCursor(ImGuiMouseCursor c) { (void)c; }
static void reset_packet(void) {
  usr.gdata.data.last_e_mtm=0; usr.gdata.data.lsxm=usr.gdata.data.lsym=0;
  usr.gdata.data.lsang=-1; steering=boost=0;
}
int main(void) {
  tcontext ctx={0}; twindow window={0}; tmouse mouse={0};
  tenv env={.usr=&usr,.ctx=&ctx,.wnd=&window,.ms=&mouse};
  struct mg_connection connection={0};
  usr.gdata.connection=&connection; usr.gdata.conn=CONNECTED;
  usr.gdata.data.follow_view=true;usr.gdata.data.snake_id=7;
  usr.gdata.bot.output.ym=500;usr.usrs.hotkeys[HOTKEY_BOT].active=true;
  // The actual outbound path, not a duplicated routing helper.
  for(int i=0;i<4;i++) {
    io.WantTextInput=i==1;captured=i==2;focused=i!=3;held=true;
    reset_packet(); input_network(&env); assert(steering==1 && boost==0);
  }
  usr.usrs.hotkeys[HOTKEY_BOT].active=false;
  for(int i=1;i<4;i++) {
    io.WantTextInput=i==1;captured=i==2;focused=i!=3;
    reset_packet(); input_network(&env); assert(steering==0 && boost==0);
  }
  io.WantTextInput=false;captured=false;focused=true;held=false;
  reset_packet(); input_network(&env);assert(steering==1);
  connection.is_closing=true; reset_packet();input_network(&env);assert(steering==0);
  puts("Bot steering: live output during chat/hover/focus loss, no boost; manual capture and closed socket guards passed");
}
