#include "perf_trace.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  double seconds,mean_ms,max_ms,stage[PERF_COUNT],worst[PERF_COUNT],ping_ms,max_ping_ms;
  unsigned frames,slow,rx,backlog,sprites,foods,mode,images,playing,worst_playing;
} trace_row;
static LARGE_INTEGER frequency;
static double started,period,last_frame,sum,max_frame,stages[PERF_COUNT];
static double frame_stages[PERF_COUNT],worst_stages[PERF_COUNT];
static unsigned playing,playing_frames,worst_playing;
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
  if(f) fputs("seconds,frames,mean_ms,max_ms,over33,wait_ms,network_ms,update_ms,geometry_ms,submit_ms,pacing_ms,rx_bytes,max_backlog_bytes,max_sprites,max_foods,present_mode,images,last_ping_ms,max_ping_ms,bot_ms,commands_ms,queue_submit_ms,present_ms,playing_frames,worst_playing,worst_wait_ms,worst_network_ms,worst_update_ms,worst_geometry_ms,worst_submit_ms,worst_pacing_ms,worst_bot_ms,worst_commands_ms,worst_queue_submit_ms,worst_present_ms\n",f);
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
      if(f) { fprintf(f,"%.1f,%u,%.3f,%.3f,%u,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%u,%u,%u,%u,%u,%u,%.3f,%.3f,%.3f",
        r->seconds,r->frames,r->mean_ms,r->max_ms,r->slow,r->stage[0],r->stage[1],r->stage[2],r->stage[3],r->stage[4],r->stage[5],r->rx,r->backlog,r->sprites,r->foods,r->mode,r->images,r->ping_ms,r->max_ping_ms,r->stage[PERF_BOT]);
        fprintf(f,",%.3f,%.3f,%.3f,%u,%u",r->stage[PERF_COMMANDS],r->stage[PERF_QUEUE_SUBMIT],r->stage[PERF_PRESENT],r->playing,r->worst_playing);
        for(int j=0;j<PERF_COUNT;j++) fprintf(f,",%.3f",r->worst[j]);
        fputc('\n',f);
      }
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
// Stages can nest: submit contains commands/queue/present; pacing includes network.
void perf_add_duration(int stage,double milliseconds) {
  if(stage<0 || stage>=PERF_COUNT || milliseconds<0) return;
  stages[stage]+=milliseconds; frame_stages[stage]+=milliseconds;
}
void perf_add(int stage,double start) { perf_add_duration(stage,perf_now()-start); }
void perf_context(int value) { playing=value!=0; }
void perf_received(size_t bytes) { rx+=(unsigned)bytes; }
void perf_backlog(unsigned long bytes) { if(bytes>backlog) backlog=bytes; }
void perf_ping(double milliseconds) { last_ping=milliseconds; if(milliseconds>max_ping) max_ping=milliseconds; }
void perf_frame(int sprites,int foods,int mode,int images) {
  double now=perf_now();
  if(last_frame) { double dt=now-last_frame; sum+=dt; if(dt>max_frame) { max_frame=dt; memcpy(worst_stages,frame_stages,sizeof(worst_stages)); worst_playing=playing; } if(dt>33.34) slow++; frames++; playing_frames+=playing; }
  memset(frame_stages,0,sizeof(frame_stages));
  last_frame=now;
  if((unsigned)sprites>sprites_max) sprites_max=sprites;
  if((unsigned)foods>foods_max) foods_max=foods;
  if(now-period<1000 || !frames) return;
  trace_row row={.seconds=(now-started)/1000,.frames=frames,.mean_ms=sum/frames,.max_ms=max_frame,.slow=slow,.rx=rx,.backlog=backlog,.sprites=sprites_max,.foods=foods_max,.mode=mode,.images=images,.ping_ms=last_ping,.max_ping_ms=max_ping,.playing=playing_frames,.worst_playing=worst_playing};
  for(int i=0;i<PERF_COUNT;i++) { row.stage[i]=stages[i]/frames; row.worst[i]=worst_stages[i]; }
  if(thread && TryEnterCriticalSection(&lock)) {
    unsigned next=(head+1)%64;
    if(next!=tail) { rows[head]=row; head=next; }
    LeaveCriticalSection(&lock);
  }
  period=now; playing_frames=worst_playing=0; memset(worst_stages,0,sizeof(worst_stages)); frames=slow=rx=backlog=sprites_max=foods_max=0; sum=max_frame=max_ping=0; memset(stages,0,sizeof(stages));
}
