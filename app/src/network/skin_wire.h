#ifndef SKIN_WIRE_H
#define SKIN_WIRE_H

#include <stddef.h>
#include <stdint.h>

/* The snake packet carries the custom-skin length in one byte. Eight bytes
 * belong to its header; each colour run needs two more bytes. */
#define SKIN_WIRE_RUNS 123
#define SKIN_WIRE_BYTES (SKIN_WIRE_RUNS * 2)

static inline size_t skin_wire_pack(const uint8_t* colors, size_t count,
                                   uint8_t out[SKIN_WIRE_BYTES]) {
  size_t used = 0;
  for (size_t i = 0; i < count; i++) {
    if (used && out[used - 1] == colors[i] && out[used - 2] < 255) {
      out[used - 2]++;
    } else {
      if (used == SKIN_WIRE_BYTES) return 0;
      out[used++] = 1;
      out[used++] = colors[i];
    }
  }
  return used;
}

/* Keep exact colours whenever they fit. Otherwise sample across the whole
 * design, keeping its length and original saved code instead of cutting off
 * its tail. Every bucket represents two or three segments for a 250-char skin. */
static inline size_t skin_wire_encode(const uint8_t* colors, size_t count,
                                     uint8_t out[SKIN_WIRE_BYTES]) {
  size_t used = skin_wire_pack(colors, count, out);
  if (used || !count) return used;
  used = 0;
  for (size_t i = 0; i < SKIN_WIRE_RUNS; i++) {
    size_t start = i * count / SKIN_WIRE_RUNS;
    size_t end = (i + 1) * count / SKIN_WIRE_RUNS;
    uint8_t color = colors[(start + end - 1) / 2];
    out[used++] = (uint8_t)(end - start);
    out[used++] = color;
  }
  return used;
}

#endif
