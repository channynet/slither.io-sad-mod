#ifndef PICTURE_WORKER_H
#define PICTURE_WORKER_H
// Tag pictures read and made ready on a thread of their own, so a frame never waits for a PNG to
// be decoded (a big NTL tag took several ms; a crowd of new tags at once made the game stutter).
// The main thread asks for pictures and takes the finished pixels, then makes the textures.
#include <stdbool.h>

enum { PICTURE_NTL_BOX = 0,           // an NTL tag: the image 21 px in from its box's corner, on a soft black shadow
       PICTURE_PLAIN = 1 };           // an image as it is (at most 512 px across)
typedef struct {
  int key;                            // the caller's name for it (dealt with one at a time per key)
  unsigned stamp;                     // the caller's version (handed back as it is)
  int kind, box_w, box_h;             // box: PICTURE_NTL_BOX only
  char path[320], fallback[320];      // where to read it (fallback: when path cannot be opened; may be "")
} picture_job;
typedef struct {
  int key; unsigned stamp;
  unsigned char* rgba;                // malloc'd, NULL when it could not be read
  int w, h;
} picture_result;

// Queues a picture (false when the queue is full, or this key is already on its way).
bool picture_worker_request(const picture_job* job);
bool picture_worker_pending(int key);
// A finished picture, if any (the caller frees rgba).
bool picture_worker_take(picture_result* out);
// The pixels for a job, made here and now (the thread runs this; tests too).
unsigned char* picture_make(const picture_job* job, int* w, int* h);
void picture_worker_stop(void);
#endif
