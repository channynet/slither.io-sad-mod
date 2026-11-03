#ifndef MISSILE_WORKER_H
#define MISSILE_WORKER_H
#include <stdbool.h>
typedef struct { bool running; int active,total; char state[48]; } missile_worker_state;
bool missile_worker_start(const char* server,float x,float y,int count);
void missile_worker_target(float x,float y);
missile_worker_state missile_worker_poll(void);
void missile_worker_stop(void);
#endif
