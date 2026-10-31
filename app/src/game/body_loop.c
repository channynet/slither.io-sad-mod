#include "body_loop.h"
#include <math.h>
#include <float.h>
#include <string.h>
#define PI 3.14159265358979323846f
static def_vec sub(def_vec a,def_vec b) { return (def_vec){a.x-b.x,a.y-b.y}; }
static float dot(def_vec a,def_vec b) { return a.x*b.x+a.y*b.y; }
static float norm(def_vec a) { return sqrtf(dot(a,a)); }
static def_vec mix(def_vec a,def_vec b,float t) { return (def_vec){a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t}; }
static float projection(def_vec p,def_vec a,def_vec b) {
  def_vec d=sub(b,a); float n=dot(d,d);
  return n>0.0001f?fminf(1,fmaxf(0,dot(sub(p,a),d)/n)):0;
}
bool body_loop_capture(body_loop* loop,const def_vec* trail,int count,float heading,float radius,float minimum_radius) {
  memset(loop,0,sizeof(*loop));
  if(!trail || count<8 || count>BODY_LOOP_SCAN || !isfinite(heading) ||
     !isfinite(radius) || !isfinite(minimum_radius) || radius<=0 || minimum_radius<=0) return false;
  // A damaged tail after a complete lap must not discard the good enclosure.
  // Stop at the first gap: never bridge missing data or count it as overlap.
  float body_length=0; int contiguous=1;
  for(int i=1;i<count;i++) {
    float distance=norm(sub(trail[i],trail[i-1]));
    if(!isfinite(distance) || distance>fmaxf(100,radius*8)) break;
    body_length+=distance; contiguous++;
  }
  count=contiguous; if(count<8) return false;
  float arc=0,turn=0,absolute_turn=0,previous=heading,best=FLT_MAX;
  float turn_sample_arc=0;
  def_vec turn_anchor=trail[0];
  float near=fmaxf(radius*3,minimum_radius*0.6f),best_fraction=0;
  int end=-1;
  for(int i=1;i<count;i++) {
    def_vec forward=sub(trail[i-1],trail[i]); float distance=norm(forward);
    if(!isfinite(distance)) return false;
    if(distance<0.01f) continue;
    // Discontinuous body data must not be joined into an invented enclosure.
    if(distance>fmaxf(100,radius*8)) break;
    arc+=distance;
    // Measure lap winding over body-width baselines, not tiny individual
    // segments whose coordinate jitter can look like repeated reverse turns.
    float angle=previous;
    if(arc-turn_sample_arc>=fmaxf(radius*2,minimum_radius*.35f)) {
      def_vec tangent=sub(turn_anchor,trail[i]);
      angle=atan2f(tangent.y,tangent.x);
      float delta=def_wrap(angle-previous);
      turn+=delta; absolute_turn+=fabsf(delta); previous=angle;
      turn_anchor=trail[i]; turn_sample_arc=arc;
    }
    if(fabsf(turn)>2.6f*PI) break; // use the nearest lap, not several coils
    if(arc<fmaxf(radius*12,minimum_radius*4) || fabsf(turn)<1.5f*PI ||
       fabsf(turn)<absolute_turn*0.65f || cosf(angle-heading)<0.6f) continue;
    float t=projection(trail[0],trail[i],trail[i-1]);
    def_vec q=mix(trail[i],trail[i-1],t);
    float gap=norm(sub(q,trail[0]));
    if(gap>near) continue;
    float score=gap+fabsf(fabsf(turn)-2*PI)*radius;
    if(score<best) { best=score; end=i; best_fraction=t; }
  }
  if(end<0) return false;
  def_vec start=mix(trail[end],trail[end-1],best_fraction);
  float total=norm(sub(start,trail[end-1]))+norm(sub(trail[0],start));
  for(int i=end-1;i>0;i--) total+=norm(sub(trail[i],trail[i-1]));
  if(!isfinite(total) || total<=0) return false;
  // Uniform arc-length sampling bounds all later steering/prediction work.
  float step=total/BODY_LOOP_POINTS,walked=0,next=0; int output=0;
  def_vec a=start;
  for(int i=end-1;i>=-1 && output<BODY_LOOP_POINTS;i--) {
    def_vec b=i>=0?trail[i]:start; float distance=norm(sub(b,a));
    if(distance>0.0001f) while(output<BODY_LOOP_POINTS && next<walked+distance) {
      loop->points[output++]=mix(a,b,(next-walked)/distance); next=output*step;
    }
    walked+=distance; a=b;
  }
  if(output!=BODY_LOOP_POINTS) return false;
  float area=0;
  for(int i=0;i<output;i++) {
    def_vec a=loop->points[i],b=loop->points[(i+1)%output];
    // Translate first: map coordinates can be much larger than a small coil.
    a=sub(a,start); b=sub(b,start); area+=a.x*b.y-a.y*b.x;
  }
  if(fabsf(area)<radius*radius) return false;
  loop->count=output; loop->length=total; loop->body_length=body_length;
  loop->overlap=body_length-total; loop->spacing=body_length/(count-1);
  loop->direction=area>0?1:-1;
  return true;
}
bool body_loop_capture_follow(body_loop* loop,const def_vec* trail,int count,float heading,float radius,float minimum_radius) {
  memset(loop,0,sizeof(*loop));
  if(!trail || count<8 || count>BODY_LOOP_SCAN || !isfinite(heading) ||
     !isfinite(radius) || !isfinite(minimum_radius) || radius<=0 || minimum_radius<=0) return false;
  // Exclude the neck: turning back toward our own just-created trail is not
  // following the older body ahead. Keep enough forward body for prediction.
  float skip=fmaxf(radius*8,minimum_radius*2),arc=0,best=FLT_MAX;
  // NTL follows the nearest older body before the coil has fully closed.
  // Allow a reachable approach (up to six body widths) instead of requiring
  // the head to already be almost touching the previous lap. Alignment,
  // neck exclusion and the collision planner still gate the join.
  float near=fmaxf(radius*12,minimum_radius*2);
  def_vec facing={cosf(heading),sinf(heading)};
  int neck=-1,end=-1,valid=1;
  for(int i=1;i<count;i++) {
    def_vec d=sub(trail[i-1],trail[i]); float distance=norm(d);
    if(!isfinite(distance) || distance>fmaxf(100,radius*8)) break;
    arc+=distance; valid=i+1;
    if(neck<0 && arc>=skip) neck=i;
    if(neck<0 || arc<skip+fmaxf(radius*4,minimum_radius)) continue;
    float alignment=dot(d,facing)/fmaxf(distance,.001f);
    if(alignment<.35f) continue;
    def_vec q=mix(trail[i],trail[i-1],projection(trail[0],trail[i],trail[i-1]));
    def_vec delta=sub(q,trail[0]); float gap=norm(delta);
    if(gap>near || dot(delta,facing)<-radius*2) continue;
    float score=gap+(1-alignment)*radius*2;
    if(score<best) { best=score; end=i; }
  }
  if(end<0 || neck<0) return false;
  // A short portion behind the join point prevents the moving tail endpoint
  // from constantly becoming our only steering target. No invented closure.
  end=end+3<valid?end+3:valid-1;
  float total=0;
  for(int i=end;i>neck;i--) total+=norm(sub(trail[i-1],trail[i]));
  if(total<fmaxf(radius*4,minimum_radius)) return false;
  float step=total/(BODY_LOOP_POINTS-1),walked=0,next=0; int output=0;
  for(int i=end;i>neck && output<BODY_LOOP_POINTS-1;i--) {
    def_vec a=trail[i],b=trail[i-1]; float distance=norm(sub(b,a));
    if(distance>.0001f) while(output<BODY_LOOP_POINTS-1 && next<walked+distance) {
      loop->points[output++]=mix(a,b,(next-walked)/distance); next=output*step;
    }
    walked+=distance;
  }
  if(output!=BODY_LOOP_POINTS-1) return false;
  loop->points[output++]=trail[neck];
  float area=0;
  for(int i=1;i<count && i<=end;i++) {
    def_vec a=sub(trail[i],trail[0]),b=sub(trail[i-1],trail[0]); area+=a.x*b.y-a.y*b.x;
  }
  loop->count=output; loop->length=total; loop->body_length=arc;
  loop->spacing=arc/fmaxf(1,valid-1); loop->direction=area>=0?1:-1; loop->open=true;
  return true;
}
float body_loop_offset(const body_loop* loop,float target_length,float radius,float speed,float latency) {
  if(!loop || loop->count<3 || !loop->direction || loop->open) return 0;
  // Keep at least four body samples and a latency reserve overlapping the seam.
  float reserve=fmaxf(fmaxf(radius*4,loop->spacing*4),speed*(latency+.2f));
  float overlap_error=loop->overlap-reserve;
  // Aim at the captured lap size, not the largest loop our full body can fill.
  // Looking ahead along a curved body cuts the chord inward. A small outward
  // feed-forward term counters this, with circumference feedback bounding drift.
  float lookahead=fmaxf(radius*1.5f,speed*(.20f+latency));
  float chord=lookahead*lookahead*PI/fmaxf(loop->length,1)*.25f;
  float target=fminf(target_length,loop->length+overlap_error);
  float offset=chord+(target-loop->length)/(2*PI)*.7f;
  if(overlap_error<0) offset=fminf(offset,overlap_error/(2*PI)*.5f);
  return fminf(radius*.35f,fmaxf(-radius*.5f,offset));
}
float body_loop_distance(const body_loop* loop,def_vec position) {
  if(!loop || loop->count<3) return FLT_MAX;
  float best=FLT_MAX;
  for(int i=0;i<loop->count-(loop->open?1:0);i++) {
    def_vec a=loop->points[i],b=loop->points[(i+1)%loop->count];
    def_vec delta=sub(position,mix(a,b,projection(position,a,b)));
    best=fminf(best,dot(delta,delta));
  }
  return sqrtf(best);
}
static def_vec nearest_aligned(const body_loop* loop,def_vec position,float heading,def_vec* normal) {
  float best=FLT_MAX; def_vec result=position,facing={cosf(heading),sinf(heading)};
  for(int i=0;i<loop->count;i++) {
    def_vec a=loop->points[i],b=loop->points[(i+1)%loop->count],d=sub(b,a);
    float len=norm(d); if(len<.001f) continue;
    def_vec q=mix(a,b,projection(position,a,b));
    float score=norm(sub(q,position))+fmaxf(0,-dot(d,facing)/len)*loop->length*.1f;
    if(score<best) {
      best=score; result=q;
      *normal=(def_vec){loop->direction*d.y/len,-loop->direction*d.x/len};
    }
  }
  return result;
}
float body_loop_rebase_inward(const body_loop* old,const body_loop* next,def_vec position,float heading,float offset) {
  if(offset>=0 || !old || !next || old->open || next->open || old->count<3 || next->count<3 ||
     old->direction!=next->direction) return 0;
  def_vec normal={0},unused={0};
  def_vec before=nearest_aligned(old,position,heading,&unused);
  def_vec after=nearest_aligned(next,position,heading,&normal);
  // Keep the previous detour at its world-space distance. The live body moves
  // inward too: applying the full inset again to every new lap causes spiraling.
  return fminf(0,offset+dot(sub(before,after),normal));
}
float body_loop_heading_offset(const body_loop* loop,def_vec position,float heading,float lookahead,float offset) {
  if(!loop || loop->count<3) return heading;
  int nearest=0; float fraction=0,best=FLT_MAX;
  def_vec facing={cosf(heading),sinf(heading)};
  for(int i=0;i<loop->count-(loop->open?1:0);i++) {
    def_vec a=loop->points[i],b=loop->points[(i+1)%loop->count],d=sub(b,a);
    float t=projection(position,a,b),distance=norm(sub(position,mix(a,b,t)));
    // At adjacent coils/crossings prefer the segment travelling our way.
    float alignment=dot(d,facing)/fmaxf(norm(d),0.001f);
    float score=distance+fmaxf(0,-alignment)*lookahead*2;
    // An open route ends at the neck behind us. Pure nearest-point selection
    // can pick that endpoint instead of the older body ahead and steer away.
    // Penalize targets behind the head; keep the forward joining branch.
    if(loop->open) score+=fmaxf(0,-dot(sub(mix(a,b,t),position),facing))*2;
    if(score<best) { nearest=i; fraction=t; best=score; }
  }
  float remaining=fminf(fmaxf(1,lookahead),loop->length*0.2f);
  def_vec a=mix(loop->points[nearest],loop->points[(nearest+1)%loop->count],fraction);
  for(int j=0;j<=loop->count;j++) {
    if(loop->open && nearest+j+1>=loop->count) {
      def_vec tangent=sub(loop->points[loop->count-1],loop->points[loop->count-2]);
      return atan2f(tangent.y,tangent.x);
    }
    def_vec b=loop->points[(nearest+j+1)%loop->count]; float distance=norm(sub(b,a));
    if(distance>=remaining && distance>0) {
      def_vec target=mix(a,b,remaining/distance);
      def_vec tangent=sub(b,a);
      target.x+=loop->direction*tangent.y/distance*offset;
      target.y-=loop->direction*tangent.x/distance*offset;
      return atan2f(target.y-position.y,target.x-position.x);
    }
    remaining-=distance; a=b;
  }
  return heading;
}
float body_loop_heading(const body_loop* loop,def_vec position,float heading,float lookahead) {
  return body_loop_heading_offset(loop,position,heading,lookahead,0);
}
