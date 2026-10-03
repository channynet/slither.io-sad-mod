#include "../app/src/user.h"
#include "../app/src/game/perf_trace.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
static tuser_data usr;
static int calls;
double perf_now(void) { calls++; return 0; }
void perf_add(int stage,double start) { (void)start; assert(stage==PERF_BOT); }
snake* get_snake(game_data* g,int id) {
  for(int i=0;i<tdarray_length(g->data.snakes);i++) if(g->data.snakes[i].id==id) return g->data.snakes+i;
  return NULL;
}
int main(void) {
  tenv env={.usr=&usr}; game_data* g=&usr.gdata;
  sbot_init(&env); sbot_go(&env); assert(calls==0);
  g->data.snakes=tdarray_create(snake);
  g->data.foods=tdarray_create(food);
  snake me={.id=42,.generation=1,.sp=5.76f,.ssp=5.76f,.sc=1,.spang=1,.scang=1};
  snake enemy={.id=99,.generation=2,.xx=130,.sc=1,.sp=0};
  enemy.pts=tdarray_create(body_part);
  body_part tail={.xx=130,.yy=-1000},neck={.xx=130,.yy=1000};
  tdarray_push(&enemy.pts,&tail); tdarray_push(&enemy.pts,&neck);
  tdarray_push(&g->data.snakes,&me); tdarray_push(&g->data.snakes,&enemy);
  g->data.snake_id=42; g->data.follow_view=true; g->data.mamu=3.0f/125;
  g->data.flux_grd=g->data.real_flux_grd=10000; g->data.ctm=100;
  usr.usrs.hotkeys[HOTKEY_BOT].active=true;
  sbot_go(&env); assert(calls==1 && !g->bot.output.accel);
  assert(fabsf(g->bot.output.ym)>100); // Uses our ID, not last snake in array.
  g->data.ctm=110; sbot_go(&env); assert(calls==1);
  g->data.ctm=140; sbot_go(&env); assert(calls==2);
  g->data.snakes[0].generation=3; sbot_go(&env); assert(calls==3);
  usr.usrs.hotkeys[HOTKEY_BOT].active=false;
  for(int i=0;i<10000;i++) sbot_go(&env); assert(calls==3);
  usr.usrs.hotkeys[HOTKEY_BOT].active=true;
  g->data.snakes[0].dead=true; sbot_go(&env); assert(calls==3 && !g->bot.output.accel);
  puts("Defensive bot integration: own identity, body segments, no boost, cadence, respawn and zero OFF planning passed");
}
