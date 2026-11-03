#include "picture_worker.h"
#include "tag_image.h"
#include "../platform/compat.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The picture as NTL draws it: the image 21 px in from the box's corner, on a soft black shadow
// (canvas shadowBlur 20: three box blurs of the alpha come close).
static void blur(float* a, int w, int h, int r) {
  float* tmp = malloc(sizeof(float) * (w > h ? w : h));
  for (int pass = 0; pass < 3; pass++) {
    for (int y = 0; y < h; y++) {                         // rows
      float* row = a + y * w, sum = 0;
      for (int x = -r; x <= r; x++) sum += x >= 0 && x < w ? row[x] : 0;
      for (int x = 0; x < w; x++) {
        tmp[x] = sum / (2 * r + 1);
        int out = x - r, in = x + r + 1;
        sum += (in < w ? row[in] : 0) - (out >= 0 ? row[out] : 0);
      }
      memcpy(row, tmp, sizeof(float) * w);
    }
    for (int x = 0; x < w; x++) {                         // columns
      float sum = 0;
      for (int y = -r; y <= r; y++) sum += y >= 0 && y < h ? a[y * w + x] : 0;
      for (int y = 0; y < h; y++) {
        tmp[y] = sum / (2 * r + 1);
        int out = y - r, in = y + r + 1;
        sum += (in < h ? a[in * w + x] : 0) - (out >= 0 ? a[out * w + x] : 0);
      }
      for (int y = 0; y < h; y++) a[y * w + x] = tmp[y];
    }
  }
  free(tmp);
}
unsigned char* picture_make(const picture_job* job, int* w_out, int* h_out) {
  tag_image src = {0};
  FILE* f = fopen(job->path, "rb");
  if (!f && job->fallback[0]) f = fopen(job->fallback, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); long length = ftell(f); rewind(f);
  unsigned char* bytes = length > 0 && length < 4 * 1024 * 1024 ? malloc(length) : NULL;
  bool read = bytes && fread(bytes, 1, length, f) == (size_t)length;
  fclose(f);
  bool decoded = read && tag_image_decode(bytes, length, &src);
  free(bytes);
  if (!decoded) return NULL;
  if (job->kind == PICTURE_PLAIN) { *w_out = src.width; *h_out = src.height; return src.rgba; }
  int w = job->box_w, h = job->box_h;
  if (w <= 0 || h <= 0 || w > 2048 || h > 2048) { tag_image_free(&src); return NULL; }
  float* shadow = calloc((size_t)w * h, sizeof(float));
  unsigned char* out = calloc((size_t)w * h, 4);
  if (!shadow || !out) { free(shadow); free(out); tag_image_free(&src); return NULL; }
  for (int y = 0; y < src.height; y++) for (int x = 0; x < src.width; x++) {
    int tx = x + 21, ty = y + 21;
    if (tx < w && ty < h) shadow[ty * w + tx] = src.rgba[(y * src.width + x) * 4 + 3] / 255.f;
  }
  blur(shadow, w, h, 6);
  for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
    unsigned char* o = out + (y * w + x) * 4;
    float sa = shadow[y * w + x], r = 0, g = 0, b = 0, a = 0;
    int sx = x - 21, sy = y - 21;
    if (sx >= 0 && sy >= 0 && sx < src.width && sy < src.height) {
      const unsigned char* p = src.rgba + (sy * src.width + sx) * 4;
      a = p[3] / 255.f; r = p[0] / 255.f; g = p[1] / 255.f; b = p[2] / 255.f;
    }
    float oa = a + sa * (1 - a);                          // the picture over its black shadow
    o[0] = (unsigned char)lroundf(oa > 0 ? 255 * r * a / oa : 0);
    o[1] = (unsigned char)lroundf(oa > 0 ? 255 * g * a / oa : 0);
    o[2] = (unsigned char)lroundf(oa > 0 ? 255 * b * a / oa : 0);
    o[3] = (unsigned char)lroundf(255 * oa);
  }
  free(shadow); tag_image_free(&src);
  *w_out = w; *h_out = h;
  return out;
}
// ---- the thread
#define QUEUE 32
static picture_job jobs[QUEUE]; static int job_count;
static picture_result done[QUEUE]; static int done_count;
static int busy_key = -1;
static CRITICAL_SECTION lock; static HANDLE wake, thread; static bool ready;
static volatile LONG stop;
static DWORD WINAPI worker(void* unused) {
  (void)unused;
  while (!InterlockedCompareExchange(&stop, 0, 0)) {
    EnterCriticalSection(&lock);
    if (!job_count || done_count >= QUEUE) { LeaveCriticalSection(&lock); WaitForSingleObject(wake, 250); continue; }
    picture_job job = jobs[0];
    memmove(jobs, jobs + 1, sizeof(picture_job) * (size_t)--job_count);
    busy_key = job.key;
    LeaveCriticalSection(&lock);
    picture_result r = {job.key, job.stamp, NULL, 0, 0};
    r.rgba = picture_make(&job, &r.w, &r.h);
    EnterCriticalSection(&lock);
    done[done_count++] = r; busy_key = -1;
    LeaveCriticalSection(&lock);
  }
  return 0;
}
static bool start(void) {
  if (ready) return true;
  InitializeCriticalSection(&lock);
  wake = CreateEventW(NULL, FALSE, FALSE, NULL);
  thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
  ready = thread != NULL;
  return ready;
}
static bool pending_locked(int key) {
  if (busy_key == key) return true;
  for (int i = 0; i < job_count; i++) if (jobs[i].key == key) return true;
  for (int i = 0; i < done_count; i++) if (done[i].key == key) return true;
  return false;
}
bool picture_worker_request(const picture_job* job) {
  if (!start()) return false;
  EnterCriticalSection(&lock);
  bool ok = job_count < QUEUE && !pending_locked(job->key);
  if (ok) jobs[job_count++] = *job;
  LeaveCriticalSection(&lock);
  if (ok) SetEvent(wake);
  return ok;
}
bool picture_worker_pending(int key) {
  if (!ready) return false;
  EnterCriticalSection(&lock); bool p = pending_locked(key); LeaveCriticalSection(&lock);
  return p;
}
bool picture_worker_take(picture_result* out) {
  if (!ready) return false;
  EnterCriticalSection(&lock);
  bool got = done_count > 0;
  if (got) { *out = done[0]; memmove(done, done + 1, sizeof(picture_result) * (size_t)--done_count); }
  LeaveCriticalSection(&lock);
  if (got) SetEvent(wake);
  return got;
}
void picture_worker_stop(void) {
  if (!ready) return;
  InterlockedExchange(&stop, 1); SetEvent(wake);
  WaitForSingleObject(thread, 3000); CloseHandle(thread);
  EnterCriticalSection(&lock);
  for (int i = 0; i < done_count; i++) free(done[i].rgba);
  done_count = job_count = 0;
  LeaveCriticalSection(&lock);
  ready = false; InterlockedExchange(&stop, 0);
}
