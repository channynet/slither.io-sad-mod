#ifndef PERSONAL_TAG_H
#define PERSONAL_TAG_H
#include <thermite.h>
void personal_tag_init(tenv* env);
void personal_tag_prepare(tenv* env);
void personal_tag_draw(tenv* env);
void personal_tag_controls(tenv* env);
void personal_tag_toggle(void);
void personal_cosmetic_toggle(void);
void personal_image_editor(tenv* env);
void personal_tag_preview(tenv* env,float x,float y,float scale,float direction,float alpha);
void personal_cosmetic_preview(tenv* env,float x,float y,float scale,float direction,float alpha);
void personal_tag_destroy(tenv* env);
#endif
