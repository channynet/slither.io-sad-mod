#include "../app/src/game/death_review.c"
#include "../app/src/game/target_marker.c"
#include <assert.h>
native_preferences native_prefs;
static int pressed=-1, saves, connects, waits;
// UI is exercised in the live client; these tests drive the actual state transitions.
void igPushFont(ImFont* f,float size) {}
void igPopFont(void) {}
void igSetNextWindowPos(ImVec2 p,ImGuiCond c,ImVec2 pivot) {}
void igSetNextWindowBgAlpha(float a) {}
bool igBegin(const char* n,bool* open,ImGuiWindowFlags flags) { return true; }
void igEnd(void) {}
void igTextUnformatted(const char* t,const char* end) {}
void igTextDisabled(const char* fmt,...) {}
void igText(const char* fmt,...) {}
bool igButton(const char* s,ImVec2 size) { return false; }
void igSameLine(float a,float b) {}
ImGuiIO* igGetIO_Nil(void) { static ImGuiIO io; return &io; }
ImDrawList* igGetWindowDrawList(void) { return NULL; }
void ImDrawList_AddCircleFilled(ImDrawList* d,ImVec2 p,float r,ImU32 c,int n) {}
void igCalcTextSize(ImVec2* out,const char* t,const char* e,bool h,float w) { *out=(ImVec2){10,10}; }
void ImDrawList_AddText_Vec2(ImDrawList* d,ImVec2 p,ImU32 c,const char* t,const char* e) {}
int tkeyboard_key_pressed(tkeyboard* kb,int key) { return key==pressed; }
bool ntl_is_teammate(tenv* env,int sid) { return sid==77; }
snake* get_snake(game_data* g,int id) {
  for(int i=0;i<tdarray_length(g->data.snakes);i++) if(g->data.snakes[i].id==id) return g->data.snakes+i;
  return NULL;
}
void save_user_settings(user_settings* s) { saves++; }
void game_data_reset(tenv* env) { death_review_reset(); target_marker_reset(); }
void server_connect(tenv* env) { connects++; }
void server_note_end(const char* format, ...) { (void)format; }
void duel_snake_removed(struct game_data* game,struct snake* o,bool is_kill) { (void)game; (void)o; (void)is_kill; }
void duel_kill_confirmed(void) {}
void duel_game_over(struct game_data* game) { (void)game; }
void duel_connected(void) {}
void glfwSetTime(double t) {}
VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice d) { waits++; return VK_SUCCESS; }
int main(void) {
  tuser_data* usr=calloc(1,sizeof(*usr)); tcontext ctx={0}; tenv env={.usr=usr,.ctx=&ctx};
  struct mg_connection connection={0}; game_data* g=&usr->gdata;
  g->connection=&connection; g->restart_req=true; g->data.follow_view=true;
  g->data.score=4321; native_prefs.death_freeze=true;
  assert(!death_review_begin(&env)); // No submitted live frame yet.
  death_review.last_frame=1; native_prefs.death_freeze=false;
  assert(!death_review_begin(&env));
  native_prefs.death_freeze=true;
  assert(death_review_begin(&env));
  assert(death_review.frozen && death_review.frame==1 && connection.is_closing);
  assert(!g->restart_req && !g->data.follow_view && usr->usrs.score==4321 && saves==1);
  death_review.action=1; death_review_finish(&env);
  assert(death_review.frozen && connects==0); // Wait for socket close.
  g->connection=NULL; g->closed=true; death_review_finish(&env);
  assert(!death_review.frozen && g->conn==CONNECTING && connects==1 && waits==1 && !g->closed);
  death_review.last_frame=0; assert(death_review_begin(&env));
  death_review.action=2; death_review_finish(&env);
  assert(g->curr_screen==TITLE_SCREEN && g->conn==DISCONNECTED && connects==1 && waits==2);

  g->curr_screen=PLAYING; g->conn=CONNECTED; g->data.follow_view=true;
  ctx.size[0]=1000; ctx.size[1]=800; g->data.gsc=1; g->data.snake_id=1;
  g->data.snakes=tdarray_create(snake);
  snake s={.id=1,.generation=1}; tdarray_push(&g->data.snakes,&s);
  s=(snake){.id=2,.generation=2,.ntl_id=77}; tdarray_push(&g->data.snakes,&s);
  s=(snake){.id=3,.generation=3,.ntl_id=3}; strcpy(s.nk,"enemy"); tdarray_push(&g->data.snakes,&s);
  s=(snake){.id=4,.generation=4,.xx=10000}; tdarray_push(&g->data.snakes,&s);
  assert(target_marker_input(&env,true) && choice_count==1); // Own, team, offscreen excluded.
  pressed=GLFW_KEY_0; assert(target_marker_input(&env,false));
  assert(!selecting && selected.id==3 && resolve(g,selected));
  get_snake(g,3)->generation=5; assert(!resolve(g,selected)); // Reused ID is not the old target.
  pressed=-1; target_marker_input(&env,true);
  get_snake(g,3)->generation=6; pressed=GLFW_KEY_0; target_marker_input(&env,false);
  assert(selected.generation==3); // Stale selector cannot pick replacement.
  pressed=-1; target_marker_input(&env,true); pressed=GLFW_KEY_DELETE; target_marker_input(&env,false);
  assert(!selecting && !selected.generation);
  snake* enemy=get_snake(g,3); strcpy(enemy->target_key,"9_3_27"); enemy->has_serial=true; enemy->serial=12345;
  target_marker_receive(&env,"9_3_27|2");
  assert(marked(enemy,GetTickCount64()));
  char report[48]; target_marker_report(&env,report,sizeof(report)); assert(!strcmp(report,"9_3_27|2"));
  // Preserve ring assignment when another member sends only a sighting.
  target_marker_receive(&env,"9_3_27"); target_marker_report(&env,report,sizeof(report)); assert(!strcmp(report,"9_3_27|2"));
  // A recycled server ID cannot inherit the old mark if serials differ.
  enemy->serial++; assert(!marked(enemy,GetTickCount64()));
  target_marker_report(&env,report,sizeof(report)); assert(!report[0]);
  target_marker_clear_team(); target_marker_receive(&env,"9_3_27");
  enemy->dead=true; assert(!marked(enemy,GetTickCount64())); enemy->dead=false;
  assert(!marked(enemy,GetTickCount64()));
  target_marker_clear_team(); target_marker_receive(&env,"9_3_27");
  assert(!marked(enemy,GetTickCount64()+300001));
  target_marker_clear_team(); g->data.follow_view=false;
  target_marker_receive(&env,"9_3_27"); assert(!marked(enemy,GetTickCount64()));
  tdarray_destroy(g->data.snakes); free(usr);
  puts("Gameplay review: death frame retention, disabled/no-frame cases, reconnect/menu lifecycle, target filtering and recycled IDs passed");
}
