#include "target_marker.h"
#include "../user.h"
#include "../network/ntl.h"
#include "../network/ntl_protocol.h"
#include "../platform/compat.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct { int id; uint64_t generation; } target_ref;
static target_ref choices[36], selected;
static int choice_count;
static bool selecting;
static bool consumed;
typedef struct {
  char wire[NTL_TARGET],key[NTL_TARGET];
  uint64_t seen;
  uint32_t serial;
  bool bound,dead;
} team_target;
static team_target team_targets[64];
static int report_cursor,target_slots;
static char announce[NTL_TARGET];
void target_marker_clear_team(void) { memset(team_targets,0,sizeof(team_targets)); announce[0]=0; report_cursor=target_slots=0; }
static team_target* remember(const char* wire,uint64_t now) {
  if(!ntl_target_valid(wire)) return NULL;
  char key[NTL_TARGET]; snprintf(key,sizeof(key),"%s",wire);
  char* ring=strchr(key,'|'); if(ring) *ring=0;
  int slot=-1;
  for(int i=0;i<target_slots;i++) {
    team_target* t=team_targets+i;
    if(!strcmp(t->key,key)) { slot=i; break; }
    if(slot<0 && (!t->key[0] || now-t->seen>300000)) slot=i;
  }
  if(slot<0 && target_slots<64) slot=target_slots++;
  if(slot<0) return NULL;
  team_target* t=team_targets+slot;
  if(strcmp(t->key,key) || now-t->seen>300000) { memset(t,0,sizeof(*t)); snprintf(t->key,sizeof(t->key),"%s",key); }
  if(!t->dead) t->seen=now;
  // A bare sighting must not erase an NTL ring assignment.
  if(ring || !t->wire[0]) snprintf(t->wire,sizeof(t->wire),"%s",wire);
  return t;
}
static team_target* marked(snake* s,uint64_t now) {
  if(!s->target_key[0]) return NULL;
  for(int i=0;i<target_slots;i++) {
    team_target* t=team_targets+i;
    if(!t->key[0] || t->dead || now-t->seen>300000 || strcmp(t->key,s->target_key)) continue;
    if(s->dead || (t->bound && s->has_serial && t->serial!=s->serial)) { t->dead=true; continue; }
    if(s->has_serial) { t->bound=true; t->serial=s->serial; }
    t->seen=now; return t;
  }
  return NULL;
}
void target_marker_receive(tenv* env,const char* wire) {
  if(env->usr->gdata.conn==CONNECTED && env->usr->gdata.data.follow_view) remember(wire,GetTickCount64());
}
void target_marker_report(tenv* env,char* out,size_t capacity) {
  if(!capacity) return; out[0]=0;
  game_data* g=&env->usr->gdata;
  if(g->conn!=CONNECTED || !g->data.follow_view) return;
  if(!target_slots && !announce[0]) return;
  uint64_t now=GetTickCount64();
  for(int i=0;i<tdarray_length(g->data.snakes);i++) marked(g->data.snakes+i,now);
  if(announce[0]) { snprintf(out,capacity,"%s",announce); announce[0]=0; return; }
  // Only relay a target that is actually still visible, one per normal poll.
  for(int k=0;k<target_slots;k++) {
    int i=(report_cursor+k)%target_slots; team_target* t=team_targets+i;
    if(t->key[0] && !t->dead && t->seen==now) {
      bool present=false;
      for(int j=0;j<tdarray_length(g->data.snakes);j++)
        if(!g->data.snakes[j].dead && !strcmp(t->key,g->data.snakes[j].target_key)) { present=true; break; }
      if(present) { snprintf(out,capacity,"%s",t->wire); report_cursor=(i+1)%target_slots; return; }
    }
  }
}
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
  target_marker_clear_team();
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
  if(tkeyboard_key_pressed(env->kb,GLFW_KEY_DELETE)) { selected=(target_ref){0}; target_marker_clear_team(); selecting=false; return true; }
  for(int i=0;i<choice_count;i++) {
    int key=i<10 ? GLFW_KEY_0+i : GLFW_KEY_A+i-10;
    if(!tkeyboard_key_pressed(env->kb,key)) continue;
    snake* s=resolve(g,choices[i]);
    if(s && !ntl_is_teammate(env,s->ntl_id)) {
      if(selected.generation==s->generation) {
        selected=(target_ref){0}; announce[0]=0;
        for(int j=0;j<target_slots;j++) if(!strcmp(team_targets[j].key,s->target_key)) team_targets[j].dead=true;
      }
      else {
        selected=choices[i]; snprintf(selected_name,sizeof(selected_name),"%s",s->nk[0]?s->nk:"(no name)");
        if(remember(s->target_key,GetTickCount64())) snprintf(announce,sizeof(announce),"%s",s->target_key);
      }
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
  ImDrawList* draw=igGetWindowDrawList();
  uint64_t now=GetTickCount64();
  for(int i=0;i<tdarray_length(g->data.snakes);i++) {
    snake* s=g->data.snakes+i;
    if(s==target || s->id==g->data.snake_id || !marked(s,now)) continue;
    ImVec2 p=head(env,s); if(!visible(env,p)) continue;
    p.y+=40+11*s->sc*g->data.gsc;
    ImDrawList_AddCircleFilled(draw,p,13,0xff2222ed,24);
    ImDrawList_AddCircleFilled(draw,p,9,0xffffffff,24);
    ImDrawList_AddCircleFilled(draw,p,5,0xff2222ed,24);
  }
  if(!selecting && !target) return;
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
