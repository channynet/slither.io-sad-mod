// Tag pictures made on their own thread: an NTL box with its shadow, a plain image, the queue.
#include "../app/src/ui/picture_worker.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#define nap() Sleep(5)
#else
#include <unistd.h>
#define nap() usleep(5000)
#endif

int main(int argc, char** argv) {
  assert(argc > 1);
  picture_job box = {.key = 5, .stamp = 7, .kind = PICTURE_NTL_BOX, .box_w = 300, .box_h = 260};
  snprintf(box.path, sizeof box.path, "missing.png");
  snprintf(box.fallback, sizeof box.fallback, "%s", argv[1]);          // (the fallback is read)
  int w = 0, h = 0;
  unsigned char* px = picture_make(&box, &w, &h);
  assert(px && w == 300 && h == 260);
  assert(px[3] == 0);                                    // the corner: nothing
  bool shadow = false;
  for (int y = 0; y < h && !shadow; y++) for (int x = 0; x < 21 && !shadow; x++) if (px[(y * w + x) * 4 + 3] > 0) shadow = true;
  assert(shadow);                                        // a soft shadow spills into the margin
  free(px);
  picture_job plain = {.key = 10001, .stamp = 3, .kind = PICTURE_PLAIN};
  snprintf(plain.path, sizeof plain.path, "%s", argv[1]);
  // the queue: both made, handed back with their keys and stamps; a key already on its way is refused
  assert(picture_worker_request(&box) && picture_worker_request(&plain) && !picture_worker_request(&plain));
  int got = 0;
  for (int i = 0; i < 2000 && got < 2; i++) {
    picture_result r;
    while (picture_worker_take(&r)) {
      assert((r.key == 5 && r.stamp == 7 && r.w == 300) || (r.key == 10001 && r.stamp == 3 && r.w > 0 && r.w <= 512));
      assert(r.rgba); free(r.rgba); got++;
    }
    if (got < 2) nap();
  }
  assert(got == 2 && !picture_worker_pending(5));
  picture_job none = {.key = 6, .kind = PICTURE_PLAIN}; snprintf(none.path, sizeof none.path, "missing.png");
  assert(picture_worker_request(&none));
  picture_result r = {0}; int tries = 0;
  while (!picture_worker_take(&r) && tries++ < 2000) nap();
  assert(r.key == 6 && !r.rgba);                         // (a picture that cannot be read: handed back empty)
  picture_worker_stop();
  printf("Picture worker: NTL box, plain image, queue passed\n");
  return 0;
}
