#ifndef FIRE_SKIN_H
#define FIRE_SKIN_H
// Burning skin: the body glows in fire colours that run from the head to the tail and flicker,
// with a warm glow under it and flames licking off its sides and trailing behind.
// Only drawn on this screen, for my own snake.
#include <stdint.h>
#include "../rendering/bst_renderer.h"

extern float skin_effect_time;             // seconds; set once per frame by the renderer

// How hot a body part is (0 cold .. 1 white hot) at a time: hotter at the head, with waves
// running down the body and a quick flicker.
float fire_heat(int index, int count, float t);
// Fire colour for a heat: deep red, red, orange, yellow, pale yellow.
void fire_color(float out[3], float heat);
void fire_body_color(float out[3], int index, int count, float t);

typedef struct {
  const float *x, *y, *angle;              // body parts in world units, head first; angle points tailwards
  const uint8_t* visible;                  // >= 1: drawn
  int count;
  float radius;                            // body radius in world units
  float view_x, view_y, scale, half_w, half_h;
  float alpha, strength;                   // snake opacity, flame size (0.2 .. 1.5)
} fire_body;

// Glow under the body and flames over it, as additive glow instances. Returns instances pushed.
int fire_skin_effects(bst_renderer* under, bst_renderer* over, const fire_body* body, float t);
#endif
