#ifndef SNAKE_MISSILE_H
#define SNAKE_MISSILE_H
#include <thermite.h>
extern bool snake_missile_active;
extern float snake_missile_x,snake_missile_y;
extern char snake_missile_status[192];
void snake_missile_input(tenv* env);
void snake_missile_reset(void);
#endif
