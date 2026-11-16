#include "tag_share.h"
#include "http_simple.h"
#include "../platform/compat.h"
#include "../user.h"
#include "../game/ntl_tags.h"
#include "../ui/personal_tag.h"
#include "../game/native_preferences.h"
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
#define HAVE_MAX (MAX_ENTRIES * 2) // A tag and a cosmetic for every visible player.

typedef struct {
  bool want;                          // playing, sharing on, a relay set
  char url[160], server[200];
  int sid;
  char tag[200];                      // the "tag" JSON for the room ("null": none)
  char hash[2][17];
  char appearance[768];
  int generation;
} wish;
static struct {
  bool ready;
  CRITICAL_SECTION lock;
  HANDLE wake, thread;
  volatile LONG stop;
  wish w;
  struct { unsigned char* data; size_t length; char hash[17]; } images[2];
  share_entry list[MAX_ENTRIES]; int list_count, list_generation; char list_server[200];
  char asked[16][17]; int asked_count;          // pictures the main thread wants
  char have[HAVE_MAX][17]; int have_count, have_next; // bounded cache of pictures on disk
  char failed[32][17]; int failed_count;
  char status[96];
} S;
// main-thread copies
static share_entry list[MAX_ENTRIES]; static int list_count, list_generation = -1;
static char list_server[200], my_server[200];
static ULONGLONG list_at;

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
static void remember_picture(const char* hash) { // caller holds S.lock
  if (in_set(S.have, S.have_count, hash)) return;
  memcpy(S.have[S.have_next], hash, 17);
  S.have_next = (S.have_next + 1) % HAVE_MAX;
  if (S.have_count < HAVE_MAX) S.have_count++;
}

static DWORD WINAPI worker(void* unused) {
  (void)unused;
  char uploaded[2][17] = {{0}};
  char uploaded_url[160]="";
  make_dir(TAG_SHARE_DIR);
  while (!InterlockedCompareExchange(&S.stop, 0, 0)) {
    EnterCriticalSection(&S.lock);
    wish w = S.w;
    if(strcmp(uploaded_url,w.url)) { memset(uploaded,0,sizeof(uploaded)); snprintf(uploaded_url,sizeof(uploaded_url),"%s",w.url); }
    unsigned char* png[2]={0}; size_t png_length[2]={0};
    for(int i=0;i<2;i++) if(w.want && strcmp(uploaded[i],w.hash[i]) && S.images[i].data && !strcmp(S.images[i].hash,w.hash[i])) {
      png[i]=malloc(S.images[i].length);
      if(png[i]) { memcpy(png[i],S.images[i].data,S.images[i].length); png_length[i]=S.images[i].length; }
    }
    char asked[16][17]; int asked_count = S.asked_count;
    memcpy(asked, S.asked, sizeof asked);
    LeaveCriticalSection(&S.lock);
    char base[200]; base_url(w.url, base, sizeof base);
    if (w.want) {
      for(int i=0;i<2 && !InterlockedCompareExchange(&S.stop,0,0);i++) if(w.hash[i][0] && png[i]) {
        char url[300];snprintf(url,sizeof(url),"%s/v1/img/%s",base,w.hash[i]);
        int status=http_request("PUT",url,"image/png",png[i],png_length[i],NULL,NULL,0,8000);
        if(status==200) memcpy(uploaded[i],w.hash[i],17);
      }
      if(InterlockedCompareExchange(&S.stop,0,0)) {
        for(int i=0;i<2;i++) free(png[i]);
        break;
      }
      char server[600], url[900], body[1200];
      url_escape(w.server, server, sizeof server);
      snprintf(url, sizeof url, "%s/v1/room?s=%s", base, server);
      snprintf(body, sizeof body, "{\"sid\":%d,\"tag\":%s,\"appearance\":%s}", w.sid, w.tag,w.appearance);
      unsigned char* reply = NULL; size_t length = 0;
      int status = http_request("POST", url, "application/json", body, strlen(body), &reply, &length, 256 * 1024, 6000);
      share_entry got[MAX_ENTRIES]; int n = 0;
      if (status == 200 && reply) n = tag_share_parse((const char*)reply, length, got, MAX_ENTRIES);
      free(reply);
      EnterCriticalSection(&S.lock);
      if (status == 200 && S.w.generation==w.generation) {
        memcpy(S.list, got, sizeof(share_entry) * (size_t)n); S.list_count = n; S.list_generation++;
        snprintf(S.list_server, sizeof S.list_server, "%s", w.server);
        int others = 0; for (int i = 0; i < n; i++) if (got[i].sid != w.sid) others++;
        snprintf(S.status, sizeof S.status, "연결됨 · 이 서버의 외형 공유 %d명", others);
      } else if(S.w.generation==w.generation) snprintf(S.status, sizeof S.status, status ? "공유 서버 오류 (%d)" : "공유 서버에 연결하지 못함", status);
      LeaveCriticalSection(&S.lock);
    }
    // pictures asked for: from the relay, kept on disk
    for (int i = 0; i < asked_count && w.url[0] && !InterlockedCompareExchange(&S.stop,0,0); i++) {
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
        if (!here && (status == 0 || status == 404 || status >= 500)) continue; // upload/network may still be pending
      }
      EnterCriticalSection(&S.lock);
      if (!here && strcmp(S.w.url,w.url)) { LeaveCriticalSection(&S.lock); continue; }
      if (here) remember_picture(asked[i]);
      else if (S.failed_count < 32) memcpy(S.failed[S.failed_count++], asked[i], 17);
      for (int k = 0; k < S.asked_count; k++) if (!strcmp(S.asked[k], asked[i])) { memmove(S.asked[k], S.asked[k + 1], 17 * (size_t)(S.asked_count - k - 1)); S.asked_count--; break; }
      LeaveCriticalSection(&S.lock);
    }
    for(int i=0;i<2;i++) free(png[i]);
    // wait for the next poll, or until the wish changes / a picture is asked for
    if (!InterlockedCompareExchange(&S.stop,0,0)) WaitForSingleObject(S.wake, w.want ? POLL_MS : 1000);
  }
  return 0;
}
static void start(void) {
  if (S.ready) return;
  S.ready = true;
  InitializeCriticalSection(&S.lock);
  S.wake = CreateEventW(NULL, FALSE, FALSE, NULL);
  snprintf(S.status, sizeof S.status, "꺼짐");
  if (S.wake) S.thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
  if (!S.thread) snprintf(S.status, sizeof S.status, "공유 작업을 시작하지 못함");
}

void tag_share_tick(tenv* env) {
  start();
  game_data* g = &env->usr->gdata;
  snake* me = g->curr_screen == PLAYING && g->conn == CONNECTED && g->data.follow_view ? get_snake(g, g->data.snake_id) : NULL;
  const char* url = ntl_tag_prefs.share_url[0] ? ntl_tag_prefs.share_url : TAG_SHARE_DEFAULT_URL;
  wish w = {0};
  w.want = ntl_tag_prefs.share && url[0] && me && !me->dead;
  snprintf(my_server, sizeof my_server, "%s", env->usr->usrs.ipv4);
  share_image image[2] = {0};
  if (w.want) {
    snprintf(w.url, sizeof w.url, "%s", url);
    snprintf(w.server, sizeof w.server, "%s", env->usr->usrs.ipv4);
    w.sid = g->data.snake_id;
    snprintf(w.tag, sizeof w.tag, "null");
    if (ntl_tag_prefs.mode == 1) {
      int t = ntl_tag_own_local();                    // (a bought tag worn without its password too)
      if (t >= 0) snprintf(w.tag, sizeof w.tag, "[\"ntl\",%d]", t);
    } else if (personal_tag_share(&image[0])) {
      snprintf(w.tag, sizeof w.tag, "[\"img\",\"%s\",%.1f,%.1f,%.1f,%d]", image[0].hash, image[0].size, image[0].gap, image[0].angle, image[0].sway ? 1 : 0);
      memcpy(w.hash[0], image[0].hash, 17);
    }
    char cosmetic[200]="null",gradient[256]="null";
    if(personal_cosmetic_share(&image[1])) {
      memcpy(w.hash[1],image[1].hash,17);
      snprintf(cosmetic,sizeof(cosmetic),"[\"%s\",%.1f,%.1f,%.1f,%d]",image[1].hash,image[1].size,image[1].gap,image[1].angle,image[1].sway?1:0);
    }
    if(native_prefs.gradient_enabled) {
      int n=native_prefs.gradient_count; if(n<2 || n>4) n=2;
      int at=snprintf(gradient,sizeof(gradient),"[%d",n);
      for(int i=0;i<n*3;i++) at+=snprintf(gradient+at,sizeof(gradient)-at,",%.3f",native_prefs.gradient_colors[i/3][i%3]);
      snprintf(gradient+at,sizeof(gradient)-at,"]");
    }
    snprintf(w.appearance,sizeof(w.appearance),"{\"fire\":%.3f,\"gradient\":%s,\"cosmetic\":%s}",native_prefs.fire_skin?native_prefs.fire_strength:0,gradient,cosmetic);
  } else if (url[0]) snprintf(w.url, sizeof w.url, "%s", url);
  EnterCriticalSection(&S.lock);
  bool same = S.w.want == w.want && !strcmp(S.w.url, w.url) && !strcmp(S.w.server, w.server) && S.w.sid == w.sid && !strcmp(S.w.tag, w.tag) && !strcmp(S.w.appearance,w.appearance);
  if (!same) {
    int generation = S.w.generation + 1;
    if (strcmp(S.w.url,w.url)) S.failed_count = 0;
    S.w = w; S.w.generation = generation;
    if (!w.want) { S.list_count = 0; S.list_generation++; snprintf(S.status, sizeof S.status, url[0] ? (ntl_tag_prefs.share ? "게임 중에만 공유" : "꺼짐") : "공유 서버 주소 없음"); }
  }
  for(int i=0;i<2;i++) if(w.hash[i][0] && strcmp(S.images[i].hash,w.hash[i]) && image[i].png && image[i].length) {
    unsigned char* copy=malloc(image[i].length);
    if(copy) { memcpy(copy,image[i].png,image[i].length);free(S.images[i].data);S.images[i].data=copy;
      S.images[i].length=image[i].length;memcpy(S.images[i].hash,w.hash[i],17); }
  }
  if (S.list_generation != list_generation) {
    memcpy(list, S.list, sizeof(share_entry) * (size_t)S.list_count); list_count = S.list_count;
    list_generation = S.list_generation; list_at=GetTickCount64();
    snprintf(list_server, sizeof list_server, "%s", S.list_server);
  }
  LeaveCriticalSection(&S.lock);
  if (!same) SetEvent(S.wake);
}
bool tag_share_lookup(int sid, share_entry* out) {
  if (!ntl_tag_prefs.share || strcmp(list_server, my_server) || GetTickCount64()-list_at>12000) return false;
  for (int i = 0; i < list_count; i++) if (list[i].sid == sid) { *out = list[i]; return true; }
  return false;
}
bool tag_share_image_path(const char* hash, char* path, size_t cap) {
  if (!S.ready || !S.thread || !hash || strlen(hash)!=16 || strspn(hash,"0123456789abcdef")!=16) return false;
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
  if (S.thread) { WaitForSingleObject(S.thread, INFINITE); CloseHandle(S.thread); }
  if (S.wake) CloseHandle(S.wake);
  for(int i=0;i<2;i++) free(S.images[i].data);
  DeleteCriticalSection(&S.lock);
  memset(&S,0,sizeof(S));
  list_count=0; list_generation=-1; list_at=0;
  list_server[0]=my_server[0]=0;
}
