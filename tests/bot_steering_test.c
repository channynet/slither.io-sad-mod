#include "../app/src/user.h"
#include "../app/src/game/input.h"
#include "../app/src/game/native_preferences.h"
#include "../app/src/game/death_review.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
native_preferences native_prefs;
bool native_settings_open;
death_review_state death_review;
static tuser_data usr;
static snake me={.id=7};
static ImGuiIO io;
static bool captured,chat_typing,focused=true,held;
static bool q_held,target_captured;
static int steering,boost,stop_boost,last_angle;
static double mouse_x=500,mouse_y=500;
double glfwGetTime(void) { return 1; }
int glfwGetWindowAttrib(GLFWwindow* w,int a) { (void)w;(void)a;return focused; }
void glfwGetCursorPos(GLFWwindow* w,double* x,double* y) { (void)w;*x=mouse_x;*y=mouse_y; }
ImGuiIO* igGetIO_Nil(void) { return &io; }
bool ntl_mouse_captured(void) { return captured; }
bool ntl_chat_typing(void) { return chat_typing; }
bool ntl_is_teammate(tenv* env,int sid) { (void)env; (void)sid; return false; }
bool twindow_key_down(twindow* w,int k) {
  (void)w;
  if(k==GLFW_KEY_Q) return q_held;
  return held && (k==GLFW_KEY_SPACE || k==GLFW_KEY_UP || k==GLFW_KEY_LEFT_SHIFT || k==GLFW_KEY_RIGHT_SHIFT);
}
bool twindow_button_down(twindow* w,int k) { (void)w;(void)k;return held; }
snake* get_snake(game_data* g,int id) { (void)g;return id==7?&me:NULL; }
size_t mg_ws_send(struct mg_connection* c,const void* data,size_t n,int op) {
  (void)c;(void)op; unsigned char b=*(const unsigned char*)data;
  if(b<251) { steering++; last_angle=b; } if(b==253) boost++; if(b==254) stop_boost++; return n;
}
size_t _tdarray_length(void* a) { (void)a; return 0; }
bool target_marker_captured(void) { return target_captured; }
static int safe_logged,safe_logged_steer; static float safe_gap=-1;
void perf_safe_wall(bool on,bool steering,float gap) { safe_logged+=on; safe_logged_steer+=steering; safe_gap=gap; }
int tkeyboard_key_pressed(tkeyboard* k,int key) { (void)k;(void)key;return false; }
void igSetMouseCursor(ImGuiMouseCursor c) { (void)c; }
static void reset_packet(void) {
  usr.gdata.data.last_e_mtm=0; usr.gdata.data.lsxm=usr.gdata.data.lsym=0;
  usr.gdata.data.lsang=-1; steering=boost=stop_boost=0;
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
  // Real outbound manual steering continues in chat, including when hovering
  // over its input area. Space/Shift/left click must not accelerate.
  chat_typing=true;io.WantTextInput=true;captured=true;held=true;
  usr.gdata.data.md=true;usr.gdata.data.last_accel_mtm=1000;
  mouse_x=500;mouse_y=0;
  reset_packet();input_network(&env);
  assert(steering==1 && last_angle==0 && boost==0 && stop_boost==1 && !usr.gdata.data.md);
  mouse_x=0;mouse_y=500;
  reset_packet();input_network(&env);
  assert(steering==1 && last_angle==63 && boost==0 && stop_boost==0);   // the nearest of the 251 directions (62.75)
  // Direction changes are still coalesced; there is no extra packet stream.
  mouse_x=-500;mouse_y=0;input_network(&env);assert(steering==1);
  focused=false;reset_packet();input_network(&env);assert(steering==0);
  // After leaving chat, normal manual boost works again.
  focused=true;chat_typing=false;io.WantTextInput=false;captured=false;
  usr.gdata.data.last_accel_mtm=0;
  reset_packet();input_network(&env);assert(steering==1 && boost==1);
  // Safewall (NTL script 2): near the wall a mouse pointing out of the map steers to the safe circle.
  usr.gdata.data.grd=3400; usr.gdata.data.real_flux_grd=3332; usr.gdata.data.mamu=.033f; usr.gdata.data.nsp3=12; usr.gdata.data.gsc=1;
  me.xx=3400+3300; me.yy=3400; me.ang=.15f; me.wang=.15f; me.sp=5.79f; me.sc=1; me.scang=1; me.spang=1;
  usr.gdata.data.view_xx=me.xx; usr.gdata.data.view_yy=me.yy;
  mouse_x=500; mouse_y=250;                                // out of the map, 27 degrees off straight out
  native_prefs.safe_wall=true; native_prefs.safe_wall_script=2; native_prefs.safe_wall_offset=0;
  reset_packet(); input_network(&env);
  assert(steering==1 && last_angle>=40 && last_angle<=63 && safe_wall_steering);   // along the wall, the mouse's side (mouse alone: 19)
  assert(safe_logged>0 && safe_logged_steer>0 && fabsf(safe_gap-32)<1);    // recorded for performance.csv: 3332 - 3300
  // Further round the wall, the wall angle keeps being sent though the mouse has not moved.
  me.xx=3400+3300*cosf(.4f); me.yy=3400+3300*sinf(.4f); me.ang=.55f; me.wang=.55f;
  usr.gdata.data.view_xx=me.xx; usr.gdata.data.view_yy=me.yy; mouse_x=500*cosf(.86f); mouse_y=500*sinf(.86f);   // 26 degrees off out
  steering=0; usr.gdata.data.last_e_mtm=0; input_network(&env);
  assert(steering==1 && last_angle>=16+40 && safe_wall_steering);
  // Mouse straight out (NTL: within 9 degrees): the mouse is used.
  me.xx=3400+3300; me.yy=3400; me.ang=.15f; me.wang=.15f; usr.gdata.data.view_xx=me.xx; usr.gdata.data.view_yy=me.yy;
  mouse_x=500; mouse_y=0; reset_packet(); input_network(&env);
  assert(steering==1 && last_angle==0 && !safe_wall_steering);
  // Away from the wall the mouse's own angle is sent again at once.
  mouse_x=500; mouse_y=250; me.xx=3400; me.yy=3400; usr.gdata.data.view_xx=me.xx; usr.gdata.data.view_yy=me.yy;
  steering=0; usr.gdata.data.last_e_mtm=0; input_network(&env);
  assert(steering==1 && last_angle==19 && !safe_wall_steering);
  // Off: out of the map stays out of the map.
  native_prefs.safe_wall=false; me.xx=3400+3300; me.yy=3400; usr.gdata.data.view_xx=me.xx; usr.gdata.data.view_yy=me.yy;
  reset_packet(); input_network(&env); assert(steering==1 && last_angle==19 && !safe_wall_steering);
  // Q uses the existing outbound route: mouse aim + boost, even over bot/coil/hug.
  usr.gdata.curr_screen=PLAYING; held=false; q_held=true;
  usr.usrs.hotkeys[HOTKEY_BOT].active=true; native_coil=1; body_hug_active=true;
  mouse_x=500;mouse_y=0;usr.gdata.data.md=false;usr.gdata.data.last_accel_mtm=0;
  reset_packet();input_network(&env);
  assert(missile_steering && steering==1 && last_angle==0 && boost==1 && !body_hug_steering);
  mouse_x=0;mouse_y=500;reset_packet();input_network(&env);
  assert(missile_steering && last_angle==63 && fabsf(missile_angle-PI2/4)<.001f);
  mouse_x=-500;mouse_y=0;input_network(&env);assert(steering==1);  // same packet cadence
  native_coil=0;body_hug_active=false;
  q_held=false;reset_packet();input_network(&env);
  assert(!missile_steering && last_angle==63);  // bot owns steering again
  usr.usrs.hotkeys[HOTKEY_BOT].active=false;
  q_held=true;usr.gdata.data.last_accel_mtm=0;reset_packet();input_network(&env);
  assert(missile_steering && boost==1);
  q_held=false;usr.gdata.data.last_accel_mtm=1000;reset_packet();input_network(&env);
  assert(!missile_steering && stop_boost==1);  // key release does not wait 50 ms

  // A keyboard hold must work while the mouse rests over a non-editing panel.
  captured=true;q_held=true;usr.gdata.data.last_accel_mtm=0;reset_packet();input_network(&env);
  assert(missile_steering && boost==1 && steering==1);
  q_held=false;input_network(&env);captured=false;

  // Chat, focus, other UI, target selection, death and disconnect cancel a hold.
  // Holding Q while the guard clears cannot silently restart the missile.
  for(int guard=0;guard<8;guard++) {
    q_held=false;input_network(&env);
    usr.gdata.data.md=false;  // fresh live state after the preceding death/disconnect case
    q_held=true;usr.gdata.data.last_accel_mtm=0;reset_packet();input_network(&env);
    assert(missile_steering && boost==1);
    io.WantTextInput=guard==0;chat_typing=guard==0;focused=guard!=1;captured=guard==2;native_settings_open=guard==2;
    target_captured=guard==3;death_review.frozen=guard==4;me.dead=guard==5;
    connection.is_closing=guard==6;
    if(guard==7) usr.usrs.hotkeys[HOTKEY_ASSIST].key=GLFW_KEY_Q;
    reset_packet();input_network(&env);assert(!missile_steering && boost==0);
    io.WantTextInput=false;chat_typing=false;focused=true;captured=false;target_captured=false;
    death_review.frozen=false;me.dead=false;connection.is_closing=false;native_settings_open=false;
    usr.usrs.hotkeys[HOTKEY_ASSIST].key=GLFW_KEY_R;
    reset_packet();input_network(&env);assert(!missile_steering && boost==0);
    q_held=false;input_network(&env);
  }
  // The optional wall protection still overrides a missile near the boundary.
  q_held=true;native_prefs.safe_wall=true;me.xx=3400+3300;me.yy=3400;me.ang=.15f;me.wang=.15f;
  usr.gdata.data.view_xx=me.xx;usr.gdata.data.view_yy=me.yy;
  mouse_x=500;mouse_y=250;usr.gdata.data.last_accel_mtm=0;
  reset_packet();input_network(&env);
  assert(missile_steering && safe_wall_steering && last_angle>=40 && last_angle<=63);
  assert(fabsf(missile_angle-safe_wall_angle)<.001f);
  connection.is_closing=true; reset_packet();input_network(&env);assert(steering==0 && !missile_steering);
  puts("Missile: live aim, boost, release, mode priority, cancellation, key conflicts, cadence and wall protection passed");
  puts("Steering: chat follows moving mouse, no typing/click boost, immediate boost release, normal input resumes; bot, focus, other UI, cadence and socket guards passed");
}
