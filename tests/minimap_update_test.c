#include "../app/src/game/minimap_update.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#define STRIDE 512
static float old_follow[STRIDE*STRIDE],new_follow[STRIDE*STRIDE];
static uint8_t target[STRIDE*STRIDE],old_out[STRIDE*STRIDE],new_out[STRIDE*STRIDE];
static void reference(float* follow,const uint8_t* src,uint8_t* dst,int size,float alpha) {
  for(int y=0;y<size;y++) for(int x=0;x<size;x++) {
    int i=y*STRIDE+x; follow[i]+=alpha*((float)src[i]-follow[i]);
  }
  for(int y=0;y<size;y++) for(int x=0;x<size;x++) {
    int i=y*STRIDE+x; dst[i]=(uint8_t)(follow[i]+0.5f);
  }
}
static double now(void) { LARGE_INTEGER q,f; QueryPerformanceCounter(&q); QueryPerformanceFrequency(&f); return q.QuadPart*1000.0/f.QuadPart; }
int main(void) {
  for(int i=0;i<STRIDE*STRIDE;i++) { target[i]=(i*37+i/512)%256; old_follow[i]=new_follow[i]=(i*11)%256+.125f; }
  int sizes[]={0,1,63,128,255,256,511,512};
  float alphas[]={0,.01f,.05f,.125f,.9f,1};
  for(int s=0;s<8;s++) for(int a=0;a<6;a++) {
    reference(old_follow,target,old_out,sizes[s],alphas[a]);
    minimap_update(new_follow,target,new_out,sizes[s],STRIDE,alphas[a]);
    assert(!memcmp(old_follow,new_follow,sizeof(old_follow)));
    assert(!memcmp(old_out,new_out,sizeof(old_out)));
  }
  // A CPU microbenchmark only; this does not predict total game FPS.
  for(int size=256;size<=512;size*=2) {
    double old_ms=0,new_ms=0;
    for(int round=0;round<8;round++) {
      for(int order=0;order<2;order++) {
        double start=now();
        if((round+order)%2) {
          for(int n=0;n<300;n++) reference(old_follow,target,old_out,size,.05f);
          old_ms+=now()-start;
        } else {
          for(int n=0;n<300;n++) minimap_update(new_follow,target,new_out,size,STRIDE,.05f);
          new_ms+=now()-start;
        }
      }
    }
    assert(!memcmp(old_out,new_out,sizeof(old_out)));
    printf("Minimap %dx%d CPU: old %.4f ms, fused %.4f ms per update\n",size,size,old_ms/2400,new_ms/2400);
  }
  puts("Minimap: interpolation, rounding, partial strides and untouched pixels match exactly");
}
