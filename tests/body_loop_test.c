#include "../app/src/game/body_loop.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <windows.h>
#define PI 3.14159265358979323846f
static def_vec trail[400];
static body_loop loop;
static def_scene scene;
static float clamp(float x,float low,float high) { return fmaxf(low,fminf(high,x)); }
static double milliseconds(void) { LARGE_INTEGER f,t; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t); return t.QuadPart*1000.0/f.QuadPart; }
static void ellipse(int sign,float phase) {
  for(int i=0;i<300;i++) {
    float a=phase-sign*i*2*PI/240;
    trail[i]=(def_vec){220*cosf(a),140*sinf(a)};
  }
}
int main(void) {
  for(int sign=-1;sign<=1;sign+=2) for(int phase=0;phase<4;phase++) {
    float a=phase*PI/2,heading=atan2f(sign*140*cosf(a),-sign*220*sinf(a));
    ellipse(sign,a);
    assert(body_loop_capture(&loop,trail,300,heading,14.5f,60));
    assert(loop.count==128 && loop.length>1100 && loop.length<1200);
    assert(loop.direction==sign && loop.overlap>200 && loop.body_length>loop.length);
    assert(body_loop_offset(&loop,14.5f,180,.1f)>0);
    body_loop short_tail=loop; short_tail.overlap=10;
    assert(body_loop_offset(&short_tail,14.5f,180,.1f)<0);
    def_scene_reset(&scene); scene.body_path=&loop; scene.position=trail[0]; scene.angle=heading;
    scene.radius=14.5f; scene.speed=180; scene.turn_rate=3; scene.latency=.1f; scene.arena_radius=10000;
    // Food outside the enclosure must not pull a safe body-following bot out.
    scene.has_food=true; scene.food=(def_vec){1000,1000};
    float turn=0,max_error=0;
    for(int i=0;i<1200;i++) {
      def_result r=def_choose(&scene); assert(r.safe && isfinite(r.angle));
      float delta=clamp(def_wrap(r.angle-scene.angle),-.12f,.12f);
      scene.position.x+=cosf(scene.angle+delta/2)*7.2f;
      scene.position.y+=sinf(scene.angle+delta/2)*7.2f;
      scene.angle+=delta; turn+=delta;
      float error=fabsf(hypotf(scene.position.x/220,scene.position.y/140)-1);
      max_error=fmaxf(max_error,error);
    }
    assert(sign*turn>35 && max_error<.12f);
  }
  // Partial turns, straight trails, a nearby neck, opposite heading and broken data
  // cannot create a false full-lap capture.
  ellipse(1,0); assert(!body_loop_capture(&loop,trail,80,PI/2,14.5f,60));
  assert(!body_loop_capture(&loop,trail,300,-PI/2,14.5f,60));
  for(int i=0;i<300;i++) trail[i]=(def_vec){-(float)i*4,0};
  assert(!body_loop_capture(&loop,trail,300,0,14.5f,60));
  ellipse(1,0); trail[20].x=NAN;
  assert(!body_loop_capture(&loop,trail,300,PI/2,14.5f,60));
  ellipse(1,0); trail[20].x=5000;
  assert(!body_loop_capture(&loop,trail,300,PI/2,14.5f,60));
  ellipse(1,0); assert(body_loop_capture(&loop,trail,300,PI/2,14.5f,60));
  def_scene_reset(&scene); scene.body_path=&loop; scene.position=trail[0]; scene.angle=PI/2;
  scene.radius=14.5f; scene.speed=180; scene.turn_rate=3; scene.latency=.1f; scene.arena_radius=10000;
  float preferred=def_choose(&scene).angle;
  def_add_obstacle(&scene,(def_obstacle){.a={80,130},.b={340,130},.radius=15});
  def_result avoid=def_choose(&scene); assert(avoid.safe && fabsf(def_wrap(avoid.angle-preferred))>.2f);
  scene.count=0; assert(fabsf(def_wrap(def_choose(&scene).angle-preferred))<.001f);
  // An outside threat suppresses expansion while a safe spine path remains.
  def_add_obstacle(&scene,(def_obstacle){.a={260,-1000},.b={260,1000},.radius=15});
  def_result spine=def_choose(&scene); assert(spine.safe);
  scene.body_offset=5;
  def_result restrained=def_choose(&scene);
  assert(restrained.safe && fabsf(def_wrap(restrained.angle-spine.angle))<.001f);
  scene.count=0;
  assert(fabsf(def_wrap(def_choose(&scene).angle-preferred))>.01f);
  scene.body_offset=0;
  // The same captured route must not override a shrinking arena edge.
  scene.arena_radius=170; assert(!def_choose(&scene).safe);
  scene.arena_radius=10000;
  for(int i=0;i<512;i++) {
    float a=i*2*PI/512;
    def_add_obstacle(&scene,(def_obstacle){.a={500*cosf(a),500*sinf(a)},.b={510*cosf(a),510*sinf(a)},.radius=15});
  }
  double start=milliseconds(); volatile float sink=0;
  for(int i=0;i<200;i++) sink+=def_choose(&scene).angle;
  printf("Body loop: 8 oval simulations, 48 s each, both directions; partial/straight/opposite/broken rejection; threats and boundary passed. 512 obstacles + 128 route points: %.3f ms/decision\n",(milliseconds()-start)/200);
  return 0;
}
