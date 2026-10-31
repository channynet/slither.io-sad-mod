#include "fire_skin.h"
#include <math.h>

float skin_effect_time;

static float hash01(int a, int b) {
  uint32_t h = (uint32_t)a * 374761393u + (uint32_t)b * 668265263u + 0x9E3779B9u;
  h = (h ^ (h >> 13)) * 1274126177u;
  h ^= h >> 16;
  return (h & 0xFFFFFF) / 16777215.0f;
}

float fire_heat(int index, int count, float t) {
  float along = count > 1 ? (float)index / (count - 1) : 0;          // 0 head .. 1 tail
  float heat = .74f - .42f * along;
  heat += .17f * sinf(index * .55f - t * 9.0f);                        // waves running tailwards
  heat += .07f * sinf(index * 1.7f + t * 23.0f);                       // quick flicker
  heat += .08f * (hash01(index, (int)floorf(t * 14)) - .5f);           // a little crackle
  return heat < 0 ? 0 : heat > 1 ? 1 : heat;
}

void fire_color(float out[3], float heat) {
  static const float stops[5][3] = {{.30f, .02f, .00f}, {.85f, .10f, .00f}, {1.0f, .42f, .02f},
                                    {1.0f, .78f, .12f}, {1.0f, .96f, .62f}};
  static const float at[5] = {0, .35f, .6f, .82f, 1};
  if (!(heat > 0)) heat = 0;
  if (heat > 1) heat = 1;
  int i = 0;
  while (i < 3 && heat > at[i + 1]) i++;
  float f = (heat - at[i]) / (at[i + 1] - at[i]);
  for (int c = 0; c < 3; c++) out[c] = stops[i][c] + (stops[i + 1][c] - stops[i][c]) * f;
}

void fire_body_color(float out[3], int index, int count, float t) {
  fire_color(out, fire_heat(index, count, t));
}

int fire_skin_effects(bst_renderer* under, bst_renderer* over, const fire_body* b, float t) {
  int pushed = 0;
  float s = b->strength < .2f ? .2f : b->strength > 1.5f ? 1.5f : b->strength;
  float r = b->radius * b->scale;                                       // on screen
  for (int j = b->count - 1; j >= 0; j--) {
    if (b->visible[j] < 1) continue;
    float x = b->half_w + (b->x[j] - b->view_x) * b->scale;
    float y = b->half_h + (b->y[j] - b->view_y) * b->scale;
    float heat = fire_heat(j, b->count, t), col[3];
    // the glow under the body
    if (under && (j % 8 == 0 || b->count < 16)) {      // (big soft quads: few of them, the fill is what costs)
      fire_color(col, heat * .8f);
      float flick = .75f + .25f * sinf(t * 17 + j * 2.3f);
      float g = r * (2.2f + .4f * s) * flick;
      bst_renderer_push(under, &(bst_instance){{x - g, y - g, 2 * g}, {col[0], col[1], col[2], .6f * s * b->alpha}});
      pushed++;
    }
    if (!over || (j % 3 && b->count >= 16)) continue;     // flames off every third part
    // two flames per part, one off each side, trailing behind as they burn out
    float tail_x = cosf(b->angle[j]), tail_y = sinf(b->angle[j]);
    float side_x = -tail_y, side_y = tail_x;
    for (int e = 0; e < 2; e++) {
      float speed = 1.1f + .9f * hash01(j, e + 7);
      float phase = t * speed + hash01(j, e);
      phase -= floorf(phase);                                            // 0 born .. 1 gone
      float side = (e == 0 ? -1.0f : e == 1 ? 1.0f : (hash01(j, 11) < .5f ? -1.0f : 1.0f)) * (.35f + .5f * hash01(j, e + 3));
      float out = r * (1.0f + 2.2f * phase * s);
      float back = r * 3.6f * phase * s;
      float fx = x + side_x * side * out + tail_x * back, fy = y + side_y * side * out + tail_y * back;
      float size = r * (.42f + .75f * (1 - phase)) * (.7f + .3f * s);
      fire_color(col, 1 - phase * .9f);
      float born = phase < .12f ? phase / .12f : 1;                        // fade in off the body
      float a = born * powf(1 - phase, 1.5f) * .95f * (.6f + .4f * s) * b->alpha;
      bst_renderer_push(over, &(bst_instance){{fx - size, fy - size, 2 * size}, {col[0], col[1], col[2], a}});
      pushed++;
    }
  }
  return pushed;
}
