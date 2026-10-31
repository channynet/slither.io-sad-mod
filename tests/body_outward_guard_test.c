#include "../app/src/game/body_loop.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#define PI 3.14159265358979323846f
static def_scene scene;
static def_vec trail[300];
static body_loop path;
int main(void) {
  def_scene_reset(&scene); scene.position=(def_vec){170,0}; scene.angle=PI/2;
  scene.radius=14.5f; scene.speed=180; scene.turn_rate=3; scene.latency=.13f; scene.arena_radius=10000; scene.body_offset=4;
  assert(def_body_outward_offset(&scene)==4);
  float last=4;
  // A head closing from outside suppresses outward lean before direct contact.
  for(int gap=1000;gap>=200;gap-=10) {
    scene.count=0;
    def_add_obstacle(&scene,(def_obstacle){.a={170+gap,0},.b={170+gap,0},.radius=15,.head=true,.velocity={-270,0}});
    float offset=def_body_outward_offset(&scene);
    assert(offset>=0 && offset<=last+.0001f); last=offset;
  }
  assert(last==0);
  scene.obstacles[0].a=scene.obstacles[0].b=(def_vec){570,0};
  scene.latency=.08f; float quick=def_body_outward_offset(&scene);
  scene.latency=.3f; assert(def_body_outward_offset(&scene)<=quick);
  scene.latency=.08f; scene.obstacles[0].velocity=(def_vec){-500,0};
  assert(def_body_outward_offset(&scene)<=quick);
  scene.obstacles[0].velocity=(def_vec){270,0};
  scene.obstacles[0].a=scene.obstacles[0].b=(def_vec){370,0};
  assert(def_body_outward_offset(&scene)==0); // Can turn/boost back toward us.
  scene.count=0; scene.omitted_clearance=20; assert(def_body_outward_offset(&scene)==0);
  scene.body_offset=-4; assert(def_body_outward_offset(&scene)==-4);
  for(int i=0;i<300;i++) {
    float a=-i*2*PI/240; trail[i]=(def_vec){170*cosf(a),170*sinf(a)};
  }
  assert(body_loop_capture(&path,trail,300,PI/2,14.5f,60));
  def_scene_reset(&scene); scene.position=trail[0]; scene.angle=PI/2;
  scene.radius=14.5f; scene.speed=180; scene.turn_rate=3; scene.latency=.13f;
  scene.arena_radius=10000; scene.body_path=&path; scene.body_offset=4;
  def_result r=def_choose(&scene); assert(r.safe && r.following_body && r.body_offset==4);
  def_add_obstacle(&scene,(def_obstacle){.a={1170,0},.b={1170,0},.radius=15,.head=true,.velocity={-270,0}});
  assert(def_body_outward_offset(&scene)==4);
  scene.turn_rate=.5f; assert(def_body_outward_offset(&scene)==0); // Slow turns need more lead time.
  scene.turn_rate=3; scene.count=0;
  def_add_obstacle(&scene,(def_obstacle){.a={500,0},.b={500,0},.radius=15,.head=true,.velocity={-270,0}});
  r=def_choose(&scene); assert(r.safe && r.following_body && r.body_offset==0);
  scene.count=0; // Once the threat has left, regain size without a new toggle.
  r=def_choose(&scene); assert(r.safe && r.following_body && r.body_offset==4);
  puts("Outward guard: no threat, approaching/turning/boosting heads, latency, overflow and inward escape passed");
}
