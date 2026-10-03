#ifndef MINIMAP_UPDATE_H
#define MINIMAP_UPDATE_H
#include <stdint.h>
// Keep interpolation and rounding identical, but consume each float while hot.
static inline void minimap_update(float* follow,const uint8_t* target,uint8_t* out,
                                 int size,int stride,float alpha) {
  for(int y=0;y<size;y++) for(int x=0;x<size;x++) {
    int i=y*stride+x;
    follow[i]+=alpha*((float)target[i]-follow[i]);
    out[i]=(uint8_t)(follow[i]+0.5f);
  }
}
#endif
