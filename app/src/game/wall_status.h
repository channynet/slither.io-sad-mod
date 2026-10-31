#ifndef WALL_STATUS_H
#define WALL_STATUS_H
// Wall status (NTL "Dynamic border semaphore"): whether the wall the server announces is
// shrinking, standing still or growing, from its radius over the last two seconds.
#include <stdbool.h>
#define WALL_STATUS_SAMPLES 9            // one every 0.25 s: 2 s of history

typedef struct {
  double at[WALL_STATUS_SAMPLES];
  float radius[WALL_STATUS_SAMPLES];
  int count, next;
  double last;
} wall_status;

enum { WALL_SHRINKING = -1, WALL_STILL = 0, WALL_GROWING = 1 };
extern int wall_status_trend;            // this frame's trend (redraw.c); 2: the server's wall does not move

// Call every frame with the time in seconds and the wall radius. Returns the trend.
static inline int wall_status_update(wall_status* w, double now, float radius) {
  if (w->count && (now - w->last > 3 || now < w->last)) w->count = w->next = 0;   // a new game
  if (!w->count || now - w->at[(w->next + WALL_STATUS_SAMPLES - 1) % WALL_STATUS_SAMPLES] >= .25) {
    w->at[w->next] = now; w->radius[w->next] = radius;
    w->next = (w->next + 1) % WALL_STATUS_SAMPLES;
    if (w->count < WALL_STATUS_SAMPLES) w->count++;
  }
  w->last = now;
  if (w->count < 2) return WALL_STILL;
  int oldest = w->count < WALL_STATUS_SAMPLES ? 0 : w->next;
  float change = radius - w->radius[oldest];
  if (change < -1.5f) return WALL_SHRINKING;
  if (change > 1.5f) return WALL_GROWING;
  return WALL_STILL;
}

// Red shrinking, orange still, green growing (NTL's colours).
static inline void wall_status_color(int trend, float out[3]) {
  if (trend == WALL_SHRINKING) { out[0] = 1; out[1] = .18f; out[2] = .15f; }
  else if (trend == WALL_GROWING) { out[0] = .25f; out[1] = 1; out[2] = .35f; }
  else { out[0] = 1; out[1] = .6f; out[2] = .1f; }
}
#endif
