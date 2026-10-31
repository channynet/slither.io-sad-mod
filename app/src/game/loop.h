#ifndef LOOP_H
#define LOOP_H

#include <thermite.h>

void game_loop(tenv* env);
// The same network and state update without drawing, for a minimized window.
void game_background(tenv* env);

#endif