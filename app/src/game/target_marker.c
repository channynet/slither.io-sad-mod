#include "target_marker.h"
#include "../user.h"
#include "../network/ntl.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct { int id; uint64_t generation; } target_ref;
static target_ref choices[36], selected;
static int choice_count;
static bool selecting;
static bool consumed;
bool target_marker_captured(void) { return selecting || consumed; }
static char selected_name[MAX_NICKNAME_LEN+1];
static snake* resolve(game_data* game,target_ref ref) {
  snake* s=get_snake(game,ref.id);
  return s && !s->dead && ref.generation && s->generation==ref.generation ? s : NULL;
}
static ImVec2 head(tenv* env,const snake* s) {
  game_data* g=&env->usr->gdata;
  return (ImVec2){env->ctx->size[0]*.5f+(s->xx+s->fx-g->data.view_xx)*g->data.gsc,
    env->ctx->size[1]*.5f+(s->yy+s->fy-g->data.view_yy)*g->data.gsc};
}
static bool visible(tenv* env,ImVec2 p) {
  return p.x>=20 && p.y>=40 && p.x<env->ctx->size[0]-20 && p.y<env->ctx->size[1]-60;
}
void target_marker_reset(void) {
  selecting=consumed=false; choice_count=0; selected=(target_ref){0}; selected_name[0]=0;
}
bool target_marker_input(tenv* env,bool toggle) {
  game_data* g=&env->usr->gdata;
  consumed=false;
  if(g->curr_screen!=PLAYING || g->conn!=CONNECTED || !g->data.follow_view) { selecting=false; return false; }
  if(toggle) {
    consumed=true;
    selecting=!selecting; choice_count=0;
    if(selecting) for(int i=0;i<tdarray_length(g->data.snakes) && choice_count<36;i++) {
      snake* s=g->data.snakes+i;
      if(s->id<0 || s->id==g->data.snake_id || s->dead || ntl_is_teammate(env,s->ntl_id) || !visible(env,head(env,s))) continue;
      choices[choice_count++]=(target_ref){s->id,s->generation};
    }
    return true;
  }
  if(!selecting) return false;
  consumed=true;
  if(tkeyboard_key_pressed(env->kb,GLFW_KEY_ESCAPE)) { selecting=false; return true; }
  if(tkeyboard_key_pressed(env->kb,GLFW_KEY_DELETE)) { selected=(target_ref){0}; selecting=false; return true; }
  for(int i=0;i<choice_count;i++) {
    int key=i<10 ? GLFW_KEY_0+i : GLFW_KEY_A+i-10;
    if(!tkeyboard_key_pressed(env->kb,key)) continue;
    snake* s=resolve(g,choices[i]);
    if(s && !ntl_is_teammate(env,s->ntl_id)) {
      if(selected.generation==s->generation) selected=(target_ref){0};
      else { selected=choices[i]; snprintf(selected_name,sizeof(selected_name),"%s",s->nk[0]?s->nk:"(no name)"); }
    }
    selecting=false; break;
  }
  return true;
}
void target_marker_draw(tenv* env) {
  game_data* g=&env->usr->gdata;
  snake* target=resolve(g,selected);
  // Discard a vanished/dead instance instead of marking another snake with a reused ID.
  if(selected.generation && !target) selected=(target_ref){0};
  if(!selecting && !target) return;
  ImDrawList* draw=igGetWindowDrawList();
  ImFont* font=env->usr->imgui_data.regular_font[FONT_SIZE_SMALL];
  igPushFont(font,font->LegacySize);
  if(selecting) for(int i=0;i<choice_count;i++) {
    snake* s=resolve(g,choices[i]); if(!s) continue;
    ImVec2 p=head(env,s); if(!visible(env,p)) continue;
    p.y-=28+11*s->sc*g->data.gsc;
    char label[2]={(char)(i<10?'0'+i:'A'+i-10),0};
    ImDrawList_AddCircleFilled(draw,p,16,0xe0000000,20);
    ImVec2 size; igCalcTextSize(&size,label,NULL,false,-1);
    ImDrawList_AddText_Vec2(draw,(ImVec2){p.x-size.x*.5f,p.y-size.y*.5f},0xffeeeeff,label,NULL);
  }
  if(target) {
    ImVec2 p=head(env,target);
    p.y+=40+11*target->sc*g->data.gsc;
    ImDrawList_AddCircleFilled(draw,p,13,0xff2222ed,24);
    ImDrawList_AddCircleFilled(draw,p,9,0xffffffff,24);
    ImDrawList_AddCircleFilled(draw,p,5,0xff2222ed,24);
  }
  igSetNextWindowPos((ImVec2){env->ctx->size[0]*.5f,16},ImGuiCond_Always,(ImVec2){.5f,0});
  igSetNextWindowBgAlpha(.7f);
  igBegin("##target_info",NULL,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings);
  if(selecting) {
    igTextUnformatted("타겟 선택 · 머리 위 숫자/문자 입력",NULL);
    igTextDisabled("Esc 취소 · Delete 표적 해제");
    if(!choice_count) igTextDisabled("화면 안에 선택할 상대가 없습니다.");
  } else igText("표적: %s · Backspace 두 번으로 변경",selected_name);
  igEnd(); igPopFont();
}
