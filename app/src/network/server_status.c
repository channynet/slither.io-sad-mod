#include "server_status.h"
#include "server_address.h"
#include "socket_latency.h"
#include "../external/mongoose.h"
#include "../platform/compat.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#define STATUS_LIMIT 32
#define PROBE_TIMEOUT 1500
#define PROBE_TTL 20000
static struct entry {
  char address[MAX_IPV4_LEN+1];
  server_status value;
  ULONGLONG attempted,used;
} entries[STATUS_LIMIT];
static int count;
static CRITICAL_SECTION lock;
static HANDLE wake,thread;
static volatile LONG stopping,menu,dirty;
static bool initialized;
static const char* cache_path="server-status.txt";
static bool clean_address(const char* address,char* clean) {
  server_history h={0}; server_history_add(&h,address);
  if(!h.count) return false; strcpy(clean,h.addresses[0]); return true;
}
// Caller holds the lock. Exact host:port identity is retained.
static int find(const char* address,bool create) {
  for(int i=0;i<count;i++) if(!strcmp(entries[i].address,address)) return i;
  if(!create) return -1;
  int i=count;
  if(count<STATUS_LIMIT) count++;
  else { i=0; for(int j=1;j<count;j++) if(entries[j].used<entries[i].used) i=j; }
  memset(&entries[i],0,sizeof(entries[i])); strcpy(entries[i].address,address);
  entries[i].value.players=entries[i].value.ping_ms=-1;
  entries[i].used=GetTickCount64(); return i;
}
static void save(void) {
  struct entry copy[STATUS_LIMIT]; int n;
  EnterCriticalSection(&lock); n=count; memcpy(copy,entries,sizeof(copy)); LeaveCriticalSection(&lock);
  FILE* f=fopen("server-status.txt.tmp","wb"); if(!f) return;
  bool ok=true;
  for(int i=0;i<n;i++) if(copy[i].value.players>=0)
    if(fprintf(f,"%s %d %lld\n",copy[i].address,copy[i].value.players,(long long)copy[i].value.players_at)<0) ok=false;
  if(fclose(f)) ok=false;
  if(ok) MoveFileExA("server-status.txt.tmp",cache_path,MOVEFILE_REPLACE_EXISTING);
}
static void load(void) {
  FILE* f=fopen(cache_path,"rb"); if(!f) return;
  char line[200],address[MAX_IPV4_LEN+1],clean[MAX_IPV4_LEN+1],extra;
  int players; long long stamp; int64_t now=(int64_t)time(NULL);
  while(fgets(line,sizeof(line),f)) {
    if(sscanf(line,"%67s %d %lld %c",address,&players,&stamp,&extra)!=3 ||
       players<0 || players>65535 || stamp<=0 || stamp>now || !clean_address(address,clean)) continue;
    int i=find(clean,true); entries[i].value.players=players; entries[i].value.players_at=stamp;
  }
  fclose(f);
}
static double precise_ms(void) {
  LARGE_INTEGER q,f; QueryPerformanceCounter(&q); QueryPerformanceFrequency(&f);
  return (double)q.QuadPart*1000.0/f.QuadPart;
}
typedef struct { double sent; int samples; float values[3]; bool done; } probe;
static void probe_callback(struct mg_connection* c,int ev,void* data) {
  probe* p=c->fn_data;
  if(ev==MG_EV_CONNECT) socket_prompt_ack((SOCKET)(uintptr_t)c->fd);
  if(ev==MG_EV_WS_OPEN) {
    p->sent=precise_ms(); mg_ws_send(c,"p",1,WEBSOCKET_OP_BINARY);
  } else if(ev==MG_EV_WS_MSG) {
    if(p->done || p->samples>=3) { c->is_closing=true; return; }
    struct mg_ws_message* msg=data;
    if(msg->data.len!=1 || msg->data.buf[0]!='p') { p->done=true; c->is_closing=true; return; }
    p->values[p->samples++]=(float)(precise_ms()-p->sent);
    if(p->samples==3) { p->done=true; c->is_closing=true; }
    else { p->sent=precise_ms(); mg_ws_send(c,"p",1,WEBSOCKET_OP_BINARY); }
  } else if(ev==MG_EV_ERROR || ev==MG_EV_CLOSE) { p->done=true; c->is_closing=true; }
  if(c->recv.len>4096) { p->done=true; c->is_closing=true; }
}
static int measure(const char* address) {
  struct mg_mgr mgr; mg_mgr_init(&mgr);
  probe p={0}; char url[160],host[MAX_IPV4_LEN+1];
  struct mg_connection* c=server_address_url(address,"/ptc",url,sizeof(url),host,sizeof(host),PROBE_TIMEOUT)
    ? mg_ws_connect(&mgr,url,probe_callback,&p,"Origin: https://slither.com\r\n") : NULL;
  server_address_host(c,host);
  ULONGLONG start=GetTickCount64();
  while(c && !p.done && GetTickCount64()-start<PROBE_TIMEOUT &&
        !InterlockedCompareExchange(&stopping,0,0) && InterlockedCompareExchange(&menu,0,0)) mg_mgr_poll(&mgr,10);
  mg_mgr_free(&mgr);
  if(p.samples!=3) return -1;
  for(int i=0;i<2;i++) for(int j=i+1;j<3;j++) if(p.values[j]<p.values[i]) { float t=p.values[i];p.values[i]=p.values[j];p.values[j]=t; }
  return (int)lroundf(p.values[1]);
}
static DWORD WINAPI worker(void* unused) {
  (void)unused; ULONGLONG last_save=0;
  while(!InterlockedCompareExchange(&stopping,0,0)) {
    char address[MAX_IPV4_LEN+1]={0};
    EnterCriticalSection(&lock);
    for(int i=0;i<count;i++) if(entries[i].value.pending) {
      if(!InterlockedCompareExchange(&menu,0,0)) entries[i].value.pending=false;
      else { strcpy(address,entries[i].address); break; }
    }
    LeaveCriticalSection(&lock);
    if(*address) {
      int ms=measure(address);
      EnterCriticalSection(&lock); int i=find(address,false);
      if(i>=0) {
        server_status* v=&entries[i].value; v->pending=false;
        if(InterlockedCompareExchange(&menu,0,0)) {
          v->failed=ms<0;
          if(ms>=0) { v->ping_ms=ms; v->ping_at=(int64_t)time(NULL); v->game_ping=false; }
        }
      }
      LeaveCriticalSection(&lock);
    }
    if(GetTickCount64()-last_save>5000 && InterlockedExchange(&dirty,0)) { save(); last_save=GetTickCount64(); }
    if(!*address) WaitForSingleObject(wake,250);
  }
  save(); return 0;
}
void server_status_init(void) {
  if(initialized) return;
  memset(entries,0,sizeof(entries)); count=0; stopping=menu=dirty=0;
  InitializeCriticalSection(&lock); initialized=true; load();
  wake=CreateEventW(NULL,FALSE,FALSE,NULL);
  if(wake) thread=CreateThread(NULL,0,worker,NULL,0,NULL);
}
void server_status_destroy(void) {
  if(!initialized) return;
  InterlockedExchange(&stopping,1); if(wake) SetEvent(wake);
  if(thread) { WaitForSingleObject(thread,INFINITE); CloseHandle(thread); } else save();
  if(wake) CloseHandle(wake); thread=wake=NULL; initialized=false; DeleteCriticalSection(&lock);
}
void server_status_menu(bool allowed) { InterlockedExchange(&menu,allowed?1:0); }
void server_status_request(const char* address) {
  if(!initialized || !thread || !InterlockedCompareExchange(&menu,0,0)) return;
  char clean[MAX_IPV4_LEN+1]; if(!clean_address(address,clean)) return;
  EnterCriticalSection(&lock); int i=find(clean,true); ULONGLONG now=GetTickCount64();
  struct entry* e=&entries[i]; e->used=now;
  if(!e->value.pending && (!e->attempted || now-e->attempted>=PROBE_TTL)) {
    e->value.pending=true; e->attempted=now; SetEvent(wake);
  }
  LeaveCriticalSection(&lock);
}
bool server_status_get(const char* address,server_status* out) {
  *out=(server_status){.players=-1,.ping_ms=-1};
  if(!initialized) return false;
  char clean[MAX_IPV4_LEN+1]; if(!clean_address(address,clean)) return false;
  EnterCriticalSection(&lock); int i=find(clean,false);
  if(i>=0) *out=entries[i].value;
  LeaveCriticalSection(&lock); return i>=0;
}
void server_status_population(const char* address,int players) {
  if(!initialized || players<0 || players>65535) return;
  char clean[MAX_IPV4_LEN+1]; if(!clean_address(address,clean)) return;
  EnterCriticalSection(&lock); int i=find(clean,true);
  entries[i].value.players=players; entries[i].value.players_at=(int64_t)time(NULL);
  entries[i].used=GetTickCount64(); LeaveCriticalSection(&lock); InterlockedExchange(&dirty,1);
}
void server_status_game_ping(const char* address,float ms) {
  if(!initialized || !isfinite(ms) || ms<0 || ms>60000) return;
  char clean[MAX_IPV4_LEN+1]; if(!clean_address(address,clean)) return;
  EnterCriticalSection(&lock); int i=find(clean,true);
  entries[i].value.ping_ms=(int)lroundf(ms); entries[i].value.ping_at=(int64_t)time(NULL);
  entries[i].value.game_ping=true; entries[i].value.failed=false; entries[i].used=GetTickCount64();
  LeaveCriticalSection(&lock);
}
