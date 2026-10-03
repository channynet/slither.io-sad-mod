#ifndef HEAD_POINT_GEOMETRY_H
#define HEAD_POINT_GEOMETRY_H
#include <math.h>
// Visible head tip, not an authoritative server collision boundary.
static inline void head_point_position(float out[2],float hx,float hy,float direction,
    float scale,float zoom,float view_x,float view_y,float width,float height) {
  float radius=14.5f*scale;
  out[0]=width*0.5f+(hx+cosf(direction)*radius-view_x)*zoom;
  out[1]=height*0.5f+(hy+sinf(direction)*radius-view_y)*zoom;
}
#endif
