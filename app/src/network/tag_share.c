#include "tag_share.h"
#include "http_simple.h"
#include "../platform/compat.h"
#include "../user.h"
#include "../game/ntl_tags.h"
#include "../ui/personal_tag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define make_dir(p) _mkdir(p)
#else
#define make_dir(p) mkdir(p, 0755)
#endif

#ifndef TAG_SHARE_DEFAULT_URL
#define TAG_SHARE_DEFAULT_URL "https://slither-tag-share.sadxxx.workers.dev"   // the relay used when none is set (tools/tag-share-worker)
#endif
#define POLL_MS 4000
#define MAX_ENTRIES 256
#define HAVE_MAX 128

typedef struct {
  bool want;                          // playing, sharing on, a relay set
  char url[160], server[200];
  int sid;
  char tag[200];                      // the "tag" JSON for the room ("null": none)
  char hash[17];                      // my image tag's (or "")
  int generation;
} wish;
static struct {
  bool ready;
  CRITICAL_SECTION lock;
  HANDLE wake, thread;
  volatile LONG stop;
  wish w;
  unsigned char* png; size_t png_length; char png_hash[17];   // my image tag's picture (our own copy)
  share_entry list[MAX_ENTRIES]; int list_count, list_generation; char list_server[200];
  char asked[16][17]; int asked_count;          // pictures the main thread wants
  char have[HAVE_MAX][17]; int have_count;      // pictures on disk (found or fetched)
  char failed[32][17]; int failed_count;
  char status[96];
} S;
// main-thread copies
static share_entry list[MAX_ENTRIES]; static int list_count, list_generation = -1;
static char list_server[200], my_server[200];

static bool in_set(char (*set)[17], int count, const char* hash) {
  for (int i = 0; i < count; i++) if (!strcmp(set[i], hash)) return true;
  return false;
}
static void base_url(const char* url, char* out, size_t cap) {
  snprintf(out, cap, "%s", url);
  size_t n = strlen(out);
  while (n && out[n - 1] == '/') out[--n] = 0;
}
static void url_escape(const char* s, char* out, size_t cap) {
  size_t n = 0;
  for (; *s && n + 4 < cap; s++) {
    unsigned char c = (unsigned char)*s;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_') out[n++] = (char)c;
    else n += (size_t)snprintf(out + n, cap - n, "%%%02X", c);
  }
  out[n] = 0;
}
static void set_status(const char* text) {
  EnterCriticalSection(&S.lock); snprintf(S.status, sizeof S.status, "%s", text); LeaveCriticalSection(&S.lock);
}
static void picture_path(const char* hash, char* out, size_t cap) { snprintf(out, cap, TAG_SHARE_DIR "/%s.png", hash); }

static DWORD WINAPI worker(void* unused) {
  (void)unused;
  char uploaded[17] = "";
  make_dir(TAG_SHARE_DIR);
  while (!InterlockedCompareExchange(&S.stop, 0, 0)) {
    EnterCriticalSection(&S.lock);
    wish w = S.w;
    unsigned char* png = NULL; size_t png_length = 0;
    if (S.png && !strcmp(S.png_hash, w.hash)) { png = malloc(S.png_length); if (png) { memcpy(png, S.png, S.png_length); png_length = S.png_length; } }
    char asked[16][17]; int asked_count = S.asked_count;
    memcpy(asked, S.asked, sizeof asked);
    LeaveCriticalSection(&S.lock);
    char base[200]; base_url(w.url, base, sizeof base);
    if (w.want) {
      // my image tag's picture, once a run
      if (w.hash[0] && png && strcmp(uploaded, w.hash)) {
        char url[300]; snprintf(url, sizeof url, "%s/v1/img/%s", base, w.hash);
        int status = http_request("PUT", url, "image/png", png, png_length, NULL, NULL, 0, 8000);
        if (status == 200) memcpy(uploaded, w.hash, 17);
      }
      char server[600], url[900], body[300];
      url_escape(w.server, server, sizeof server);
      snprintf(url, sizeof url, "%s/v1/room?s=%s", base, server);
      snprintf(body, sizeof body, "{\"sid\":%d,\"tag\":%s}", w.sid, w.tag);
      unsigned char* reply = NULL; size_t length = 0;
      int status = http_request("POST", url, "application/json", body, strlen(body), &reply, &length, 256 * 1024, 6000);
      share_entry got[MAX_ENTRIES]; int n = 0;
      if (status == 200 && reply) n = tag_share_parse((const char*)reply, length, got, MAX_ENTRIES);
      free(reply);
      EnterCriticalSection(&S.lock);
      if (status == 200) {
        memcpy(S.list, got, sizeof(share_entry) * (size_t)n); S.list_count = n; S.list_generation++;
        snprintf(S.list_server, sizeof S.list_server, "%s", w.server);
        int others = 0; for (int i = 0; i < n; i++) if (got[i].sid != w.sid) others++;
        snprintf(S.status, sizeof S.status, "연결됨 · 이 서버의 공유 태그 %d개", others);
      } else snprintf(S.status, sizeof S.status, status ? "공유 서버 오류 (%d)" : "공유 서버에 연결하지 못함", status);
      LeaveCriticalSection(&S.lock);
    }
    // pictures asked for: from the relay, kept on disk
    for (int i = 0; i < asked_count && w.url[0]; i++) {
      EnterCriticalSection(&S.lock);
      bool skip = in_set(S.have, S.have_count, asked[i]) || in_set(S.failed, S.failed_count, asked[i]);
      LeaveCriticalSection(&S.lock);
      if (skip) continue;
      char path[64]; picture_path(asked[i], path, sizeof path);
      FILE* f = fopen(path, "rb");
      bool here = f != NULL;
      if (f) fclose(f);
      if (!here) {
        char url[300]; snprintf(url, sizeof url, "%s/v1/img/%s", base, asked[i]);
        unsigned char* data = NULL; size_t length = 0;
        int status = http_request("GET", url, NULL, NULL, 0, &data, &length, 200 * 1024, 8000);
        char hash[17] = "";
        if (status == 200 && data) tag_share_hash(data, length, hash);
        if (!strcmp(hash, asked[i])) {
          char tmp[72]; snprintf(tmp, sizeof tmp, "%s.tmp", path);
          FILE* o = fopen(tmp, "wb");
          if (o) { here = fwrite(data, 1, length, o) == length; if (fclose(o)) here = false; }
          if (here) { remove(path); here = rename(tmp, path) == 0; }
          if (!here) remove(tmp);
        }
        free(data);
        if (!here && status == 0) continue;                  // (no connection: try again later)
      }
      EnterCriticalSection(&S.lock);
      if (here) { if (S.have_count < HAVE_MAX) memcpy(S.have[S.have_count++], asked[i], 17); }
      else if (S.failed_count < 32) memcpy(S.failed[S.failed_count++], asked[i], 17);
      for (int k = 0; k < S.asked_count; k++) if (!strcmp(S.asked[k], asked[i])) { memmove(S.asked[k], S.asked[k + 1], 17 * (size_t)(S.asked_count - k - 1)); S.asked_count--; break; }
      LeaveCriticalSection(&S.lock);
    }
    free(png);
    // wait for the next poll, or until the wish changes / a picture is asked for
    WaitForSingleObject(S.wake, w.want ? POLL_MS : 1000);
  }
  return 0;
}
static void start(void) {
  if (S.ready) return;
  S.ready = true;
  InitializeCriticalSection(&S.lock);
  S.wake = CreateEventW(NULL, FALSE, FALSE, NULL);
  snprintf(S.status, sizeof S.status, "꺼짐");
  S.thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
}

void tag_share_tick(tenv* env) {
  start();
  game_data* g = &env->usr->gdata;
  snake* me = g->curr_screen == PLAYING && g->conn == CONNECTED && g->data.follow_view ? get_snake(g, g->data.snake_id) : NULL;
  const char* url = ntl_tag_prefs.share_url[0] ? ntl_tag_prefs.share_url : TAG_SHARE_DEFAULT_URL;
  wish w = {0};
  w.want = ntl_tag_prefs.share && url[0] && me && !me->dead;
  snprintf(my_server, sizeof my_server, "%s", env->usr->usrs.ipv4);
  share_image image = {0};
  if (w.want) {
    snprintf(w.url, sizeof w.url, "%s", url);
    snprintf(w.server, sizeof w.server, "%s", env->usr->usrs.ipv4);
    w.sid = g->data.snake_id;
    snprintf(w.tag, sizeof w.tag, "null");
    if (ntl_tag_prefs.mode == 1) {
      int t = ntl_tag_own_local();                    // (a bought tag worn without its password too)
      if (t >= 0) snprintf(w.tag, sizeof w.tag, "[\"ntl\",%d]", t);
    } else if (personal_tag_share(&image)) {
      snprintf(w.tag, sizeof w.tag, "[\"img\",\"%s\",%.1f,%.1f,%.1f,%d]", image.hash, image.size, image.gap, image.angle, image.sway ? 1 : 0);
      memcpy(w.hash, image.hash, 17);
    }
  } else if (url[0]) snprintf(w.url, sizeof w.url, "%s", url);
  EnterCriticalSection(&S.lock);
  bool same = S.w.want == w.want && !strcmp(S.w.url, w.url) && !strcmp(S.w.server, w.server) && S.w.sid == w.sid && !strcmp(S.w.tag, w.tag);
  if (!same) {
    int generation = S.w.generation + 1;
    S.w = w; S.w.generation = generation;
    if (!w.want) { S.list_count = 0; S.list_generation++; snprintf(S.status, sizeof S.status, url[0] ? (ntl_tag_prefs.share ? "게임 중에만 공유" : "꺼짐") : "공유 서버 주소 없음"); }
  }
  if (w.hash[0] && strcmp(S.png_hash, w.hash) && image.png && image.length) {   // (a copy: personal_tag may let go of its own)
    unsigned char* copy = malloc(image.length);
    if (copy) { memcpy(copy, image.png, image.length); free(S.png); S.png = copy; S.png_length = image.length; memcpy(S.png_hash, w.hash, 17); }
  }
  if (S.list_generation != list_generation) {
    memcpy(list, S.list, sizeof(share_entry) * (size_t)S.list_count); list_count = S.list_count;
    list_generation = S.list_generation;
    snprintf(list_server, sizeof list_server, "%s", S.list_server);
  }
  LeaveCriticalSection(&S.lock);
  if (!same) SetEvent(S.wake);
}
bool tag_share_lookup(int sid, share_entry* out) {
  if (!ntl_tag_prefs.share || strcmp(list_server, my_server)) return false;
  for (int i = 0; i < list_count; i++) if (list[i].sid == sid) { *out = list[i]; return true; }
  return false;
}
bool tag_share_image_path(const char* hash, char* path, size_t cap) {
  if (!S.ready) return false;
  picture_path(hash, path, cap);
  EnterCriticalSection(&S.lock);
  bool have = in_set(S.have, S.have_count, hash), failed = in_set(S.failed, S.failed_count, hash);
  bool ask = !have && !failed && !in_set(S.asked, S.asked_count, hash) && S.asked_count < 16;
  if (ask) memcpy(S.asked[S.asked_count++], hash, 17);
  LeaveCriticalSection(&S.lock);
  if (ask) SetEvent(S.wake);
  return have;
}
const char* tag_share_status(void) {
  static char copy[96];
  if (!S.ready) return "꺼짐";
  EnterCriticalSection(&S.lock); snprintf(copy, sizeof copy, "%s", S.status); LeaveCriticalSection(&S.lock);
  return copy;
}
void tag_share_destroy(void) {
  if (!S.ready) return;
  InterlockedExchange(&S.stop, 1);
  SetEvent(S.wake);
  if (S.thread) { WaitForSingleObject(S.thread, 8000); CloseHandle(S.thread); S.thread = NULL; }
}
