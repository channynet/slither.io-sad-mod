// Wall status (NTL border semaphore): trend from the wall radius over the last two seconds.
#include "../app/src/game/wall_status.h"
#include <assert.h>
#include <stdio.h>
int wall_status_trend;
int main(void) {
  wall_status w = {0};
  double t = 10;
  for (int i = 0; i < 200; i++, t += 1 / 60.0) assert(wall_status_update(&w, t, 15000) == WALL_STILL);
  int trend = 0;
  for (int i = 0; i < 180; i++, t += 1 / 60.0) trend = wall_status_update(&w, t, 15000 - i * .5f);   // 30 a second
  assert(trend == WALL_SHRINKING);
  float r = 15000 - 179 * .5f;
  for (int i = 0; i < 180; i++, t += 1 / 60.0) trend = wall_status_update(&w, t, r);
  assert(trend == WALL_STILL);                                   // still for 3 s: the history is all equal
  for (int i = 0; i < 180; i++, t += 1 / 60.0) trend = wall_status_update(&w, t, r + i * .5f);
  assert(trend == WALL_GROWING);
  assert(wall_status_update(&w, t + 10, 9000) == WALL_STILL);   // a long gap (a new game) starts over
  float c[3];
  wall_status_color(WALL_SHRINKING, c); assert(c[0] == 1 && c[1] < .3f);
  wall_status_color(WALL_STILL, c); assert(c[0] == 1 && c[1] > .5f && c[2] < .2f);
  wall_status_color(WALL_GROWING, c); assert(c[1] == 1 && c[0] < .3f);
  puts("Wall status: still, shrinking, growing, new game and NTL colours passed");
  return 0;
}
