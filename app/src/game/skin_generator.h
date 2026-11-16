#ifndef SKIN_GENERATOR_H
#define SKIN_GENERATOR_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
  char code[257],topic[256],theories[1536];
  uint32_t theory_mask;
} generated_skin;
// Executes the bundled original color laboratory rules; no network or account required.
bool skin_generator_make(uint32_t seed,generated_skin* result);
#endif
