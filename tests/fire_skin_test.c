#include "../app/src/ui/fire_skin.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static int under_n, over_n; static float max_alpha, min_size=1e9;
void bst_renderer_push(bst_renderer* r, const bst_instance* i) {
  if (r == (bst_renderer*)1) under_n++; else over_n++;
  if (i->color.a > max_alpha) max_alpha = i->color.a;
  if (i->circle.z < min_size) min_size = i->circle.z;
  for (int c = 0; c < 3; c++) assert(i->color.raw[c] >= 0 && i->color.raw[c] <= 1);
}
int main(void) {
  float c[3];
  fire_color(c, 0); assert(c[0] < .4f && c[1] < .05f);              // cold: deep red
  fire_color(c, 1); assert(c[0] == 1 && c[1] > .9f && c[2] > .5f);  // white hot
  fire_color(c, NAN); assert(c[0] < .4f);
  float last_g = -1;
  for (int i = 0; i <= 100; i++) { fire_color(c, i / 100.0f); assert(c[1] >= last_g - 1e-6f); last_g = c[1]; }  // only gets yellower
  // heat stays in range, the head is hotter than the tail on average, and it moves with time
  double head = 0, tail = 0; int changed = 0;
  for (int k = 0; k < 400; k++) {
    float t = k * .037f;
    for (int i = 0; i < 60; i++) { float h = fire_heat(i, 60, t); assert(h >= 0 && h <= 1); }
    head += fire_heat(0, 60, t); tail += fire_heat(59, 60, t);
    if (fabsf(fire_heat(10, 60, t) - fire_heat(10, 60, t + .05f)) > .01f) changed++;
  }
  assert(head / 400 > tail / 400 + .2);
  assert(changed > 300);
  // effects: a glow under every eighth part, two flames off every third drawn part, hidden parts skipped
  float x[50], y[50], a[50]; uint8_t vis[50];
  for (int i = 0; i < 50; i++) { x[i] = 1000 + i * 20; y[i] = 500; a[i] = 0; vis[i] = i < 40 ? 2 : 0; }
  fire_body b = {x, y, a, vis, 50, 14.5f, 1000, 500, 1, 640, 360, 1, 1};
  int n = fire_skin_effects((bst_renderer*)1, (bst_renderer*)2, &b, 3.3f);
  assert(under_n == 5 && over_n == 28 && n == 33);
  assert(max_alpha > .05f && max_alpha <= .96f && min_size > 0);
  // strength is clamped; faded snakes fade their flames too
  under_n = over_n = 0; max_alpha = 0; b.strength = 99; b.alpha = .1f;
  fire_skin_effects((bst_renderer*)1, (bst_renderer*)2, &b, 1);
  assert(max_alpha <= .1f);
  puts("fire skin test passed");
  return 0;
}
