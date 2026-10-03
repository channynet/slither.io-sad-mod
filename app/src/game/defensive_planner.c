#include "defensive_planner.h"
#include <math.h>
#include <float.h>
#include <string.h>
#define DPI 3.14159265358979323846f
static float clamp(float x,float a,float b) { return fmaxf(a,fminf(x,b)); }
static def_vec sub(def_vec a,def_vec b) { return (def_vec){a.x-b.x,a.y-b.y}; }
static float dot(def_vec a,def_vec b) { return a.x*b.x+a.y*b.y; }
static float length(def_vec a) { return sqrtf(dot(a,a)); }
float def_wrap(float a) { return remainderf(a,2*DPI); }
static float point_segment_squared(def_vec p,def_vec a,def_vec b) {
  def_vec d=sub(b,a),v=sub(p,a); float n=dot(d,d);
  float t=n>0.0001f?clamp(dot(v,d)/n,0,1):0;
  def_vec delta={v.x-t*d.x,v.y-t*d.y}; return dot(delta,delta);
}
float def_point_segment(def_vec p,def_vec a,def_vec b) {
  return sqrtf(point_segment_squared(p,a,b));
}
static float cross(def_vec a,def_vec b) { return a.x*b.y-a.y*b.x; }
static float segment_distance(def_vec a,def_vec b,def_vec c,def_vec d) {
  def_vec u=sub(b,a),v=sub(d,c),w=sub(c,a); float den=cross(u,v);
  if(fabsf(den)>0.0001f) {
    float t=cross(w,v)/den,s=cross(w,u)/den;
    if(t>=0 && t<=1 && s>=0 && s<=1) return 0;
  }
  return sqrtf(fminf(fminf(point_segment_squared(a,c,d),point_segment_squared(b,c,d)),
               fminf(point_segment_squared(c,a,b),point_segment_squared(d,a,b))));
}
void def_scene_reset(def_scene* s) { memset(s,0,sizeof(*s)); s->omitted_clearance=FLT_MAX; }
// Keep the closest threats in a max heap. O(log N), no per-frame allocation.
// Discarded threats still constrain the reachable area; overflow is never open space.
void def_add_obstacle(def_scene* s,def_obstacle o) {
  o.priority=def_point_segment(s->position,o.a,o.b)-o.radius-s->radius-8-s->speed*s->turn_rate*0.01f/8;
  if(o.head) o.priority-=length(o.velocity)*(DEF_HORIZON+0.075f);
  int i;
  if(s->count<DEF_OBSTACLES) {
    i=s->count++; s->obstacles[i]=o;
    while(i>0) { int parent=(i-1)/2;
      if(s->obstacles[parent].priority>=o.priority) break;
      s->obstacles[i]=s->obstacles[parent]; i=parent;
    }
    s->obstacles[i]=o; return;
  }
  if(o.priority>=s->obstacles[0].priority) {
    s->omitted_clearance=fminf(s->omitted_clearance,o.priority); return;
  }
  s->omitted_clearance=fminf(s->omitted_clearance,s->obstacles[0].priority);
  i=0;
  while(i*2+1<s->count) {
    int child=i*2+1;
    if(child+1<s->count && s->obstacles[child+1].priority>s->obstacles[child].priority) child++;
    if(s->obstacles[child].priority<=o.priority) break;
    s->obstacles[i]=s->obstacles[child]; i=child;
  }
  s->obstacles[i]=o;
}
def_result def_evaluate(const def_scene* s,float target) {
  def_result r={.angle=def_wrap(target),.clearance=1000,.safe_seconds=DEF_HORIZON,.safe=true};
  def_vec p=s->position; float angle=s->angle;
  const float dt=DEF_HORIZON/DEF_STEPS;
  for(int k=1;k<=DEF_STEPS;k++) {
    float time=k*dt;
    float turn_dt=clamp(time-s->latency,0,dt);
    angle+=clamp(def_wrap(target-angle),-s->turn_rate*turn_dt,s->turn_rate*turn_dt);
    def_vec q={p.x+cosf(angle)*s->speed*dt,p.y+sinf(angle)*s->speed*dt};
    // Chord deviation bound covers the curved motion between samples.
    float margin=s->radius+8+s->speed*s->turn_rate*dt*dt/8;
    float clearance=s->arena_radius-fmaxf(length(sub(p,s->center)),length(sub(q,s->center)))-margin;
    clearance=fminf(clearance,s->omitted_clearance-length(sub(q,s->position)));
    for(int j=0;j<s->count;j++) {
      const def_obstacle* o=&s->obstacles[j];
      def_vec end=o->b; float uncertainty=0;
      if(o->head) {
        // The head leaves a lethal body behind: test its entire predicted sweep.
        end=(def_vec){o->a.x+o->velocity.x*time,o->a.y+o->velocity.y*time};
        uncertainty=length(o->velocity)*fminf(time,0.3f)*0.25f;
      }
      float reach=margin+o->radius+uncertainty+fminf(clearance,80);
      if(fmaxf(p.x,q.x)+reach<fminf(o->a.x,end.x) ||
         fminf(p.x,q.x)-reach>fmaxf(o->a.x,end.x) ||
         fmaxf(p.y,q.y)+reach<fminf(o->a.y,end.y) ||
         fminf(p.y,q.y)-reach>fmaxf(o->a.y,end.y)) continue;
      float c=segment_distance(p,q,o->a,end)-margin-o->radius-uncertainty;
      clearance=fminf(clearance,c);
    }
    r.clearance=fminf(r.clearance,clearance);
    if(clearance<=0) { r.safe=false; r.safe_seconds=(k-1)*dt; return r; }
    p=q;
  }
  return r;
}
def_result def_choose(const def_scene* s) {
  def_result best={0}; float best_score=-FLT_MAX;
  for(int i=0;i<DEF_CANDIDATES;i++) {
    // Include straight ahead, gentle steering, and both near-U-turns.
    float offset=(i-8)*(DPI/8.5f);
    float target=s->angle+offset;
    def_result r=def_evaluate(s,target);
    float score=(r.safe?100000:0)+r.safe_seconds*10000+clamp(r.clearance,-100,80)*2;
    score-=fabsf(offset)*8;
    if(s->has_previous) score-=fabsf(def_wrap(target-s->previous_angle))*12;
    if(s->has_food && r.safe && r.clearance>20) {
      float food_angle=atan2f(s->food.y-s->position.y,s->food.x-s->position.x);
      score+=cosf(def_wrap(target-food_angle))*12;
    }
    if(score>best_score) { best_score=score; best=r; }
  }
  return best;
}
