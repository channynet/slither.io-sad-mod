#include <stdlib.h>
#include "../app/src/external/mongoose.h"
#include "../app/src/platform/compat.h"
#include <assert.h>
static volatile LONG copies;
static void* counted_malloc(size_t n) { InterlockedIncrement(&copies); return malloc(n); }
#define malloc counted_malloc
#include "../app/src/network/tag_share.c"
#undef malloc

native_preferences native_prefs;
ntl_tag_settings ntl_tag_prefs;
snake* get_snake(game_data* g,int id) { (void)g; (void)id; return NULL; }
int ntl_tag_own_local(void) { return -1; }
bool personal_tag_share(share_image* out) { (void)out; return false; }
bool personal_cosmetic_share(share_image* out) { (void)out; return false; }

static volatile LONG posts, uploads;
int http_request(const char* method,const char* url,const char* type,const void* body,size_t body_len,
                 unsigned char** out,size_t* length,size_t limit,int timeout) {
  (void)url; (void)type; (void)body; (void)body_len; (void)limit; (void)timeout;
  if(!strcmp(method,"PUT")) { InterlockedIncrement(&uploads); return 200; }
  assert(!strcmp(method,"POST"));
  *out=malloc(9); memcpy(*out,"{\"p\":[]}",9); *length=8;
  InterlockedIncrement(&posts); return 200;
}
static void wait_posts(LONG count) {
  ULONGLONG begin=GetTickCount64();
  while(InterlockedCompareExchange(&posts,0,0)<count) { assert(GetTickCount64()-begin<2000); Sleep(1); }
}
int main(void) {
  // More than a full cache must still make the newest image available.
  char hash[17],path[80];
  for(int i=0;i<HAVE_MAX+10;i++) {
    snprintf(hash,sizeof(hash),"%016x",i);
    remember_picture(hash);
    assert(in_set(S.have,S.have_count,hash));
  }
  assert(S.have_count==HAVE_MAX);
  int next=S.have_next; remember_picture(hash); assert(S.have_next==next);
  memset(&S,0,sizeof(S));
  start(); assert(S.thread);
  assert(!tag_share_image_path("../../invalid.png",path,sizeof(path)));
  assert(S.asked_count==0);
  EnterCriticalSection(&S.lock);
  S.w.want=true; strcpy(S.w.url,"https://relay.invalid");
  strcpy(S.w.server,"game:444"); strcpy(S.w.tag,"null"); strcpy(S.w.appearance,"{}");
  strcpy(S.w.hash[0],"0123456789abcdef");
  memcpy(S.images[0].hash,S.w.hash[0],17); S.images[0].length=16; S.images[0].data=calloc(1,16);
  LeaveCriticalSection(&S.lock);
  SetEvent(S.wake); wait_posts(1);
  for(int i=2;i<=4;i++) { SetEvent(S.wake); wait_posts(i); }
  assert(uploads==1 && copies==1); // unchanged image is neither copied nor uploaded again
  EnterCriticalSection(&S.lock); strcpy(S.w.url,"https://other-relay.invalid"); LeaveCriticalSection(&S.lock);
  SetEvent(S.wake); wait_posts(5); assert(uploads==2 && copies==2);
  tag_share_destroy();
  assert(!S.ready && !S.thread && !S.wake && !S.images[0].data);
  start(); assert(S.thread); tag_share_destroy(); // clean restart, no stale stop flag
  puts("Tag share worker: cache rollover, path validation, upload reuse, relay change and clean shutdown passed");
}
