#include "perf_trace.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  double seconds,mean_ms,max_ms,stage[PERF_COUNT],ping_ms,max_ping_ms;
  unsigned frames,slow,rx,backlog,sprites,foods,mode,images;
} trace_row;
static LARGE_INTEGER frequency;
static double started,period,last_frame,sum,max_frame,stages[PERF_COUNT];
static unsigned frames,slow,rx,backlog,sprites_max,foods_max;
static double last_ping,max_ping;
static CRITICAL_SECTION lock;
static HANDLE stop,thread;
static trace_row rows[64];
static unsigned head,tail;
double perf_now(void) { LARGE_INTEGER q; QueryPerformanceCounter(&q); return (double)q.QuadPart*1000.0/frequency.QuadPart; }
static FILE* open_trace(void) {
  MoveFileExA("performance.csv","performance.previous.csv",MOVEFILE_REPLACE_EXISTING);
  FILE* f=fopen("performance.csv","wb");
  if(f) fputs("seconds,frames,mean_ms,max_ms,over33,wait_ms,network_ms,update_ms,geometry_ms,submit_ms,pacing_ms,rx_bytes,max_backlog_bytes,max_sprites,max_foods,present_mode,images,last_ping_ms,max_ping_ms,bot_ms\n",f);
  return f;
}
static DWORD WINAPI writer(void* unused) {
  FILE* f=open_trace(); unsigned written=0;
  for(;;) {
    DWORD result=WaitForSingleObject(stop,1000);
    trace_row batch[64]; unsigned count=0;
    EnterCriticalSection(&lock);
    while(tail!=head) { batch[count++]=rows[tail]; tail=(tail+1)%64; }
    LeaveCriticalSection(&lock);
    for(unsigned i=0;i<count;i++) {
      if(written>=3600) { if(f) fclose(f); f=open_trace(); written=0; }
      trace_row* r=batch+i;
      if(f) fprintf(f,"%.1f,%u,%.3f,%.3f,%u,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%u,%u,%u,%u,%u,%u,%.3f,%.3f,%.3f\n",
        r->seconds,r->frames,r->mean_ms,r->max_ms,r->slow,r->stage[0],r->stage[1],r->stage[2],r->stage[3],r->stage[4],r->stage[5],r->rx,r->backlog,r->sprites,r->foods,r->mode,r->images,r->ping_ms,r->max_ping_ms,r->stage[PERF_BOT]);
      written++;
    }
    if(f) fflush(f);
    if(result==WAIT_OBJECT_0) break;
  }
  if(f) fclose(f);
  return 0;
}
void perf_trace_init(void) {
  QueryPerformanceFrequency(&frequency); started=period=perf_now();
  InitializeCriticalSection(&lock); stop=CreateEventW(NULL,TRUE,FALSE,NULL);
  if(stop) thread=CreateThread(NULL,0,writer,NULL,0,NULL);
}
void perf_trace_destroy(void) {
  if(stop) SetEvent(stop);
  if(thread) { WaitForSingleObject(thread,INFINITE); CloseHandle(thread); }
  if(stop) CloseHandle(stop);
  DeleteCriticalSection(&lock);
}
void perf_add(int stage,double start) { stages[stage]+=perf_now()-start; }
void perf_received(size_t bytes) { rx+=(unsigned)bytes; }
void perf_backlog(unsigned long bytes) { if(bytes>backlog) backlog=bytes; }
void perf_ping(double milliseconds) { last_ping=milliseconds; if(milliseconds>max_ping) max_ping=milliseconds; }
void perf_frame(int sprites,int foods,int mode,int images) {
  double now=perf_now();
  if(last_frame) { double dt=now-last_frame; sum+=dt; if(dt>max_frame) max_frame=dt; if(dt>33.34) slow++; frames++; }
  last_frame=now;
  if((unsigned)sprites>sprites_max) sprites_max=sprites;
  if((unsigned)foods>foods_max) foods_max=foods;
  if(now-period<1000 || !frames) return;
  trace_row row={.seconds=(now-started)/1000,.frames=frames,.mean_ms=sum/frames,.max_ms=max_frame,.slow=slow,.rx=rx,.backlog=backlog,.sprites=sprites_max,.foods=foods_max,.mode=mode,.images=images,.ping_ms=last_ping,.max_ping_ms=max_ping};
  for(int i=0;i<PERF_COUNT;i++) row.stage[i]=stages[i]/frames;
  if(thread && TryEnterCriticalSection(&lock)) {
    unsigned next=(head+1)%64;
    if(next!=tail) { rows[head]=row; head=next; }
    LeaveCriticalSection(&lock);
  }
  period=now; frames=slow=rx=backlog=sprites_max=foods_max=0; sum=max_frame=max_ping=0; memset(stages,0,sizeof(stages));
}
