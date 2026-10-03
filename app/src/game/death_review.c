#include "death_review.h"
#include "native_preferences.h"
#include "../user.h"
#include "../network/server.h"
death_review_state death_review={.last_frame=-1,.frame=-1};
void death_review_reset(void) {
  death_review=(death_review_state){.last_frame=-1,.frame=-1};
}
bool death_review_begin(tenv* env) {
  if(!native_prefs.death_freeze || death_review.last_frame<0) return false;
  death_review.frozen=true;
  death_review.frame=death_review.last_frame;
  game_data* game=&env->usr->gdata;
  game->data.follow_view=false;
  env->usr->usrs.kills=game->data.kills;
  env->usr->usrs.score=game->data.score;
  env->usr->usrs.play_time=game->data.play_etm;
  save_user_settings(&env->usr->usrs);
  env->usr->gdata.restart_req=false;
  if(env->usr->gdata.connection) env->usr->gdata.connection->is_closing=true;
  return true;
}
void death_review_draw(tenv* env) {
  ImFont* font=env->usr->imgui_data.regular_font[FONT_SIZE_SMALL];
  igPushFont(font,font->LegacySize);
  igSetNextWindowPos((ImVec2){env->ctx->size[0]*.5f,20},ImGuiCond_Always,(ImVec2){.5f,0});
  igSetNextWindowBgAlpha(.85f);
  igBegin("사망 장면",NULL,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings);
  igTextUnformatted("사망 장면 정지",NULL);
  igTextDisabled("사망 알림 직전 마지막 게임 화면입니다.");
  igTextDisabled("서버의 정확한 충돌 판정을 재현하는 화면은 아닙니다.");
  if(igButton("다시 시작 · R",(ImVec2){0})) death_review.action=1;
  igSameLine(0,12);
  if(igButton("메뉴로 · Esc",(ImVec2){0})) death_review.action=2;
  if(!igGetIO_Nil()->WantTextInput) {
    if(tkeyboard_key_pressed(env->kb,GLFW_KEY_R)) death_review.action=1;
    if(tkeyboard_key_pressed(env->kb,GLFW_KEY_ESCAPE)) death_review.action=2;
  }
  igEnd(); igPopFont();
}
void death_review_finish(tenv* env) {
  game_data* game=&env->usr->gdata;
  if(!death_review.frozen || !death_review.action || game->connection) return;
  int action=death_review.action;
  // All in-flight UI frames can sample the held image. Drain them before reuse.
  vkDeviceWaitIdle(env->ctx->device);
  game_data_reset(env);
  game->closed=false; game->restart_req=false;
  if(action==1) {
    game->conn=CONNECTING; game->curr_screen=PLAYING;
    glfwSetTime(0); server_connect(env);
  } else {
    game->conn=DISCONNECTED; game->curr_screen=TITLE_SCREEN;
  }
}
