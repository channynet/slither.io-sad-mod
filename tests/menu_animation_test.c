#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "../app/src/ui/menu_animation.h"
#include <assert.h>
#include <stdio.h>
#include <windows.h>
int main(void) {
  const int delays[]={200,200,200,200};
  assert(menu_animation_frame(0,1000,delays,4)==0);
  assert(menu_animation_frame(199,1000,delays,4)==0);
  assert(menu_animation_frame(200,1000,delays,4)==1);
  assert(menu_animation_frame(400,1000,delays,4)==2);
  assert(menu_animation_frame(600,1000,delays,4)==3);
  assert(menu_animation_frame(799,1000,delays,4)==3);
  assert(menu_animation_frame(800,1000,delays,4)==0);
  assert(menu_animation_frame(86400200,1000,delays,4)==1);
  glfwInitHint(GLFW_PLATFORM,GLFW_PLATFORM_NULL);
  assert(glfwInit());
  glfwSetTime(1000);
  double old_backend_time=glfwGetTime();
  uint64_t start=glfwGetTimerValue(),frequency=glfwGetTimerFrequency();
  glfwSetTime(0); // The same reset performed by Play / reconnect.
  assert(glfwGetTime()<1 && glfwGetTimerValue()>=start);
  unsigned seen=0;
  for(int i=0;i<90;i++) {
    int frame=menu_animation_frame(glfwGetTimerValue(),frequency,delays,4);
    seen|=1u<<frame;
    double reset_clock=glfwGetTime();
    if(reset_clock<=old_backend_time) reset_clock=old_backend_time+0.00001;
    old_backend_time=reset_clock;
    Sleep(10);
  }
  assert(seen==15); // All four original poses still play after a real GLFW clock reset.
  assert(old_backend_time<1000.1); // Reproduce the previous almost-frozen ImGui clock.
  glfwTerminate();
  puts("Menu GIF: original 200ms boundaries, 800ms loop and all four poses after actual GLFW clock reset passed");
}
