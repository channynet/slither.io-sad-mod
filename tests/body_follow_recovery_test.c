#include "../app/src/game/body_loop.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#define PI 3.14159265358979323846f
static def_vec trail[300];
static body_loop path,shorter;
static def_scene scene;
static void ellipse(int sign,bool jitter) {
  for(int i=0;i<300;i++) {
    float a=-sign*i*2*PI/240,noise=jitter?1.5f*sinf(i*2.1f):0;
    trail[i]=(def_vec){(220+noise)*cosf(a),(140+noise)*sinf(a)};
  }
}
int main(void) {
  for(int sign=-1;sign<=1;sign+=2) {
    ellipse(sign,true);
    assert(body_loop_capture(&path,trail,300,sign*PI/2,14.5f,60));
    assert(path.direction==sign);
    ellipse(sign,false);
    assert(body_loop_capture(&shorter,trail,280,sign*PI/2,14.5f,60));
    trail[280].x=NAN;
    assert(body_loop_capture(&path,trail,300,sign*PI/2,14.5f,60));
    assert(fabsf(path.length-shorter.length)<.001f);
    assert(fabsf(path.overlap-shorter.overlap)<.001f);
    assert(body_loop_distance(&path,trail[0])<1);
    trail[100].x=NAN; // A gap before the lap still cannot invent an enclosure.
    assert(!body_loop_capture(&path,trail,300,sign*PI/2,14.5f,60));
  }
  ellipse(1,false); assert(body_loop_capture(&path,trail,300,PI/2,14.5f,60));
  // Live body refresh must not repeatedly subtract the same inward detour.
  // Keep a fixed world-space target while the recorded ring moves inward.
  for(int sign=-1;sign<=1;sign+=2) {
    body_loop before={.count=128,.direction=sign,.length=2*PI*170},after=before;
    for(int i=0;i<128;i++) {
      float a=sign*i*2*PI/128;
      before.points[i]=(def_vec){20000+170*cosf(a),20000+170*sinf(a)};
    }
    float offset=-29;
    for(int radius=168;radius>=140;radius-=2) {
      after.length=2*PI*radius;
      for(int i=0;i<128;i++) {
        float a=sign*i*2*PI/128;
        after.points[i]=(def_vec){20000+radius*cosf(a),20000+radius*sinf(a)};
      }
      offset=body_loop_rebase_inward(&before,&after,(def_vec){20141,20000},sign*PI/2,offset);
      assert(fabsf(radius+offset-fminf(141,radius))<.1f);
      before=after;
    }
    assert(offset==0); // Once the inset has been reached, no further inward bias.
    after.open=true;
    assert(body_loop_rebase_inward(&before,&after,(def_vec){20140,20000},sign*PI/2,-29)==0);
  }
  int inside_cases=0,held_cases=0;
  // Sweep a short external wall across the upcoming arc. Previously these
  // situations left the body route after trying only a half-radius inset.
  for(int x=100;x<=260;x+=10) for(int y=80;y<=220;y+=10) {
    def_scene_reset(&scene); scene.body_path=&path; scene.position=trail[0];
    scene.angle=PI/2; scene.radius=14.5f; scene.speed=180; scene.turn_rate=3;
    scene.latency=.13f; scene.arena_radius=10000;
    def_add_obstacle(&scene,(def_obstacle){.a={x,y},.b={x+140,y},.radius=15});
    def_result chosen=def_choose(&scene);
    if(!def_evaluate_body(&scene,0).safe && !def_evaluate_body(&scene,-7.25f).safe &&
       chosen.safe && chosen.following_body && chosen.body_offset<-7.25f) {
      inside_cases++;
    }
    def_result spine=def_evaluate_body(&scene,0),inside=def_evaluate_body(&scene,-29);
    if(spine.safe && spine.clearance<29 && inside.safe) {
      scene.has_previous_body=true; scene.previous_body_offset=-29;
      chosen=def_choose(&scene);
      assert(chosen.safe && chosen.following_body && chosen.body_offset==-29); held_cases++;
      scene.count=0; chosen=def_choose(&scene);
      assert(chosen.safe && chosen.following_body && chosen.body_offset==0);
    }
  }
  printf("Body recovery: live inward-detour rebasing, jitter and truncated tail in both directions; %d deeper inside detours, %d stable detour/rejoin cases\n",inside_cases,held_cases);
  assert(inside_cases>0 && held_cases>0);
}
