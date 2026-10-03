#ifndef COSMETIC_GEOMETRY_H
#define COSMETIC_GEOMETRY_H
#include <math.h>
#include <stdbool.h>
// Source image points up. Game headings point right at zero radians.
static inline void cosmetic_quad(float out[8],float x,float y,float size,float image_w,float image_h,
                                 float scale,float direction,float offset,bool rotate,float degrees) {
  float w=size*scale*image_w/fmaxf(image_w,image_h),h=size*scale*image_h/fmaxf(image_w,image_h);
  x+=cosf(direction)*offset*scale; y+=sinf(direction)*offset*scale;
  float angle=(rotate?direction+1.57079632679f:0)+degrees*0.01745329252f;
  float c=cosf(angle),s=sinf(angle),dx[]={-w/2,w/2,w/2,-w/2},dy[]={-h/2,-h/2,h/2,h/2};
  for(int i=0;i<4;i++) { out[2*i]=x+dx[i]*c-dy[i]*s; out[2*i+1]=y+dx[i]*s+dy[i]*c; }
}
#endif
