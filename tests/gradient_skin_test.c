#include "../app/src/user.h"
#include "../app/src/game/native_preferences.h"
#include "../app/src/ui/gradient_color.h"
#include "../app/src/ui/skin_gradient.h"
#include <assert.h>
#include <string.h>
#include <math.h>
native_preferences native_prefs;
static tuser_data usr;
static renderer render;
static bp_instance captured;
static int pushes;
void bst_renderer_push(bst_renderer* r,const bst_instance* instance) { (void)r;(void)instance; }
void bp_renderer_push(bp_renderer* r,const bp_instance* instance) {
  (void)r; captured=*instance; pushes++;
}
static void close_color(const float a[3],const float b[3]) {
  for(int c=0;c<3;c++) assert(fabsf(a[c]-b[c])<.00001f);
}
int main(void) {
  const float colors[4][3]={{1,0,0},{0,1,0},{0,0,1},{1,1,1}};
  float color[3];
  for(int count=2;count<=4;count++) {
    for(int stop=0;stop<count;stop++) {
      gradient_color(color,colors,count,(float)stop/(count-1)); close_color(color,colors[stop]);
    }
    gradient_color(color,colors,count,-1);close_color(color,colors[0]);
    gradient_color(color,colors,count,2);close_color(color,colors[count-1]);
    gradient_color(color,colors,count,NAN);close_color(color,colors[0]);
  }
  gradient_color(color,colors,2,.5f);close_color(color,(float[]){.5f,.5f,0});
  memcpy(native_prefs.gradient_colors,colors,sizeof(colors));native_prefs.gradient_count=4;
  tenv env={.usr=&usr};
  usr.r=&render;
  usr.gdata.cg_uvs[9]=(vec4s){.1f,.2f,.3f,.4f};
  usr.gdata.cg_uvs[BLANK_UV]=(vec4s){.5f,.6f,.7f,.8f};
  bp_instance original={.circle={100,200,30,1},.uv_rect={.3f,.4f,.1f,.1f},.color={.75f,.75f,.75f,.42f}};
  skin_gradient_body(&env,false,0,0,101,&original);
  assert(pushes==1 && !memcmp(&captured,&original,sizeof(original)));
  for(int mode=0;mode<=2;mode++) {
    for(int i=0;i<101;i++) {
      int before=pushes;skin_gradient_body(&env,true,mode,i,101,&original);
      assert(pushes==before+1 && captured.color.a==original.color.a);
      assert(!memcmp(&captured.circle,&original.circle,sizeof(original.circle)));
      assert(!memcmp(&captured.uv_rect,&usr.gdata.cg_uvs[mode==0?9:BLANK_UV],sizeof(vec4s)));
      gradient_color(color,colors,4,i/100.0f);
      for(int c=0;c<3;c++) assert(fabsf(captured.color.raw[c]-color[c]*(mode==0?.75f:1))<.00001f);
    }
    skin_gradient_body(&env,true,mode,0,1,&original);assert(isfinite(captured.color.r));
  }
  // Graphics options: skinless (one plain colour, shaded in textured mode), high visibility,
  // my own skin kept, other snakes' opacity.
  native_prefs.others_opacity=1; native_prefs.keep_own_skin=true;
  float plain[3]={.2f,.4f,.9f};
  native_prefs.skin_mode=1; skin_graphics_begin(plain,false);
  for(int mode=0;mode<=2;mode++) {
    skin_gradient_body(&env,false,mode,3,10,&original);
    assert(!memcmp(&captured.uv_rect,&usr.gdata.cg_uvs[mode==0?9:BLANK_UV],sizeof(vec4s)));
    for(int c=0;c<3;c++) assert(fabsf(captured.color.raw[c]-plain[c]*(mode==0?.75f:1))<.00001f);
    assert(captured.color.a==original.color.a && !memcmp(&captured.circle,&original.circle,sizeof(original.circle)));
  }
  skin_graphics_begin(plain,true); skin_gradient_body(&env,false,0,3,10,&original);
  assert(!memcmp(&captured,&original,sizeof(original)));                       // my skin kept
  native_prefs.keep_own_skin=false; skin_gradient_body(&env,false,0,3,10,&original);
  assert(!memcmp(&captured.uv_rect,&usr.gdata.cg_uvs[9],sizeof(vec4s)));
  native_prefs.skin_mode=2; skin_graphics_begin(plain,false); skin_gradient_body(&env,false,0,3,10,&original);
  assert(!memcmp(&captured.uv_rect,&original.uv_rect,sizeof(vec4s)) && captured.color.r>original.color.r && captured.color.r<=1.6f);
  native_prefs.skin_mode=0; native_prefs.others_opacity=.5f; skin_gradient_body(&env,false,0,3,10,&original);
  assert(!memcmp(&captured.uv_rect,&original.uv_rect,sizeof(vec4s)) && fabsf(captured.color.a-.21f)<.00001f && captured.color.r==original.color.r);
  skin_graphics_begin(plain,true); skin_gradient_body(&env,false,0,3,10,&original);
  assert(captured.color.a==original.color.a);                                 // my own snake is not faded
  puts("Graphics options: skinless, high visibility, own skin kept, others' opacity passed");
  puts("Gradient: 2-4 stops, endpoints, interpolation, all render modes, OFF identity, alpha/geometry and one instance per segment passed");
}
