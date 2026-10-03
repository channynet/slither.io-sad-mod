#include "sbot.h"
#include "../user.h"
#include "defensive_planner.h"
#include "body_loop.h"
#include "perf_trace.h"
#include <math.h>
#include <string.h>
static def_scene scene;
static body_loop own_loop;
static def_vec own_trail[BODY_LOOP_SCAN];
static void capture_body(const snake* me,float min_radius) {
  int count=1; own_trail[0]=(def_vec){me->xx,me->yy};
  int parts=me->pts?tdarray_length(me->pts):0;
  for(int i=parts-1;i>=0 && count<BODY_LOOP_SCAN;i--) {
    const body_part* p=me->pts+i; if(p->dying) continue;
    own_trail[count++]=(def_vec){p->xx,p->yy};
  }
  body_loop_capture(&own_loop,own_trail,count,me->ang,fmaxf(1,me->sc*14.5f),min_radius);
}
static uint64_t generation;
static float previous_angle,last_time;
static bool valid,was_active,manual_sample;
static float manual_angle,manual_time,manual_rate,manual_turn_time,manual_last_turn;
static uint64_t manual_generation;
static def_vec orbit_center;
static float orbit_radius;
static int orbit_direction;
void sbot_init(tenv* env) {
  memset(&env->usr->gdata.bot,0,sizeof(env->usr->gdata.bot));
  valid=false; generation=0; last_time=0;
  was_active=manual_sample=false; manual_rate=manual_turn_time=0; orbit_direction=0; own_loop.count=0;
}
void sbot_go(tenv* env) {
  game_data* g=&env->usr->gdata;
  bool active=env->usr->usrs.hotkeys[HOTKEY_BOT].active;
  if(!g->data.follow_view) { valid=was_active=manual_sample=false; g->bot.output.accel=false; orbit_direction=0; own_loop.count=0; return; }
  snake* me=get_snake(g,g->data.snake_id);
  if(!me || me->dead) { valid=was_active=manual_sample=false; orbit_direction=0; own_loop.count=0; g->bot.output.accel=false; return; }
  float now=g->data.ctm;
  if(!active) {
    // Only observe the manual heading here: no scene building or path search.
    float dt=(now-manual_time)/1000;
    if(manual_sample && manual_generation==me->generation && dt>0 && dt<0.15f) {
      float rate=def_wrap(me->ang-manual_angle)/dt;
      if(fabsf(rate)>0.15f && fabsf(rate)<15) {
        if(rate*manual_rate<=0) { manual_turn_time=0; manual_rate=rate; }
        float blend=1-expf(-dt/0.08f);
        manual_rate+=(rate-manual_rate)*blend; manual_turn_time+=dt; manual_last_turn=now;
      } else {
        // Brief zero-turn frames are normal between server/input updates.
        manual_rate*=expf(-dt/0.08f);
        if(now-manual_last_turn>100) { manual_rate=0; manual_turn_time=0; }
      }
    } else { manual_rate=manual_turn_time=0; }
    manual_sample=true; manual_angle=me->ang; manual_time=now; manual_generation=me->generation;
    was_active=false; valid=false; orbit_direction=0; own_loop.count=0; g->bot.output.accel=false;
    return;
  }
  // At most 30 Hz; include the one-time body capture in BOT cost accounting.
  if(was_active && valid && generation==me->generation && now>=last_time && now-last_time<33) return;
  double started=perf_now();
  if(!was_active || generation!=me->generation) {
    orbit_direction=0; own_loop.count=0; valid=false;
    if(!was_active && manual_sample && manual_generation==me->generation &&
       now>=manual_time && now-manual_time<150 && manual_turn_time>=0.08f && fabsf(manual_rate)>0.15f) {
      orbit_direction=manual_rate>0?1:-1;
      float speed=fmaxf(me->sp,me->ssp)*31.25f;
      float max_turn=fmaxf(0.1f,g->data.mamu*me->scang*me->spang*125);
      orbit_radius=fmaxf(speed/max_turn,speed/fabsf(manual_rate));
      orbit_center=(def_vec){me->xx-orbit_direction*sinf(me->ang)*orbit_radius,
                            me->yy+orbit_direction*cosf(me->ang)*orbit_radius};
    }
    // A completed body lap works even on a straight section of an oval,
    // when no recent manual turn sample exists. Capture once per activation.
    capture_body(me,fmaxf(me->sp,me->ssp)*31.25f/fmaxf(0.1f,g->data.mamu*me->scang*me->spang*125));
    was_active=true; manual_sample=false;
  }
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
  scene.body_path=own_loop.count?&own_loop:NULL;
  scene.orbit_direction=orbit_direction; scene.orbit_center=orbit_center; scene.orbit_radius=orbit_radius;
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
void sbot_destroy(tenv* env) { (void)env; valid=was_active=manual_sample=false; orbit_direction=0; own_loop.count=0; }
