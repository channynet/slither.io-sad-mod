#include "defensive_planner.h"
#include "body_loop.h"
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
float def_orbit_heading(const def_scene* s,def_vec position) {
  def_vec delta=sub(position,s->orbit_center);
  float error=length(delta)-s->orbit_radius;
  return def_wrap(atan2f(delta.y,delta.x)+s->orbit_direction*
    (DPI/2+atan2f(error,fmaxf(20,s->orbit_radius*0.5f))));
}
static float body_heading(const def_scene* s,def_vec p,float angle,float offset) {
  float lookahead=fmaxf(s->radius*1.5f,s->speed*(0.20f+s->latency));
  return body_loop_heading_offset(s->body_path,p,angle,lookahead,offset);
}
static def_result evaluate_path(const def_scene* s,float target,int mode,float offset) {
  def_result r={.angle=def_wrap(target),.clearance=1000,.safe_seconds=DEF_HORIZON,.safe=true,
                .following_body=mode==2,.body_offset=mode==2?offset:0};
  def_vec p=s->position; float angle=s->angle;
  const float dt=DEF_HORIZON/DEF_STEPS;
  for(int k=1;k<=DEF_STEPS;k++) {
    float time=k*dt;
    float turn_dt=clamp(time-s->latency,0,dt);
    if(mode==1) target=def_orbit_heading(s,p);
    else if(mode==2) target=body_heading(s,p,angle,offset);
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
def_result def_evaluate(const def_scene* s,float target) {
  return evaluate_path(s,target,0,0);
}
def_result def_evaluate_orbit(const def_scene* s) {
  return evaluate_path(s,def_orbit_heading(s,s->position),1,0);
}
def_result def_evaluate_body(const def_scene* s,float offset) {
  if(!s->body_path || s->body_path->count<3) return (def_result){.angle=s->angle};
  return evaluate_path(s,body_heading(s,s->position,s->angle,offset),2,offset);
}
float def_body_outward_offset(const def_scene* s) {
  if(s->body_offset<=0) return s->body_offset;
  // Reserve latency, a decision/update margin and time to turn back inward.
  // Nearby heads may boost or turn toward us even if currently pointing away.
  float reaction=clamp(s->latency,0,.5f)+.12f+DPI/(2*fmaxf(.1f,s->turn_rate));
  float fade=fmaxf(s->radius*2,s->speed*.4f),factor=1;
  for(int i=0;i<s->count;i++) {
    const def_obstacle* o=&s->obstacles[i];
    if(!o->head) continue;
    float gap=length(sub(o->a,s->position))-s->radius-o->radius-8;
    float closing=s->speed+fmaxf(length(o->velocity),s->speed*1.5f);
    factor=fminf(factor,clamp((gap-closing*reaction)/fade,0,1));
  }
  // Overflow cannot turn an omitted nearby threat into permission to expand.
  factor=fminf(factor,clamp((s->omitted_clearance-s->speed*reaction)/fade,0,1));
  return s->body_offset*factor;
}
def_result def_choose(const def_scene* s) {
  // Prefer the captured body lap, then the manual-circle fallback, while safe.
  // Threats and the arena boundary are checked by the same collision evaluator.
  if(s->body_path && s->body_path->count>=3) {
    float outward=def_body_outward_offset(s);
    def_result follow=def_evaluate_body(s,outward);
    // Hold a safe inside detour until the original track has a body-width
    // clearance reserve. This avoids switching tracks on every tiny update.
    if(s->has_previous_body && s->previous_body_offset<fminf(outward,-s->radius*.25f) &&
       follow.clearance<s->radius*2) {
      def_result held=def_evaluate_body(s,s->previous_body_offset);
      if(held.safe) return held;
    }
    if(follow.safe && (outward<=0 || follow.clearance>s->radius*2)) return follow;
    // Before leaving the enclosure, try the current spine and its inside edge.
    def_result spine=def_evaluate_body(s,0);
    if(spine.safe) return spine;
    // Follow the enclosure on progressively deeper inside tracks before
    // abandoning it. Every candidate includes latency, threats and the border.
    // The common safe path returns above, so it pays no extra search cost.
    const float inward[]={.5f,1,2,3};
    for(int i=0;i<4;i++) {
      float offset=-fminf(s->radius*inward[i],s->body_path->length*.04f);
      def_result inside=def_evaluate_body(s,offset);
      if(inside.safe) return inside;
    }
    if(follow.safe) return follow;
  } else if(s->orbit_direction) {
    def_result orbit=def_evaluate_orbit(s);
    if(orbit.safe) return orbit;
  }
  def_result best={0}; float best_score=-FLT_MAX;
  float food_angle=s->has_food?atan2f(s->food.y-s->position.y,s->food.x-s->position.x):s->angle;
  float return_angle=s->body_path?body_heading(s,s->position,s->angle,0):s->angle;
  int candidates=DEF_CANDIDATES+(s->has_food?1:0)+(s->has_previous?1:0);
  for(int i=0;i<candidates;i++) {
    // Include straight ahead, gentle steering, and both near-U-turns.
    float offset=(i-8)*(DPI/8.5f);
    float target=s->angle+offset;
    if(i>=DEF_CANDIDATES) {
      target=(s->has_food && i==DEF_CANDIDATES)?food_angle:s->previous_angle;
      offset=def_wrap(target-s->angle);
    }
    def_result r=def_evaluate(s,target);
    float score=(r.safe?100000:0)+r.safe_seconds*10000+clamp(r.clearance,-100,80)*2;
    // Linear turn penalties previously exceeded the maximum food reward slope,
    // so straight ahead always won in open space. Quadratic penalties allow
    // small corrections while still discouraging abrupt left/right reversals.
    score-=offset*offset*2;
    if(s->has_previous) {
      float change=def_wrap(target-s->previous_angle);
      score-=change*change*1.5f;
    }
    if(!s->body_path && !s->orbit_direction && s->has_food && r.safe && r.clearance>20) {
      score+=cosf(def_wrap(target-food_angle))*48;
    }
    if(s->body_path && r.safe) {
      float return_turn=def_wrap(target-return_angle);
      score-=return_turn*return_turn*4; // Prefer a safe route back to our lap.
    }
    if(score>best_score) { best_score=score; best=r; }
  }
  return best;
}
