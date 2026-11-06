#include "server_catalog.h"
#include "server_address.h"
#include "http_simple.h"
#include "../external/mongoose.h"
#include "../platform/compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

static server_catalog current;
static CRITICAL_SECTION lock;
static HANDLE thread, wake;
static volatile LONG stopping;
static bool initialized;
static ULONGLONG attempted;

static int integer(struct mg_str o, const char* path, int fallback, int limit) {
  double n;
  return mg_json_get_num(o,path,&n) && isfinite(n) && n>=0 && n<=limit && floor(n)==n ? (int)n : fallback;
}
static bool text_field(struct mg_str o,const char* path,char* dst,size_t cap) {
  char* text=mg_json_get_str(o,path);
  if(!text) return false;
  size_t n=strlen(text);
  if(n>=cap) { n=cap-1; while(n && ((unsigned char)text[n]&0xc0)==0x80) n--; }
  memcpy(dst,text,n); dst[n]=0; free(text);
  for(size_t i=0;i<n;i++) if((unsigned char)dst[i]<32 || dst[i]==127) dst[i]=' ';
  return true;
}
static bool array(struct mg_str o,const char* path,struct mg_str* result) {
  int length=0,at=mg_json_get(o,path,&length);
  if(at<0 || length<2 || o.buf[at]!='[' || o.buf[at+length-1]!=']') return false;
  *result=mg_str_n(o.buf+at,length); return true;
}
bool server_catalog_parse(const char* json,size_t length,server_catalog* out) {
  if(!json || !out || length>2*1024*1024) return false;
  struct mg_str root=mg_str_n(json,length),list,item;
  int root_len=0,root_at=mg_json_get(root,"$",&root_len);
  if(root_at<0 || root_len<2 || json[root_at]!='{') return false;
  for(size_t i=(size_t)root_at+root_len;i<length;i++) if(!strchr(" \t\n\r",json[i])) return false;
  char type[16]={0};
  if(!text_field(root,"$.type",type,sizeof(type)) || strcmp(type,"success") || !array(root,"$.dataList",&list)) return false;
  server_catalog* parsed=calloc(1,sizeof(*parsed)); if(!parsed) return false;
  size_t off=0;
  while((off=mg_json_next(list,off,NULL,&item))!=0 && parsed->count<SERVER_CATALOG_LIMIT) {
    server_catalog_entry row={.players=-1}; char ip[64]={0},address[96];
    int port=integer(item,"$.po",-1,65535);
    if(!text_field(item,"$.ipv4",ip,sizeof(ip)) || port<1) continue;
    struct mg_addr numeric;
    if(!mg_aton(mg_str(ip),&numeric) || numeric.is_ip6) continue;
    snprintf(address,sizeof(address),"%s:%d",ip,port);
    if(!server_address_clean(address,row.address)) continue;
    bool duplicate=false;
    for(int i=0;i<parsed->count;i++) if(!strcmp(parsed->entries[i].address,row.address)) duplicate=true;
    if(duplicate) continue;
    row.players=integer(item,"$.snakeCount",-1,1000000);
    text_field(item,"$.cc",row.country,sizeof(row.country));
    text_field(item,"$.contc",row.region,sizeof(row.region));
    text_field(item,"$.tz",row.location,sizeof(row.location));
    double stamp;
    if(mg_json_get_num(item,"$.updatedAt",&stamp) && isfinite(stamp) && stamp>0 && stamp<32503680000000.0)
      row.updated_at=(int64_t)(stamp/1000.0);
    struct mg_str leaders,leader;
    if(array(item,"$.leaderboard",&leaders)) {
      size_t pos=0; bool ranks[10]={0};
      while((pos=mg_json_next(leaders,pos,NULL,&leader))!=0 && row.leader_count<10) {
        server_leader p={0}; p.rank=integer(leader,"$.place",-1,10); p.score=integer(leader,"$.len",-1,2000000000);
        if(p.rank<1 || p.score<0 || ranks[p.rank-1] || !text_field(leader,"$.nk",p.name,sizeof(p.name))) continue;
        ranks[p.rank-1]=true; row.leaders[row.leader_count++]=p;
      }
      for(int i=0;i<row.leader_count;i++) for(int j=i+1;j<row.leader_count;j++) if(row.leaders[j].rank<row.leaders[i].rank) {
        server_leader p=row.leaders[i]; row.leaders[i]=row.leaders[j]; row.leaders[j]=p;
      }
    }
    parsed->entries[parsed->count++]=row;
  }
  // A nonempty list with no usable servers indicates a changed/broken schema.
  struct mg_str first;
  if(!parsed->count && mg_json_next(list,0,NULL,&first)) { free(parsed); return false; }
  *out=*parsed; free(parsed); return true;
}
static DWORD WINAPI worker(void* unused) {
  (void)unused;
  while(!InterlockedCompareExchange(&stopping,0,0)) {
    WaitForSingleObject(wake,INFINITE);
    if(InterlockedCompareExchange(&stopping,0,0)) break;
    unsigned char* body=NULL; size_t length=0;
    int status=http_request_referer("GET",SERVER_CATALOG_URL,SERVER_CATALOG_SOURCE,NULL,NULL,0,&body,&length,2*1024*1024,4000);
    server_catalog* result=calloc(1,sizeof(*result));
    bool ok=status==200 && result && server_catalog_parse((char*)body,length,result);
    free(body);
    EnterCriticalSection(&lock);
    if(ok) { current=*result; current.received_at=(int64_t)time(NULL); }
    current.pending=false; current.failed=!ok; current.http_status=status;
    LeaveCriticalSection(&lock);
    free(result);
  }
  return 0;
}
void server_catalog_init(void) {
  if(initialized) return;
  memset(&current,0,sizeof(current)); attempted=0; stopping=0;
  InitializeCriticalSection(&lock); initialized=true;
  wake=CreateEventW(NULL,FALSE,FALSE,NULL);
  if(wake) thread=CreateThread(NULL,0,worker,NULL,0,NULL);
}
void server_catalog_destroy(void) {
  if(!initialized) return;
  InterlockedExchange(&stopping,1); if(wake) SetEvent(wake);
  if(thread) { WaitForSingleObject(thread,INFINITE); CloseHandle(thread); }
  if(wake) CloseHandle(wake);
  thread=wake=NULL; initialized=false; DeleteCriticalSection(&lock);
}
void server_catalog_request(void) {
  if(!initialized) return;
  EnterCriticalSection(&lock);
  ULONGLONG now=GetTickCount64();
  if(!thread) current.failed=true;
  else if(!current.pending && (!attempted || now-attempted>=30000)) {
    attempted=now; current.pending=true; SetEvent(wake);
  }
  LeaveCriticalSection(&lock);
}
void server_catalog_copy(server_catalog* out) {
  if(!initialized) { memset(out,0,sizeof(*out)); return; }
  EnterCriticalSection(&lock); *out=current; LeaveCriticalSection(&lock);
}
