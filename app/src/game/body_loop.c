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
  if(!trail || count<8 || count>BODY_LOOP_SCAN || !isfinite(heading) || radius<=0 || minimum_radius<=0) return false;
  float arc=0,turn=0,absolute_turn=0,previous=heading,best=FLT_MAX;
  float near=fmaxf(radius*3,minimum_radius*0.6f),best_fraction=0;
  int end=-1;
  for(int i=1;i<count;i++) {
    def_vec forward=sub(trail[i-1],trail[i]); float distance=norm(forward);
    if(!isfinite(distance)) return false;
    if(distance<0.01f) continue;
    // Discontinuous body data must not be joined into an invented enclosure.
    if(distance>fmaxf(100,radius*8)) break;
    arc+=distance;
    float angle=atan2f(forward.y,forward.x),delta=def_wrap(angle-previous);
    turn+=delta; absolute_turn+=fabsf(delta); previous=angle;
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
  loop->count=output; loop->length=total; return true;
}
float body_loop_heading(const body_loop* loop,def_vec position,float heading,float lookahead) {
  if(!loop || loop->count<3) return heading;
  int nearest=0; float fraction=0,best=FLT_MAX;
  def_vec facing={cosf(heading),sinf(heading)};
  for(int i=0;i<loop->count;i++) {
    def_vec a=loop->points[i],b=loop->points[(i+1)%loop->count],d=sub(b,a);
    float t=projection(position,a,b),distance=norm(sub(position,mix(a,b,t)));
    // At adjacent coils/crossings prefer the segment travelling our way.
    float alignment=dot(d,facing)/fmaxf(norm(d),0.001f);
    float score=distance+fmaxf(0,-alignment)*lookahead*2;
    if(score<best) { nearest=i; fraction=t; best=score; }
  }
  float remaining=fminf(fmaxf(1,lookahead),loop->length*0.2f);
  def_vec a=mix(loop->points[nearest],loop->points[(nearest+1)%loop->count],fraction);
  for(int j=0;j<=loop->count;j++) {
    def_vec b=loop->points[(nearest+j+1)%loop->count]; float distance=norm(sub(b,a));
    if(distance>=remaining && distance>0) {
      def_vec target=mix(a,b,remaining/distance);
      return atan2f(target.y-position.y,target.x-position.x);
    }
    remaining-=distance; a=b;
  }
  return heading;
}
