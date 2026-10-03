#ifndef SBOT_H
#define SBOT_H

#include <thermite.h>

// Local defensive trajectory planner. No strategy UI or targeting/attack logic.

typedef struct sbot {
  struct {
    float xm;
    float ym;
    bool accel;
  } output;
} sbot;

void sbot_init(tenv* env);
void sbot_go(tenv* env);
void sbot_destroy(tenv* env);

#endif