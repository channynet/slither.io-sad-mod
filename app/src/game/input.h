#ifndef INPUT_H
#define INPUT_H

#include <thermite.h>

void input(tenv* env);
void input_network(tenv* env);

// Safewall is steering along the wall now (and the angle it steers to).
extern bool safe_wall_steering;
extern float safe_wall_angle;
// Body hug (E): active this frame (E held), steering now, and the point aimed at.
extern bool body_hug_active, body_hug_steering;
// F1 / F2 (NTL "Coil Left / Right"): keep turning as hard as possible, -1 counterclockwise, +1 clockwise, 0 off.
extern int native_coil;
extern float body_hug_tx, body_hug_ty;

// Hold Q to steer with the mouse and boost, temporarily overriding bot/coil/hug.
extern bool missile_steering;
extern float missile_angle;
extern const char* missile_status;

void body_hug_log(int kind,const char* fmt,...);   // E diagnostics (hug-log.txt)
#endif
