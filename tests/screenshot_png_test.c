// Screenshot PNGs: what the writer saves reads back exactly.
#include "external/stb/stb_image_write.h"
#include "external/stb/stb_image.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(void) {
  int w = 321, h = 177;
  unsigned char* px = malloc((size_t)w * h * 4);
  for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
    unsigned char* p = px + ((size_t)y * w + x) * 4;
    p[0] = (unsigned char)(x * 7); p[1] = (unsigned char)(y * 3); p[2] = (unsigned char)((x ^ y) & 255); p[3] = 255;
  }
  assert(stbi_write_png("shot.png", w, h, 4, px, w * 4));
  int rw, rh, n; unsigned char* back = stbi_load("shot.png", &rw, &rh, &n, 4);
  assert(back && rw == w && rh == h);
  for (size_t i = 0; i < (size_t)w * h * 4; i++) assert(back[i] == px[i]);
  stbi_image_free(back); free(px);
  puts("Screenshot PNG: write and read back identical passed");
  return 0;
}
