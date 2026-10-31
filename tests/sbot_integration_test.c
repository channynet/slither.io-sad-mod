#include "../app/src/user.h"
#include "../app/src/game/perf_trace.h"
#include "../app/src/game/defensive_planner.h"
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
  // A full body oval takes precedence over a guessed circle, including activation
  // on its straightest section with no manual turning sample at all.
  for(int sign=-1;sign<=1;sign+=2) {
    sbot_init(&env); own->generation++; own->dead=false;
    own->xx=0; own->yy=140; own->ang=sign>0?3.14159265359f:0;
    own->pts=tdarray_create(body_part);
    for(int j=299;j>=1;j--) {
      float a=1.57079632679f-sign*j*6.28318530718f/240;
      body_part p={.xx=220*cosf(a),.yy=140*sinf(a)}; tdarray_push(&own->pts,&p);
    }
    // Ignore a dying sample without joining to its invalid position.
    body_part dying={.xx=100000,.yy=100000,.dying=true}; tdarray_push(&own->pts,&dying);
    usr.usrs.hotkeys[HOTKEY_BOT].active=false;
    for(int j=0;j<10;j++) { g->data.ctm+=20; sbot_go(&env); }
    usr.usrs.hotkeys[HOTKEY_BOT].active=true;
    float total_turn=0,max_error=0;
    for(int j=0;j<1000;j++) {
      // Keep the neck at the moving head. A permanently frozen snapshot is
      // now intentionally expired rather than trusted for the entire test.
      tdarray_clear(own->pts);
      float phase=atan2f(own->yy/140,own->xx/220);
      for(int k=299;k>=1;k--) {
        float a=phase-sign*k*6.28318530718f/240;
        body_part p={.xx=220*cosf(a),.yy=140*sinf(a)}; tdarray_push(&own->pts,&p);
      }
      g->data.ctm+=40; sbot_go(&env);
      float target=atan2f(g->bot.output.ym,g->bot.output.xm);
      float delta=fmaxf(-.12f,fminf(.12f,remainderf(target-own->ang,6.28318530718f)));
      own->xx+=cosf(own->ang+delta/2)*7.2f; own->yy+=sinf(own->ang+delta/2)*7.2f;
      own->ang+=delta; total_turn+=delta;
      max_error=fmaxf(max_error,fabsf(hypotf(own->xx/220,own->yy/140)-1));
      assert(!g->bot.output.accel);
    }
    assert(sign*total_turn>30 && max_error<.12f);
    int prior_calls=calls;
    usr.usrs.hotkeys[HOTKEY_BOT].active=false;
    for(int j=0;j<1000;j++) { g->data.ctm+=20; sbot_go(&env); }
    assert(calls==prior_calls);
    // New life with no full body lap cannot reuse the previous oval.
    own->dead=true; sbot_go(&env); own->dead=false; own->generation++;
    tdarray_destroy(own->pts); own->pts=NULL;
    usr.usrs.hotkeys[HOTKEY_BOT].active=true; g->data.ctm+=40; sbot_go(&env);
    assert(fabsf(remainderf(atan2f(g->bot.output.ym,g->bot.output.xm)-own->ang,6.28318530718f))<.001f);
  }
  // Keep a missing body briefly, expire it, then reacquire a real lap. Never
  // steer toward an old enclosure after a large server position correction.
  sbot_init(&env); own->generation++; own->xx=220; own->yy=0; own->ang=1.57079632679f;
  own->pts=tdarray_create(body_part); g->data.ctm=100000;
  for(int pass=0;pass<2;pass++) {
    for(int k=299;k>=1;k--) {
      float a=-k*6.28318530718f/240;
      body_part p={.xx=220*cosf(a),.yy=140*sinf(a)}; tdarray_push(&own->pts,&p);
    }
    g->data.ctm+=120; sbot_go(&env);
    assert(def_wrap(atan2f(g->bot.output.ym,g->bot.output.xm)-own->ang)>.02f);
    tdarray_clear(own->pts);
    if(!pass) {
      g->data.ctm+=800; sbot_go(&env);
      assert(def_wrap(atan2f(g->bot.output.ym,g->bot.output.xm)-own->ang)>.02f);
      g->data.ctm+=800; sbot_go(&env);
      assert(fabsf(def_wrap(atan2f(g->bot.output.ym,g->bot.output.xm)-own->ang))<.001f);
    } else {
      own->xx+=2000; g->data.ctm+=120; sbot_go(&env);
      assert(fabsf(def_wrap(atan2f(g->bot.output.ym,g->bot.output.xm)-own->ang))<.001f);
    }
  }
  tdarray_destroy(own->pts); own->pts=NULL;
  puts("Defensive bot integration: circles/ovals, activation/reset, cadence, zero OFF planning; 800ms missing body retention, stale expiry, reacquisition and detached route rejection passed");
}
