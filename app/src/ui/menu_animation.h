#ifndef MENU_ANIMATION_H
#define MENU_ANIMATION_H
#include <stdint.h>
#include <math.h>
// Use the raw monotonic timer: gameplay resets glfwGetTime(), which can stall
// ImGui's accumulated clock until the old game time is reached again.
static inline int menu_animation_frame(uint64_t ticks,uint64_t frequency,const int* delays,int count) {
  if(!frequency||count<=0) return 0;
  double total=0;
  for(int i=0;i<count;i++) { if(delays[i]<=0) return 0; total+=delays[i]; }
  double elapsed=fmod((double)ticks/(double)frequency*1000.0,total);
  int frame=0;
  while(frame<count-1 && elapsed>=delays[frame]) elapsed-=delays[frame++];
  return frame;
}
#endif
