#include "screenshot.h"
#include "toast.h"
#include "../platform/compat.h"
#include "external/stb/stb_image_write.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define make_folder(p) _mkdir(p)
#define SEP "\\"
#else
#define make_folder(p) mkdir(p, 0755)
#define SEP "/"
#endif

typedef struct { unsigned char* pixels; int w, h; bool bgra; char path[260]; } job;
static volatile LONG busy;              // a PNG is being written
static volatile LONG done;              // 1 saved, 2 failed: the saver tells the next frame
static char done_path[260];

void screenshot_name(char* out, size_t capacity, int year, int month, int day, int hour, int minute, int second, int serial) {
  if (serial > 1) snprintf(out, capacity, "screenshots" SEP "slither-%04d%02d%02d-%02d%02d%02d-%d.png", year, month, day, hour, minute, second, serial);
  else snprintf(out, capacity, "screenshots" SEP "slither-%04d%02d%02d-%02d%02d%02d.png", year, month, day, hour, minute, second);
}

static DWORD WINAPI save(void* arg) {
  job* j = arg;
  size_t count = (size_t)j->w * j->h;          // to RGBA, opaque (here, not on the game thread)
  for (size_t i = 0; i < count; i++) {
    unsigned char* p = j->pixels + i * 4;
    if (j->bgra) { unsigned char t = p[0]; p[0] = p[2]; p[2] = t; }
    p[3] = 255;
  }
  stbi_write_png_compression_level = 6;
  int ok = stbi_write_png(j->path, j->w, j->h, 4, j->pixels, j->w * 4);
  snprintf(done_path, sizeof(done_path), "%s", j->path);
  free(j->pixels); free(j);
  InterlockedExchange(&done, ok ? 1 : 2);
  InterlockedExchange(&busy, 0);
  return 0;
}

void screenshot_request(tenv* env) {
  if (busy || env->ctx->capture_request || env->ctx->capture_pending || env->ctx->capture_ready) return;
  if (!env->ctx->can_capture) { toast_show("이 그래픽 환경에서는 스크린샷을 찍을 수 없습니다", 1, .55f, .45f, 3); return; }
  env->ctx->capture_request = true;
}

void screenshot_frame(tenv* env) {
  tcontext* ctx = env->ctx;
  LONG result = InterlockedExchange(&done, 0);
  if (result == 2)            // saved: no notice (only a failure is shown) toast_show("스크린샷을 저장하지 못했습니다 (screenshots 폴더를 확인해 주세요)", 1, .45f, .4f, 4);
  if (ctx->capture_failed) { ctx->capture_failed = false; toast_show("스크린샷을 찍지 못했습니다", 1, .45f, .4f, 3); }
  if (!ctx->capture_ready) return;
  job* j = malloc(sizeof(job));
  if (!j) { free(ctx->capture_pixels); ctx->capture_pixels = NULL; ctx->capture_ready = false; return; }
  j->pixels = ctx->capture_pixels; j->w = ctx->capture_w; j->h = ctx->capture_h; j->bgra = ctx->capture_bgra;
  ctx->capture_pixels = NULL; ctx->capture_ready = false;
  make_folder("screenshots");
  time_t now = time(NULL); struct tm local;
#ifdef _WIN32
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  for (int serial = 1; serial < 100; serial++) {
    screenshot_name(j->path, sizeof(j->path), local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec, serial);
    struct stat st; if (stat(j->path, &st) != 0) break;
  }
  InterlockedExchange(&busy, 1);
  HANDLE thread = CreateThread(NULL, 0, save, j, 0, NULL);
  if (thread) CloseHandle(thread);
  else save(j);
}
