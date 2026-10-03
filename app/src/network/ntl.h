#ifndef NTL_H
#define NTL_H
#include <thermite.h>
#include "../game/user_settings.h"
void ntl_init(void);
void ntl_destroy(void);
void ntl_frame(tenv* env);
void ntl_pace(tenv* env);
bool ntl_mouse_captured(void);
bool ntl_is_teammate(tenv* env,int sid);
void ntl_focus_chat(void);
void ntl_lightweight_settings(user_settings* settings);
#endif
