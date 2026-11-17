#include "snake_swarm.h"
#include "../user.h"
#include "../platform/swarm_worker.h"
#include "native_preferences.h"
#include "native_shortcuts.h"
#include "death_review.h"
#include "target_marker.h"
#include "../network/ntl.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool active,was_down,toggle_requested;
static float target_x,target_y,center_x,center_y;
static double last_command;
static int last_mode=-1;
static unsigned session;
static char status[320];
static const char* modes[]={"마우스 추적","사용자 추적","직진","비","회오리"};
static const char* reason(const char* state) {
 if(!strcmp(state,"connection_failed")) return "접속 실패 · 재시도 대기";
 if(!strcmp(state,"handshake_rejected")) return "생성 전 연결 종료 · 재시도 대기";
 if(!strcmp(state,"handshake_timeout")) return "생성 응답 없음 · 재시도 대기";
 if(!strcmp(state,"connection_closed")) return "연결 종료 · 재시도 대기";
 if(!strcmp(state,"control_timeout")) return "제어 신호 중단";
 if(!strcmp(state,"invalid_target")) return "잘못된 목표";
 if(!strcmp(state,"worker_failed") || !state[0]) return "봇 프로그램 실행 오류";
 return "초당 최대 1마리 접속";
}
void snake_swarm_stop(void) {
 if(active) swarm_worker_stop();
 active=false;toggle_requested=false;last_command=0;last_mode=-1;
}
static bool key_conflict(tenv* env) {
 for(int i=0;i<NUM_HOTKEYS;i++) if(env->usr->usrs.hotkeys[i].key==GLFW_KEY_G) return true;
 return false;
}
void snake_swarm_tick(tenv* env) {
 bool down=twindow_key_down(env->wnd,GLFW_KEY_G),pressed=down&&!was_down;
 was_down=down;
 game_data* g=&env->usr->gdata;
 snake* me=g->data.follow_view?get_snake(g,g->data.snake_id):NULL;
 bool playing=g->curr_screen==PLAYING && g->conn==CONNECTED && g->connection &&
  !g->closed && !g->connection->is_closing && me && !me->dead && !death_review.frozen && env->config.running;
 if(!playing || (active && session!=g->network_session)) {
  if(active) {snake_swarm_stop();snprintf(status,sizeof(status),"봇 종료 · 게임 접속 종료");}
  toggle_requested=false;return;
 }
 bool mouse_free=glfwGetWindowAttrib(env->wnd->handle,GLFW_FOCUSED) &&
  !igGetIO_Nil()->WantTextInput && !native_settings_open && !ntl_mouse_captured() && !target_marker_captured();
 bool modifier=twindow_key_down(env->wnd,GLFW_KEY_LEFT_CONTROL)||twindow_key_down(env->wnd,GLFW_KEY_RIGHT_CONTROL)||
  twindow_key_down(env->wnd,GLFW_KEY_LEFT_ALT)||twindow_key_down(env->wnd,GLFW_KEY_RIGHT_ALT)||
  twindow_key_down(env->wnd,GLFW_KEY_LEFT_SUPER)||twindow_key_down(env->wnd,GLFW_KEY_RIGHT_SUPER);
 bool toggle=toggle_requested || (pressed && mouse_free && !modifier && !key_conflict(env));
 toggle_requested=false;
 if(active && toggle) {snake_swarm_stop();snprintf(status,sizeof(status),"봇 종료");return;}
 if(native_prefs.swarm_mode<0 || native_prefs.swarm_mode>4) native_prefs.swarm_mode=0;
 if(last_mode!=native_prefs.swarm_mode || (toggle && !active)) {
  center_x=me->xx;center_y=me->yy;target_x=me->xx;target_y=me->yy;last_mode=native_prefs.swarm_mode;
 }
 if(native_prefs.swarm_mode==1) {target_x=me->xx;target_y=me->yy;}
 else if(native_prefs.swarm_mode>=2) {target_x=center_x;target_y=center_y;}
 else if(mouse_free && g->data.gsc>0 && isfinite(g->data.gsc)) {
  double x,y;glfwGetCursorPos(env->wnd->handle,&x,&y);
  target_x=g->data.view_xx+((float)x-env->ctx->size[0]/2.0f)/g->data.gsc;
  target_y=g->data.view_yy+((float)y-env->ctx->size[1]/2.0f)/g->data.gsc;
 }
 if(!isfinite(target_x)||!isfinite(target_y)) return;
 if(toggle && !active) {
  active=swarm_worker_start(env->usr->usrs.ipv4,target_x,target_y,native_prefs.swarm_count);
  if(!active) {snprintf(status,sizeof(status),"봇 실행 실패 · 봇 프로그램 파일을 확인하세요");return;}
  session=g->network_session;last_command=-1;
 }
 if(!active) return;
 double now=glfwGetTime();
 if(now-last_command>=.1 || now<last_command || last_command<0) {
  if(!swarm_worker_control(target_x,target_y,native_prefs.swarm_mode,
    native_prefs.swarm_direction*3.141592653589793/180,native_prefs.swarm_boost)) {
   snake_swarm_stop();snprintf(status,sizeof(status),"봇 종료 · 제어 신호 전송 실패");return;
  }
  last_command=now;
 }
 swarm_worker_state s=swarm_worker_poll();
 if(!s.running || !strcmp(s.state,"control_timeout") || !strcmp(s.state,"invalid_target")) {
  swarm_worker_stop();active=false;
  snprintf(status,sizeof(status),"봇 종료 · %s",reason(s.state));return;
 }
 snprintf(status,sizeof(status),"봇 %d/%d · 접속 중 %d · 재시도 대기 %d\n%s · %s",
  s.active,s.total,s.connecting,s.waiting,modes[native_prefs.swarm_mode],
  s.active==s.total?"G로 종료":reason(s.state));
}
void snake_swarm_settings(tenv* env) {
 igSeparatorText("봇 조종");
 igTextWrapped("이 PC에서 별도의 지렁이를 접속시킵니다. G를 한 번 누르면 시작하고 다시 누르면 종료합니다.");
 igBeginDisabled(active);
 igSetNextItemWidth(230);
 igSliderInt("생성 수",&native_prefs.swarm_count,1,100,"%d 마리",ImGuiSliderFlags_AlwaysClamp);
 igEndDisabled();
 igSetNextItemWidth(230);
 igCombo_Str_arr("이동 방식",&native_prefs.swarm_mode,modes,5,-1);
 if(native_prefs.swarm_mode==2 || native_prefs.swarm_mode==3) {
  igSetNextItemWidth(230);
  igSliderInt("진행 방향",&native_prefs.swarm_direction,0,359,"%d 도",ImGuiSliderFlags_AlwaysClamp);
  igTextWrapped("0도는 오른쪽, 90도는 아래쪽입니다.");
 }
 igCheckbox("봇 가속",&native_prefs.swarm_boost);
 igTextWrapped("생성 수는 요청 수입니다. 실제 생성 수는 게임 서버의 접속 허용 범위에 따라 달라집니다.");
 igTextWrapped("직진과 비는 지정한 방향으로 이동합니다. 비와 회오리의 중심은 시작할 때의 내 위치입니다.");
 if(key_conflict(env)) igTextWrapped("G가 다른 기능에 지정돼 있습니다. 단축키 설정을 바꾸거나 아래 버튼을 사용하세요.");
 bool ready=env->usr->gdata.curr_screen==PLAYING && env->usr->gdata.conn==CONNECTED;
 igBeginDisabled(!active && !ready);
 if(igButton(active?"봇 정지":"봇 시작",(ImVec2){150,0})) {
  if(active) {snake_swarm_stop();snprintf(status,sizeof(status),"봇 종료");}
  else toggle_requested=true;
 }
 igEndDisabled();
 if(!ready) igTextWrapped("게임에 접속한 뒤 G를 누르세요.");
 if(status[0]) igTextWrapped("%s",status);
 igTextWrapped("앱 종료·내 지렁이 사망·서버 변경 시 봇을 종료합니다. 설정과 채팅을 열면 마우스 목표를 유지합니다.");
}
void snake_swarm_draw(tenv* env) {
 if(!status[0]) return;
 ImDrawList* draw=igGetWindowDrawList();
 ImU32 color=igColorConvertFloat4ToU32((ImVec4){.75f,.95f,.9f,1});
 ImDrawList_AddText_Vec2(draw,(ImVec2){16,env->ctx->size[1]-105},color,status,NULL);
 if(active && native_prefs.swarm_mode!=2) {
  game_data* g=&env->usr->gdata;
  ImVec2 p={env->ctx->size[0]/2.0f+(target_x-g->data.view_xx)*g->data.gsc,
            env->ctx->size[1]/2.0f+(target_y-g->data.view_yy)*g->data.gsc};
  if(p.x>=0 && p.x<=env->ctx->size[0] && p.y>=0 && p.y<=env->ctx->size[1])
   ImDrawList_AddCircle(draw,p,12,color,24,2);
 }
}
