#ifndef TAG_ATTACHMENT_H
#define TAG_ATTACHMENT_H
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
typedef struct { float direction; double time; uint64_t generation; bool valid; } tag_attachment;
typedef struct {
  float angle,velocity,heading,phase;
  double time;
  uint64_t generation;
  bool valid;
} tag_sway;
// Damped pendulum around the attachment point; turning adds angular inertia.
// Exact spring integration avoids a frame-rate-dependent stiffness or instability.
static inline float tag_sway_update(tag_sway* s,float heading,double now,uint64_t generation) {
  double elapsed=(now-s->time)*.001;
  if(!s->valid || s->generation!=generation || elapsed<0 || elapsed>.25) {
    *s=(tag_sway){.heading=heading,.time=now,.generation=generation,.valid=true};
    return 0;
  }
  if(elapsed<=0) return s->angle;
  float dt=(float)elapsed;
  float turn=atan2f(sinf(heading-s->heading),cosf(heading-s->heading));
  float impulse=fmaxf(-1.5f,fminf(1.5f,-3.f*turn));
  float phase=s->phase+dt*.5f;
  float target=.10f*sinf(phase*1.8f)+.04f*sinf(phase*2.9f);
  float damping=2.5f,frequency=sqrtf(24.f-damping*damping);
  float decay=expf(-damping*dt),c=cosf(frequency*dt),sn=sinf(frequency*dt);
  float offset=s->angle-target,v=s->velocity+impulse*.5f;
  s->angle=target+decay*(offset*c+(v+damping*offset)*sn/frequency);
  s->velocity=decay*(v*c-(damping*v+24.f*offset)*sn/frequency)+impulse*.5f;
  if(fabsf(s->angle)>.6f) { s->angle=copysignf(.6f,s->angle); s->velocity=0; }
  s->velocity=fmaxf(-3.f,fminf(3.f,s->velocity));
  s->phase=fmodf(s->phase+dt,62.8318530718f);
  s->time=now; s->heading=heading;
  return s->angle;
}
static inline float tag_follow(tag_attachment* state,float heading,double now,uint64_t generation) {
  double dt=(now-state->time)*.001;
  if(!state->valid || generation!=state->generation || dt<0 || dt>.25) state->direction=heading;
  else if(dt>0) {
    float delta=atan2f(sinf(heading-state->direction),cosf(heading-state->direction));
    state->direction+=delta*(1-expf(-(float)dt/.09f));
  }
  state->time=now; state->generation=generation; state->valid=true;
  return state->direction;
}
// Upright source images attach at top-center; image extends behind the head.
// Position/length use world units, so zoom does not affect the attachment motion.
static inline void tag_attachment_geometry(float quad[8],float cord[14],float x,float y,
    float size,float image_w,float image_h,float scale,float heading,float trailing,float gap,float degrees) {
  float reach=(14.5f+gap)*scale;
  float ax=x-cosf(trailing)*reach,ay=y-sinf(trailing)*reach;
  float cx=x-cosf(heading)*reach*.5f,cy=y-sinf(heading)*reach*.5f;
  for(int i=0;i<7;i++) {
    float t=i/6.f,u=1-t;
    cord[2*i]=u*u*x+2*u*t*cx+t*t*ax;
    cord[2*i+1]=u*u*y+2*u*t*cy+t*t*ay;
  }
  float w=size*scale*image_w/fmaxf(image_w,image_h),h=size*scale*image_h/fmaxf(image_w,image_h);
  float a=trailing+1.57079632679f+degrees*.01745329252f,c=cosf(a),s=sinf(a);
  const float dx[]={-w/2,w/2,w/2,-w/2},dy[]={0,0,h,h};
  for(int i=0;i<4;i++) {quad[2*i]=ax+dx[i]*c-dy[i]*s;quad[2*i+1]=ay+dx[i]*s+dy[i]*c;}
}
#endif
