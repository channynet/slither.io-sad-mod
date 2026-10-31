#ifndef SKIN_GRADIENT_H
#define SKIN_GRADIENT_H
#include <thermite.h>
#include "../rendering/bp_renderer.h"
void skin_gradient_body(tenv* env,bool enabled,int mode,int index,int count,const bp_instance* original);
void skin_gradient_controls(void);
// Graphics options (NTL "Graphics" / "High visibility skins") for the snake drawn next:
// its plain colour (the glow colour of its first skin stripe) and whether it is the player's.
void skin_graphics_begin(const float color[3], bool own);
// The body colour the gradient or the burning skin gives this part (head = 0).
void skin_effect_color(float out[3],int index,int count);
void fire_skin_controls(void);
#endif
