#ifndef NTL_TAGS_UI_H
#define NTL_TAGS_UI_H
// NTL tags on screen: every snake's tag on its rope, the pictures' textures, and the settings
// (choosing between my own image tag and an NTL tag, free tags, bought tags with a password).
#include <thermite.h>
void ntl_tags_init(tenv* env);
void ntl_tags_prepare(tenv* env);     // before the frame: makes the pictures asked for
void ntl_tags_draw(tenv* env);        // in the game window
void ntl_tags_controls(tenv* env);    // settings
void ntl_tags_destroy(tenv* env);
#endif
