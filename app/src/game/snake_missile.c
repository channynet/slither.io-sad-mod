#include "snake_missile.h"
#include "../user.h"
#include "../platform/missile_worker.h"
#include "native_shortcuts.h"
#include "death_review.h"
#include "target_marker.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
bool snake_missile_active;
float snake_missile_x,snake_missile_y;
char snake_missile_status[192];
static bool was_down;
static double last_target;
void snake_missile_reset(void) {
  missile_worker_stop();snake_missile_active=false;was_down=true;snake_missile_status[0]=0;
}
void snake_missile_input(tenv* env) {
  bool down=twindow_key_down(env->wnd,GLFW_KEY_G),pressed=down&&!was_down;was_down=down;
  missile_worker_state worker=missile_worker_poll();
  if(snake_missile_active && !worker.running) {
    snake_missile_active=false;
    snprintf(snake_missile_status,sizeof(snake_missile_status),"미사일 실행 실패: 설정·프록시 목록을 확인하세요 (%s)",worker.state);
  }
  if(!down && !snake_missile_active) return;
  game_data* g=&env->usr->gdata;
  snake* me=g->data.follow_view?get_snake(g,g->data.snake_id):NULL;
  const char* reason=NULL;
  if(g->curr_screen!=PLAYING || g->conn!=CONNECTED || !g->connection || g->closed || g->connection->is_closing || !me || me->dead || death_review.frozen)
    reason="미사일: 플레이 중에 G를 누르세요";
  else if(igGetIO_Nil()->WantTextInput) reason="미사일: 채팅 입력을 끝내세요";
  else if(native_settings_open) reason="미사일: 설정 창을 닫으세요";
  else if(target_marker_captured()) reason="미사일: 대상 선택을 끝내세요";
  else if(!glfwGetWindowAttrib(env->wnd->handle,GLFW_FOCUSED)) reason="미사일: 게임 창을 선택하세요";
  else if(twindow_key_down(env->wnd,GLFW_KEY_LEFT_CONTROL)||twindow_key_down(env->wnd,GLFW_KEY_RIGHT_CONTROL)||
      twindow_key_down(env->wnd,GLFW_KEY_LEFT_ALT)||twindow_key_down(env->wnd,GLFW_KEY_RIGHT_ALT)||
      twindow_key_down(env->wnd,GLFW_KEY_LEFT_SUPER)||twindow_key_down(env->wnd,GLFW_KEY_RIGHT_SUPER))
    reason="미사일: G만 누르세요";
  for(int i=0;i<NUM_HOTKEYS;i++) if(env->usr->usrs.hotkeys[i].key==GLFW_KEY_G) reason="미사일: 다른 기능의 G 단축키를 바꾸세요";
  if(reason) {
    if(snake_missile_active) missile_worker_stop();snake_missile_active=false;
    snprintf(snake_missile_status,sizeof(snake_missile_status),"%s",reason);return;
  }
  double x,y;glfwGetCursorPos(env->wnd->handle,&x,&y);
  float scale=g->data.gsc;if(scale<=0 || !isfinite(scale)) return;
  snake_missile_x=g->data.view_xx+((float)x-env->ctx->size[0]/2.0f)/scale;
  snake_missile_y=g->data.view_yy+((float)y-env->ctx->size[1]/2.0f)/scale;
  if(pressed) {
    if(snake_missile_active) {
      missile_worker_stop();snake_missile_active=false;
      snprintf(snake_missile_status,sizeof(snake_missile_status),"미사일 종료");return;
    }
    snake_missile_active=missile_worker_start(env->usr->usrs.ipv4,snake_missile_x,snake_missile_y);
    last_target=0;
    if(!snake_missile_active) {
      snprintf(snake_missile_status,sizeof(snake_missile_status),"미사일 엔진을 실행하지 못했습니다. 배포 폴더를 확인하세요");return;
    }
  }
  if(snake_missile_active) {
    double now=glfwGetTime();
    if(now-last_target>=.1) {missile_worker_target(snake_missile_x,snake_missile_y);last_target=now;}
    const char* note=!strcmp(worker.state,"connection_failed")?"접속 실패: 프록시 설정 확인":
      !strcmp(worker.state,"handshake_timeout")?"서버 응답 지연":worker.active?"마우스로 목표 지정 / G로 종료":"연결 중 / G로 종료";
    snprintf(snake_missile_status,sizeof(snake_missile_status),"미사일 지렁이 %d/%d · %s",worker.active,worker.total,note);
  }
}
