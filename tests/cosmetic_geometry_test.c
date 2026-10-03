#include "../app/src/ui/cosmetic_geometry.h"
#include <assert.h>
#include <stdio.h>
static void near(float a,float b) { assert(fabsf(a-b)<0.001f); }
int main(void) {
  float p[8];
  // Facing up leaves an up-oriented image upright; behind the head is below it.
  cosmetic_quad(p,100,100,160,1000,500,1,-1.57079632679f,-20,true,0);
  near(p[0],20); near(p[1],80); near(p[4],180); near(p[5],160);
  // Facing right rotates the wings vertical, with the attachment behind the head.
  cosmetic_quad(p,100,100,160,1000,500,1,0,-20,true,0);
  near(p[0],120); near(p[1],20); near(p[4],40); near(p[5],180);
  // Halving zoom halves both dimensions and the attachment offset.
  cosmetic_quad(p,100,100,160,1000,500,0.5f,0,-20,true,0);
  near(p[0],110); near(p[1],60); near(p[4],70); near(p[5],140);
  // Rotation can be disabled independently of attachment direction.
  cosmetic_quad(p,100,100,160,1000,500,1,0,-20,false,0);
  near(p[0],0); near(p[1],60); near(p[4],160); near(p[5],140);
  cosmetic_quad(p,100,100,160,1000,500,1,0,-20,false,90);
  near(p[0],120); near(p[1],20);
  puts("Cosmetics: head direction, zoom, offset, aspect ratio and rotation override passed");
}
