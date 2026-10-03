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
  // Capture manual clockwise/counterclockwise curvature, then keep orbiting
  // for multiple full rotations. Food suppression is checked in the planner test.
  for(int sign=-1;sign<=1;sign+=2) {
    sbot_init(&env); g->data.snakes[1].dead=true;
    snake* own=&g->data.snakes[0]; own->dead=false; own->generation++;
    usr.usrs.hotkeys[HOTKEY_BOT].active=false;
    for(int i=0;i<=12;i++) {
      float angle=sign*i*0.03f;
      own->ang=angle; own->xx=sign*120*sinf(angle); own->yy=sign*120*(1-cosf(angle));
      g->data.ctm=1000+i*20; sbot_go(&env);
    }
    usr.usrs.hotkeys[HOTKEY_BOT].active=true;
    float total_turn=0;
    for(int i=0;i<200;i++) {
      g->data.ctm+=40; sbot_go(&env);
      float target=atan2f(g->bot.output.ym,g->bot.output.xm);
      float delta=remainderf(target-own->ang,6.28318530718f);
      delta=fmaxf(-0.12f,fminf(0.12f,delta));
      own->xx+=cosf(own->ang+delta/2)*180*0.04f;
      own->yy+=sinf(own->ang+delta/2)*180*0.04f;
      own->ang+=delta; total_turn+=delta;
    }
    float radius=hypotf(own->xx,own->yy-sign*120);
    assert(sign*total_turn>6.28f && radius>95 && radius<160);
    // Turning OFF and ON while straight must discard the old orbit.
    usr.usrs.hotkeys[HOTKEY_BOT].active=false;
    for(int i=0;i<10;i++) { g->data.ctm+=20; sbot_go(&env); }
    usr.usrs.hotkeys[HOTKEY_BOT].active=true; g->data.ctm+=40; sbot_go(&env);
    float target=atan2f(g->bot.output.ym,g->bot.output.xm);
    assert(fabsf(remainderf(target-own->ang,6.28318530718f))<0.001f);
  }
  // One zero-turn frame between manual movement and activation must not lose
  // the circle. Conversely, a fresh generation must not inherit it.
  sbot_init(&env); snake* own=&g->data.snakes[0]; own->dead=false;
  usr.usrs.hotkeys[HOTKEY_BOT].active=false;
  for(int i=0;i<=10;i++) {
    own->ang=i*0.03f; own->xx=120*sinf(own->ang); own->yy=120*(1-cosf(own->ang));
    g->data.ctm=20000+i*20; sbot_go(&env);
  }
  g->data.ctm+=20; sbot_go(&env); // same angle, just one frame
  usr.usrs.hotkeys[HOTKEY_BOT].active=true; g->data.ctm+=20; sbot_go(&env);
  own->xx+=cosf(own->ang)*15; own->yy+=sinf(own->ang)*15;
  g->data.ctm+=40; sbot_go(&env);
  assert(remainderf(atan2f(g->bot.output.ym,g->bot.output.xm)-own->ang,6.28318530718f)>0.02f);
  own->generation++; g->data.ctm+=40; sbot_go(&env);
  assert(fabsf(remainderf(atan2f(g->bot.output.ym,g->bot.output.xm)-own->ang,6.28318530718f))<0.001f);
  puts("Defensive bot integration: both manual circle directions and radii, OFF/ON reset; own identity, body segments, no boost, cadence, respawn and zero OFF planning passed");
}
