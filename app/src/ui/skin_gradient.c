#include "skin_gradient.h"
#include "gradient_color.h"
#include "../user.h"
#include "../game/native_preferences.h"
#include "fire_skin.h"
#include <math.h>

void skin_effect_color(float out[3],int index,int count) {
  if(native_prefs.fire_skin) fire_body_color(out,index,count,skin_effect_time);
  else gradient_color(out,native_prefs.gradient_colors,native_prefs.gradient_count,count>1?(float)index/(count-1):0);
}

static float plain[3]={1,1,1};
static bool own_snake;
void skin_graphics_begin(const float color[3],bool own) {
  plain[0]=color[0]; plain[1]=color[1]; plain[2]=color[2]; own_snake=own;
}

void skin_gradient_body(tenv* env,bool enabled,int mode,int index,int count,const bp_instance* original) {
  if(!enabled) {
    bool styled=native_prefs.skin_mode && !(own_snake && native_prefs.keep_own_skin);
    bool faded=!own_snake && native_prefs.others_opacity>0 && native_prefs.others_opacity<.999f;
    if(!styled && !faded) { bp_renderer_push(env->usr->r->bpr,original); return; }
    bp_instance instance=*original;
    if(styled && native_prefs.skin_mode==1) {         // skinless: one plain colour, still shaded
      instance.uv_rect=env->usr->gdata.cg_uvs[mode==0?9:BLANK_UV];
      float shade=mode==0?original->color.r:1;
      instance.color.r=plain[0]*shade; instance.color.g=plain[1]*shade; instance.color.b=plain[2]*shade;
    } else if(styled) {                                 // high visibility: brighter, clearer stripes
      instance.color.r=fminf(1.6f,original->color.r*1.45f+.08f);
      instance.color.g=fminf(1.6f,original->color.g*1.45f+.08f);
      instance.color.b=fminf(1.6f,original->color.b*1.45f+.08f);
    }
    if(faded) instance.color.a*=native_prefs.others_opacity;
    bp_renderer_push(env->usr->r->bpr,&instance);
    return;
  }
  bp_instance instance=*original;
  float color[3]; skin_effect_color(color,index,count);
  // Reuse the white shaded body tile for textured mode, the blank tile for
  // flat modes. No additional instances, textures, or shader passes.
  instance.uv_rect=env->usr->gdata.cg_uvs[mode==0?9:BLANK_UV];
  float shade=mode==0?original->color.r:1;
  instance.color.r=color[0]*shade; instance.color.g=color[1]*shade; instance.color.b=color[2]*shade;
  bp_renderer_push(env->usr->r->bpr,&instance);
}
