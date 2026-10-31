#include "../app/src/game/ntl_bot_core.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static nb_bot bot;
static nb_part mine[512], theirs[512];
// A body laid out behind a head, tail first; `curve` bends it into a ring.
static int lay(nb_part* out, int n, double hx, double hy, double ang, double curve) {
  double x = hx, y = hy, a = ang + M_PI;
  for (int i = 0; i < n; i++) {
    x += cos(a) * 24; y += sin(a) * 24; a += curve;
    out[n - 1 - i] = (nb_part){(float)x, (float)y, 0, 0, false};
  }
  return n;
}
static nb_out run(nb_scene* scene, int frames) {
  nb_out out = {0};
  for (int i = 0; i < frames; i++) { scene->now_ms = i * 16.0; nb_go(&bot, scene, &out); }
  return out;
}
int main(void) {
  double grd = 21600;
  // Growing: after the short wait it aims at the only food; nothing in the way, no boost.
  {
    nb_snake snakes[1] = {{1, 20000, 20000, 0, 0, 0, 0, 5.78f, 1.1f, true, mine, lay(mine, 12, 20000, 20000, 0, 0)}};
    nb_food foods[2] = {{20300, 20120, 6, false}, {20300, 19700, 9, true}};   // the bigger one is already eaten
    nb_scene scene = {snakes, 1, 0, foods, 2, grd, false, 0, 20000, 20000, 50, 0, false, true, 2000};
    nb_reset(&bot);
    nb_out first = run(&scene, 1);
    assert(first.stage == NB_GROW && !first.has_goal && !first.accel);
    nb_out later = run(&scene, 20);
    assert(later.stage == NB_GROW && later.has_goal && later.goal_x == 20300 && later.goal_y == 20120 && !later.accel);
  }
  // A body right in front: turn away from it at once, whatever the food says.
  {
    int n = lay(mine, 12, 20000, 20000, 0, 0), m = 0;
    for (int i = 0; i < 20; i++) theirs[m++] = (nb_part){20090, (float)(19760 + i * 24), 0, 0, false};
    nb_snake snakes[2] = {{1, 20000, 20000, 0, 0, 0, 0, 5.78f, 1.1f, true, mine, n}, {2, 20090, 20260, 0, 0, 1.57f, 1.57f, 5.78f, 1.5f, true, theirs, m}};
    nb_food foods[1] = {{20400, 20000, 9, false}};
    nb_scene scene = {snakes, 2, 0, foods, 1, grd, false, 0, 20000, 20000, 50, 0, false, true, 2000};
    nb_reset(&bot);
    nb_out out = run(&scene, 20);
    assert(out.has_goal && out.goal_x < 20000);          // away from the body, not at the food beyond it
  }
  // Smart bot off: circling from the first step, whatever the length; held boost is passed on.
  {
    int n = lay(mine, 70, 20000, 20000, 0, 24 / 300.0);
    nb_snake snakes[1] = {{1, 20000, 20000, 0, 0, 0, 0, 5.78f, 1.64f, true, mine, n}};
    nb_scene scene = {snakes, 1, 0, NULL, 0, grd, false, 0, 20000, 20000, 30, 0, false, false, 2000};
    nb_reset(&bot);
    nb_out out = run(&scene, 30);
    assert(out.stage == NB_CIRCLE && !out.accel);
    scene.default_accel = true;
    assert(run(&scene, 2).accel);
  }
  // Driving a snake with it for a minute: from a straight body it curls up, then keeps going
  // round its own ring instead of wandering off or unwinding.
  {
    int n = 70; double x = 20000, y = 20000, ang = 0.3, turned = 0, sc = 1.64;
    for (int i = 0; i < n; i++) mine[i] = (nb_part){(float)(x - cos(ang) * 24 * (n - i)), (float)(y - sin(ang) * 24 * (n - i)), 0, 0, false};
    double rate = 0.033 / 8 * (0.13 + 0.87 * pow((7 - sc) / 6, 2)), sx = 0, sy = 0, far = 0; int samples = 0;
    nb_reset(&bot); nb_out out = {0};
    for (int f = 0; f < 3600; f++) {
      nb_snake snakes[1] = {{1, (float)x, (float)y, 0, 0, (float)ang, (float)ang, 5.78f, (float)sc, true, mine, n}};
      nb_scene scene = {snakes, 1, 0, NULL, 0, grd, false, 0, x, y, 2500, f * 16.667, false, true, 2000};
      nb_go(&bot, &scene, &out);
      if (out.has_goal) {
        double d = remainder(atan2(out.goal_y - y, out.goal_x - x) - ang, 2 * M_PI), step = rate * 16.667;
        d = d > step ? step : d < -step ? -step : d;
        ang += d; turned += d;
      }
      x += cos(ang) * 5.78 * 16.667 / 32; y += sin(ang) * 5.78 * 16.667 / 32;
      if (hypot(x - mine[n - 1].xx, y - mine[n - 1].yy) >= 24) {
        for (int i = 0; i + 1 < n; i++) mine[i] = mine[i + 1];
        mine[n - 1] = (nb_part){(float)x, (float)y, 0, 0, false};
      }
      if (f >= 2400) { sx += x; sy += y; samples++; }
    }
    for (int i = 0; i < n; i++) far = fmax(far, hypot(mine[i].xx - sx / samples, mine[i].yy - sy / samples));
    assert(out.stage == NB_CIRCLE && fabs(turned) > 4 * 2 * M_PI && far < 600);
  }
  // Smart bot on and long enough: curl up, and switch to circling once the head meets the body.
  {
    int n = lay(mine, 120, 20000, 20000, 0, 0);
    nb_snake snakes[1] = {{1, 20000, 20000, 0, 0, 0, 0, 5.78f, 2.1f, true, mine, n}};
    nb_scene scene = {snakes, 1, 0, NULL, 0, grd, false, 0, 20000, 20000, 2500, 0, false, true, 2000};
    nb_reset(&bot);
    nb_out out = run(&scene, 3);
    assert(out.stage == NB_TOCIRCLE && out.has_goal);
    n = lay(mine, 120, 20000, 20000, 0, 24 / 250.0);    // now wound into a ring that reaches the head
    snakes[0].count = n;
    out = run(&scene, 3);
    assert(out.stage == NB_CIRCLE);
    scene.length = 500;                                  // shrunk below the bot length: back to growing
    assert(run(&scene, 1).stage == NB_GROW);
  }
  puts("NTL bot core: food choice, turning away, circling, curling up, stage changes and a one-minute drive passed");
  return 0;
}
