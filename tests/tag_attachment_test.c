#include "../app/src/ui/tag_attachment.h"
#include <assert.h>
#include <stdio.h>
static void near(float a,float b) { assert(fabsf(a-b)<.001f); }
int main(void) {
  float q[8],c[14];
  // Heading up: top-center is tied below the head; image hangs down.
  tag_attachment_geometry(q,c,100,100,128,512,256,1,-1.57079632679f,-1.57079632679f,24,0);
  near(c[0],100); near(c[1],100); near(c[12],100); near(c[13],138.5f);
  near((q[0]+q[2])/2,c[12]); near((q[1]+q[3])/2,c[13]);
  near(q[0],36); near(q[1],138.5f); near(q[4],164); near(q[5],202.5f);
  // Heading right: image and cord are behind the head, never screen-up anchored.
  tag_attachment_geometry(q,c,100,100,128,512,256,1,0,0,24,0);
  near(c[12],61.5f); near(c[13],100); near((q[4]+q[6])/2,-2.5f);
  // Body growth and zoom use the same scale for cord and image.
  tag_attachment_geometry(q,c,100,100,128,512,256,.5f,0,0,24,0);
  near(c[12],80.75f); near(hypotf(q[2]-q[0],q[3]-q[1]),64);
  near(hypotf(q[4]-q[2],q[5]-q[3]),32);
  // Image-angle correction rotates around the attachment, not its center.
  tag_attachment_geometry(q,c,100,100,128,512,256,1,0,0,24,90);
  near((q[0]+q[2])/2,c[12]); near((q[1]+q[3])/2,c[13]);
  tag_attachment a={0},b={0};
  near(tag_follow(&a,0,1000,1),0);
  float turn=tag_follow(&a,1,1016,1); assert(turn>0 && turn<1);
  near(tag_follow(&a,2,1016,1),turn); // duplicate frame never advances motion
  near(tag_follow(&a,-2,1032,2),-2); // respawn resets the old trail
  near(tag_follow(&a,1,2000,2),1); // pause resumes at current head
  near(tag_follow(&a,0,100,2),0); // clock reset
  a=(tag_attachment){0}; tag_follow(&a,3.13f,1000,1);
  float wrapped=tag_follow(&a,-3.13f,1016,1); assert(wrapped>3.13f && wrapped<3.16f);
  a=(tag_attachment){0};tag_follow(&a,0,1000,1);tag_follow(&b,0,1000,1);
  for(int i=1;i<=60;i++) tag_follow(&a,1,1000+i*1000.0/60,1);
  for(int i=1;i<=120;i++) tag_follow(&b,1,1000+i*1000.0/120,1);
  near(a.direction,b.direction);
  puts("Head tag: tether anchor, rotation, zoom/aspect ratio, angle pivot, turn following, respawn/pause/clock reset and FPS independence passed");
}
