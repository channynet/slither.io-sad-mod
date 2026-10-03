#ifndef TAG_ATTACHMENT_H
#define TAG_ATTACHMENT_H
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
typedef struct { float direction; double time; uint64_t generation; bool valid; } tag_attachment;
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
