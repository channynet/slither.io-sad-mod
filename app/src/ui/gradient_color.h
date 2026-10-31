#ifndef GRADIENT_COLOR_H
#define GRADIENT_COLOR_H
#include <math.h>
// Position is relative to the complete body, not just visible samples.
static inline void gradient_color(float out[3],const float colors[4][3],int count,float position) {
  if(count<2) count=2; if(count>4) count=4;
  float t=isfinite(position)?fminf(1,fmaxf(0,position)):0;
  float segment=t*(count-1); int i=(int)segment;
  if(i>=count-1) i=count-2;
  float blend=segment-i;
  for(int c=0;c<3;c++) out[c]=colors[i][c]+(colors[i+1][c]-colors[i][c])*blend;
}
#endif
