#include "duel_board.h"
#include "../user.h"
#include "../network/ntl.h"
#include "perf_trace.h"

// How far outside a body a head may still appear (the picture runs a little ahead of or
// behind the server) for that body to count as what it ran into.
#define DUEL_CLOSE 30.0f
#define DUEL_LOOSE 150.0f
#define DUEL_WINDOW_MS 1500.0

typedef struct { char name[DUEL_NAME]; double at; float gap; bool counted, confirmed; } recent_death;
// The server counted the kill: the kill effect goes off where that snake died.
static duel_table table;
static recent_death recents[8];
static int recent_next;
static bool death_done;
static double credit_at=-1e12;

const duel_table* duel_board_table(void) { return &table; }
bool duel_board_record(const char* name,int* kills,int* deaths) {
  if(!name || !name[0] || !table.count) return false;
  const duel_entry* e=duel_peek(&table,name);
  if(!e || !(e->kills+e->deaths)) return false;
  *kills=e->kills; *deaths=e->deaths;
  return true;
}
void duel_connected(void) { death_done=false; credit_at=-1e12; memset(recents,0,sizeof(recents)); }

// Distance from the head of `victim` to the surface of `o`, negative when it is inside.
static float body_gap(const snake* victim,const snake* o) {
  int n=tdarray_length(o->pts); if(n<1) return 1e9f;
  float x=victim->xx+victim->fx,y=victim->yy+victim->fy;
  float px=o->xx+o->fx,py=o->yy+o->fy,best=1e9f;
  for(int j=n-1;j>=0;j--) {
    if(o->pts[j].dying) break;
    float qx=o->pts[j].xx+o->pts[j].fx,qy=o->pts[j].yy+o->pts[j].fy;
    float d=duel_segment_distance(x,y,px,py,qx,qy);
    if(d<best) best=d;
    px=qx; py=qy;
  }
  return best-14.5f*o->sc-14.5f*victim->sc*.8f;
}
static const snake* nearest_body(game_data* game,const snake* victim,float* gap) {
  const snake* found=NULL; *gap=1e9f;
  int count=tdarray_length(game->data.snakes);
  for(int i=0;i<count;i++) {
    const snake* o=game->data.snakes+i;
    if(o==victim || o->id<0 || o->dead) continue;
    if(o->sp<0.01f) continue;   // a still body: a practice server's wall or label, not a player
    float g=body_gap(victim,o);
    if(g<*gap) { *gap=g; found=o; }
  }
  return found;
}
static void own_death(game_data* game,snake* me) {
  if(death_done) return;
  death_done=true;
  float gap=1e9f; const snake* by=me ? nearest_body(game,me,&gap) : NULL;
  if(by && gap<=DUEL_CLOSE) duel_add(&table,by->nk,false);
  else table.other_deaths++;   // the wall, or nothing we could see
}
void duel_snake_removed(game_data* game,snake* o,bool is_kill) {
  if(!is_kill || o->dead || o->id<0) return;
  if(o->id==game->data.snake_id) { own_death(game,o); return; }
  snake* me=get_snake(game,game->data.snake_id);
  if(!me || me->dead) return;
  double now=perf_now();
  float gap=1e9f,mine=body_gap(o,me);
  const snake* by=nearest_body(game,o,&gap);
  recent_death r={0}; strncpy(r.name,o->nk,DUEL_NAME-1); r.at=now; r.gap=mine;
  if(by==me && gap<=DUEL_CLOSE) { duel_add(&table,o->nk,true); r.counted=true; }
  else if(now-credit_at<DUEL_WINDOW_MS && mine<DUEL_LOOSE) {  // the server already said we killed someone
    duel_add(&table,o->nk,true); r.counted=r.confirmed=true; credit_at=-1e12;
  }
  if(r.counted && now-credit_at<DUEL_WINDOW_MS) { r.confirmed=true; credit_at=-1e12; }
  recents[recent_next]=r; recent_next=(recent_next+1)%8;
}
void duel_kill_confirmed(void) {
  double now=perf_now(); recent_death* pick=NULL;
  for(int i=0;i<8;i++) {   // a kill we already counted from positions
    recent_death* r=recents+i;
    if(r->at>0 && now-r->at<DUEL_WINDOW_MS && r->counted && !r->confirmed && (!pick || r->at<pick->at)) pick=r;
  }
  if(pick) { pick->confirmed=true; return; }
  for(int i=0;i<8;i++) {   // otherwise the snake that died closest to our body just now
    recent_death* r=recents+i;
    if(r->at>0 && now-r->at<DUEL_WINDOW_MS && !r->counted && r->gap<DUEL_LOOSE && (!pick || r->gap<pick->gap)) pick=r;
  }
  if(pick) { duel_add(&table,pick->name,true); pick->counted=pick->confirmed=true; }
  else credit_at=now;      // its removal has not arrived yet
}
void duel_game_over(game_data* game) { own_death(game,get_snake(game,game->data.snake_id)); }

void duel_board_draw(tenv* env) {
  game_data* game=&env->usr->gdata;
  if(game->curr_screen!=PLAYING || !twindow_key_down(env->wnd,GLFW_KEY_W)) return;
  if(igGetIO_Nil()->WantTextInput || ntl_chat_typing()) return;
  int order[20]; int rows=duel_sorted(&table,order,20),kills=0,deaths=table.other_deaths;
  for(int i=0;i<table.count;i++) { kills+=table.entries[i].kills; deaths+=table.entries[i].deaths; }
  // The font in use here has no Korean letters; the headings and Korean nicknames need this one.
  ImFont* font=env->usr->imgui_data.regular_font[FONT_SIZE_SMALL]; igPushFont(font,font->LegacySize);
  igSetNextWindowPos((ImVec2){env->ctx->size[0]*.5f,env->ctx->size[1]*.5f},ImGuiCond_Always,(ImVec2){.5f,.5f});
  igSetNextWindowBgAlpha(0.82f);
  igBegin("##duel_board",NULL,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoDocking|
      ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoNav);
  igText("상대 전적   킬 %d · 데스 %d",kills,deaths);
  igSeparator();
  if(!rows) igTextColored((ImVec4){1,1,1,.6f},"아직 기록이 없습니다");
  else if(igBeginTable("##duel_rows",3,ImGuiTableFlags_SizingFixedFit,(ImVec2){0},0)) {
    igTableSetupColumn("상대",ImGuiTableColumnFlags_WidthFixed,260,0);
    igTableSetupColumn("킬",ImGuiTableColumnFlags_WidthFixed,56,0);
    igTableSetupColumn("데스",ImGuiTableColumnFlags_WidthFixed,56,0);
    igTableHeadersRow();
    int count=tdarray_length(game->data.snakes);
    for(int k=0;k<rows;k++) {
      const duel_entry* e=table.entries+order[k];
      bool here=false;    // brighter when that opponent is on this server right now
      for(int i=0;i<count && !here;i++) {
        const snake* o=game->data.snakes+i;
        here=o->id>=0 && !o->dead && o->id!=game->data.snake_id && !strncmp(o->nk[0] ? o->nk : "(no name)",e->name,DUEL_NAME-1);
      }
      ImVec4 color={1,1,1,here ? 1.0f : .55f};
      igTableNextRow(ImGuiTableRowFlags_None,0);
      igTableSetColumnIndex(0); igTextColored(color,"%s",e->name);
      igTableSetColumnIndex(1); igTextColored((ImVec4){.55f,1,.6f,color.w},"%d",e->kills);
      igTableSetColumnIndex(2); igTextColored((ImVec4){1,.55f,.55f,color.w},"%d",e->deaths);
    }
    igEndTable();
  }
  if(table.other_deaths) igTextColored((ImVec4){1,1,1,.6f},"벽·기타로 죽음 %d",table.other_deaths);
  igEnd(); igPopFont();
}
