#include "../app/src/ui/head_point_geometry.h"
#include <assert.h>
#include <stdio.h>
static void near(float a,float b) { assert(fabsf(a-b)<0.001f); }
int main(void) {
  float p[2];
  head_point_position(p,100,200,0,1,1,100,200,800,600);
  near(p[0],414.5f); near(p[1],300);
  head_point_position(p,100,200,1.570796327f,2,0.5f,100,200,800,600);
  near(p[0],400); near(p[1],314.5f);
  head_point_position(p,110,190,3.141592654f,1,2,100,200,800,600);
  near(p[0],391); near(p[1],280);
  puts("Head point: direction, snake scale, zoom and camera offset passed");
}
