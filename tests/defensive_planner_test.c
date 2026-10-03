#include "../app/src/game/defensive_planner.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <windows.h>
static def_scene s;
static void reset(void) {
  def_scene_reset(&s); s.speed=180; s.turn_rate=3; s.radius=15;
  s.arena_radius=10000; s.latency=0.1f;
}
static void wall(float x,float y1,float y2) {
  def_add_obstacle(&s,(def_obstacle){.a={x,y1},.b={x,y2},.radius=15});
}
static double milliseconds(void) {
  LARGE_INTEGER f,t; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t);
  return t.QuadPart*1000.0/f.QuadPart;
}
int main(void) {
  reset(); assert(fabsf(def_choose(&s).angle)<0.001f);
  assert(fabsf(def_wrap(3.13f-(-3.13f)))<0.03f);
  // Continuous segments catch a wall even with far-apart body points.
  wall(130,-1000,1000);
  assert(!def_evaluate(&s,0).safe);
  def_result r=def_choose(&s); assert(r.safe && fabsf(r.angle)>0.5f);
  // Food behind the wall cannot override a safe escape.
  s.has_food=true; s.food=(def_vec){250,0}; assert(def_choose(&s).safe);
  // Prediction detects a head crossing a currently clear route.
  reset(); def_add_obstacle(&s,(def_obstacle){.a={140,-130},.b={140,-130},.velocity={0,180},.radius=20,.head=true});
  assert(!def_evaluate(&s,0).safe); assert(def_choose(&s).safe);
  // Border avoidance uses world center/radius, not camera position.
  reset(); s.position=(def_vec){850,0}; s.arena_radius=1000;
  assert(!def_evaluate(&s,0).safe); assert(def_choose(&s).safe);
  // A gap narrower than our diameter + margin is not an escape.
  reset(); wall(120,-1000,-25); wall(120,25,1000);
  assert(!def_evaluate(&s,0).safe); assert(def_choose(&s).safe);
  // High command latency must not improve a physically impossible turn.
  reset(); wall(95,-1000,1000); r=def_choose(&s);
  s.latency=0.5f; assert(def_choose(&s).safe_seconds<=r.safe_seconds);
  // Overflow cannot drop a late-arriving close threat or invent safe space.
  reset(); for(int i=0;i<800;i++) wall(400+i,-10,10);
  wall(80,-1000,1000); assert(s.count==DEF_OBSTACLES);
  assert(!def_evaluate(&s,0).safe);
  // Mirror symmetry and translated geometry.
  reset(); s.position=(def_vec){20000,20000}; s.center=s.position;
  wall(20130,19000,21000); assert(def_choose(&s).safe);
  // A completely blocked scene yields a finite best-effort turn, never 'safe'.
  reset(); wall(0,-1000,1000); r=def_choose(&s);
  assert(!r.safe && isfinite(r.angle));
  // Persistent preference cannot force an unsafe old heading.
  reset(); wall(130,-1000,1000); s.has_previous=true; s.previous_angle=0;
  assert(def_choose(&s).safe);
  // Regression: previous-heading preference must not lock straight flight.
  for(int sign=-1;sign<=1;sign+=2) {
    reset(); s.has_food=true; s.food=(def_vec){0,sign*200};
    s.has_previous=true; s.previous_angle=0;
    r=def_choose(&s); assert(r.safe && sign*r.angle>0.5f);
  }
  // A small food angle must not fall into the coarse candidate dead zone.
  reset(); s.has_food=true; s.food=(def_vec){200,20};
  s.has_previous=true; s.previous_angle=0;
  assert(def_choose(&s).angle>0.03f);
  // Orbit preference overrides food only when its curved trajectory is safe.
  reset(); s.orbit_direction=1; s.orbit_center=(def_vec){0,120}; s.orbit_radius=120;
  s.has_food=true; s.food=(def_vec){0,-200};
  assert(def_evaluate_orbit(&s).safe);
  assert(fabsf(def_choose(&s).angle)<0.001f);
  wall(110,-100,300);
  assert(!def_evaluate_orbit(&s).safe);
  assert(def_choose(&s).safe);
  s.count=0; assert(fabsf(def_choose(&s).angle)<0.001f); // resume orbit after threat disappears
  // The curved path must also respect the shrinking arena.
  reset(); s.position=(def_vec){900,0}; s.arena_radius=1000;
  s.orbit_direction=1; s.orbit_center=(def_vec){900,120}; s.orbit_radius=120;
  assert(!def_evaluate_orbit(&s).safe); assert(def_choose(&s).safe);
  // Worst retained-obstacle count; report actual planner time, not an FPS claim.
  reset(); for(int i=0;i<512;i++) {
    float a=i*0.0122718463f;
    def_add_obstacle(&s,(def_obstacle){.a={300*cosf(a),300*sinf(a)},.b={310*cosf(a),310*sinf(a)},.radius=15});
  }
  double start=milliseconds(); volatile float sink=0;
  for(int i=0;i<200;i++) sink+=def_choose(&s).angle;
  printf("Defensive planner: 16 scenario groups passed; 512 obstacles, up to 19 paths, 15 steps: %.3f ms/decision (200 runs)\n",(milliseconds()-start)/200);
  return 0;
}
