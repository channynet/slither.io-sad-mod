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
  tag_sway swing={0},faster={0};
  near(tag_sway_update(&swing,0,1000,1),0);
  for(int i=1;i<=60;i++) tag_sway_update(&swing,0,1000+i*1000.0/60,1);
  assert(fabsf(swing.angle)>.025f); // visible gentle movement even going straight
  float old=swing.angle;
  near(tag_sway_update(&swing,2,2000,1),old); // no duplicate-frame impulse
  near(tag_sway_update(&swing,2,3000,1),0); // no jump after pause
  near(tag_sway_update(&swing,1,3016,2),0); // respawn
  near(tag_sway_update(&swing,0,100,2),0); // clock reset
  swing=(tag_sway){0}; faster=(tag_sway){0};
  tag_sway_update(&swing,0,1000,1); tag_sway_update(&faster,0,1000,1);
  for(int i=1;i<=240;i++) tag_sway_update(&swing,sinf(i/60.f*2),1000+i*1000.0/60,1);
  for(int i=1;i<=480;i++) tag_sway_update(&faster,sinf(i/120.f*2),1000+i*1000.0/120,1);
  assert(fabsf(swing.angle-faster.angle)<.002f);
  assert(fabsf(swing.velocity-faster.velocity)<.005f);
  swing=(tag_sway){0}; tag_sway_update(&swing,3.13f,1000,1);
  tag_sway_update(&swing,-3.13f,1016,1); assert(fabsf(swing.velocity)<.1f);
  for(int i=1;i<=5000;i++) {
    tag_sway_update(&swing,i%2?3.f:-3.f,1016+i*16.0,1);
    assert(isfinite(swing.angle) && fabsf(swing.angle)<=.6001f && fabsf(swing.velocity)<=3.001f);
  }
  // The image still pivots exactly at the cord endpoint while swinging.
  tag_attachment_geometry(q,c,100,100,128,512,256,1,0,.12f,24,25);
  near((q[0]+q[2])/2,c[12]); near((q[1]+q[3])/2,c[13]);
  puts("Head tag: tether anchor, rotation, zoom/aspect ratio, angle pivot, turn following, respawn/pause/clock reset FPS independence, damped sway, idle motion and angular limits passed");
}
