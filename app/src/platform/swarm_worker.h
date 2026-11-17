#ifndef MISSILE_WORKER_H
#define MISSILE_WORKER_H
#include <stdbool.h>
typedef struct { bool running; int active,total,connecting,waiting; char state[48]; } swarm_worker_state;
bool swarm_worker_start(const char* server,float x,float y,int count);
bool swarm_worker_control(float x,float y,int mode,double angle,bool boost);
void swarm_worker_target(float x,float y);
swarm_worker_state swarm_worker_poll(void);
void swarm_worker_stop(void);
#endif
