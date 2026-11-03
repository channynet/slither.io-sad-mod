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
#include "../network/tag_share.h"
#include "../game/ntl_tags.h"
// My image tag for sharing (network/tag_share.h): false when there is none to show.
bool personal_tag_share(share_image* out);
// An image tag hung from a head as mine is (also other players' shared ones).
void personal_tag_draw_image(VkDescriptorSet picture,int tw,int th,float size,float gap,float angle,
    float x,float y,float scale,float heading,float trailing,float swing,float alpha,ntl_tag_look look);
#endif
