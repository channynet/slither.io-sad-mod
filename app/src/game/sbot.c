#include "sbot.h"
#include "../user.h"
#include "defensive_planner.h"
#include "perf_trace.h"
#include <math.h>
#include <string.h>
static def_scene scene;
static uint64_t generation;
static float previous_angle,last_time;
static bool valid;
void sbot_init(tenv* env) {
  memset(&env->usr->gdata.bot,0,sizeof(env->usr->gdata.bot));
  valid=false; generation=0; last_time=0;
}
void sbot_go(tenv* env) {
  game_data* g=&env->usr->gdata;
  if(!env->usr->usrs.hotkeys[HOTKEY_BOT].active || !g->data.follow_view) return;
  snake* me=get_snake(g,g->data.snake_id);
  if(!me || me->dead) { valid=false; g->bot.output.accel=false; return; }
  float now=g->data.ctm;
  // At most 30 Hz, independent of FPS. Normal play never enters the planner.
  if(valid && generation==me->generation && now>=last_time && now-last_time<33) return;
  double started=perf_now();
  bool continuity=valid && generation==me->generation && now>=last_time && now-last_time<250;
  def_scene_reset(&scene);
  scene.position=(def_vec){me->xx,me->yy};
  scene.center=(def_vec){g->data.grd,g->data.grd};
  scene.angle=me->ang;
  // Same units as oef: vfr=milliseconds/8; displacement=sp*vfr/4.
  scene.speed=fmaxf(me->sp,me->ssp)*31.25f;
  scene.turn_rate=fmaxf(0.1f,g->data.mamu*me->scang*me->spang*125);
  scene.radius=fmaxf(1,me->sc*14.5f);
  scene.arena_radius=fminf(g->data.flux_grd,g->data.real_flux_grd);
  if(scene.arena_radius<=0) scene.arena_radius=g->data.flux_grd;
  scene.latency=fminf(0.5f,fmaxf(0.08f,g->data.ping/1000.0f+0.05f));
  if(g->data.lagging) scene.latency=0.5f;
  scene.has_previous=continuity; scene.previous_angle=previous_angle;
  for(int i=0;i<tdarray_length(g->data.snakes);i++) {
    snake* s=g->data.snakes+i;
    if(s->id==me->id || s->dead) continue;
    float radius=fmaxf(1,s->sc*14.5f);
    def_vec head={s->xx,s->yy};
    def_add_obstacle(&scene,(def_obstacle){.a=head,.b=head,.radius=radius,.head=true,
      .velocity={cosf(s->ang)*s->sp*31.25f,sinf(s->ang)*s->sp*31.25f}});
    def_vec previous=head;
    // pts are oldest (tail) to newest (neck). Connect neck to head as well.
    for(int j=tdarray_length(s->pts)-1;j>=0;j--) {
      body_part* p=s->pts+j; if(p->dying) continue;
      def_vec point={p->xx,p->yy};
      def_add_obstacle(&scene,(def_obstacle){.a=previous,.b=point,.radius=radius});
      previous=point;
    }
  }
  float food_score=0;
  for(int i=0;i<tdarray_length(g->data.foods);i++) {
    food* f=g->data.foods+i; if(f->eaten) continue;
    float dx=f->xx-me->xx,dy=f->yy-me->yy,d2=dx*dx+dy*dy;
    if(d2<100 || d2>160000) continue;
    float score=fminf(f->sz,10)/d2;
    if(score>food_score) { food_score=score; scene.food=(def_vec){f->xx,f->yy}; scene.has_food=true; }
  }
  def_result result=def_choose(&scene);
  previous_angle=result.angle; generation=me->generation; last_time=now; valid=true;
  g->bot.output.xm=cosf(result.angle)*500;
  g->bot.output.ym=sinf(result.angle)*500;
  g->bot.output.accel=false;
  perf_add(PERF_BOT,started);
}
void sbot_destroy(tenv* env) { (void)env; valid=false; }
