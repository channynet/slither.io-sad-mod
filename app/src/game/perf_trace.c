#include "perf_trace.h"
#include "../platform/compat.h"
#include <stdio.h>
#include <string.h>

typedef struct {
  double seconds,mean_ms,max_ms,stage[PERF_COUNT],worst[PERF_COUNT],ping_ms,max_ping_ms;
  unsigned frames,slow,rx,backlog,sprites,foods,mode,images,playing,worst_playing;
  unsigned session,retrans,prompt_ack;
  float tcp_rtt,tcp_min,send_queue;
  double poll_gap;
  float gateway; int signal; unsigned scans,roams,tuned;
  unsigned safe_on,safe_steers; float wall_gap;
  unsigned unfocused; double minimized_ms;
  SYSTEMTIME clock;
} trace_row;
static LARGE_INTEGER frequency;
static double started,period,last_frame,sum,max_frame,stages[PERF_COUNT];
static double frame_stages[PERF_COUNT],worst_stages[PERF_COUNT];
static unsigned playing,playing_frames,worst_playing;
static unsigned frames,slow,rx,backlog,sprites_max,foods_max;
static double last_ping,max_ping;
static unsigned session,retrans,prompt_ack;
static float tcp_rtt=-1,tcp_min=-1,send_queue=-1;
static double poll_gap;
static unsigned safe_on,safe_steers; static float wall_gap=-1;
static unsigned unfocused_frames; static double minimized_total;
static float link_gateway=-1; static int link_signal=-1; static unsigned link_scans,link_roams,link_tuned;
static CRITICAL_SECTION lock;
static HANDLE stop,thread;
static trace_row rows[64];
static unsigned head,tail;
double perf_now(void) { LARGE_INTEGER q; QueryPerformanceCounter(&q); return (double)q.QuadPart*1000.0/frequency.QuadPart; }
// Restarting the game used to overwrite the only older trace, losing the record
// of whatever made the player restart. Older traces are kept by end time instead.
#define TRACE_HISTORY_KEEP 40
static void archive_previous(void) {
  WIN32_FILE_ATTRIBUTE_DATA info; SYSTEMTIME utc,local;
  if(!GetFileAttributesExA("performance.previous.csv",GetFileExInfoStandard,&info)) return;
  if(!FileTimeToSystemTime(&info.ftLastWriteTime,&utc) || !SystemTimeToTzSpecificLocalTime(NULL,&utc,&local)) return;
  CreateDirectoryA("trace-history",NULL);
  char name[96];
  snprintf(name,sizeof(name),"trace-history\\performance-%04d%02d%02d-%02d%02d%02d.csv",
           local.wYear,local.wMonth,local.wDay,local.wHour,local.wMinute,local.wSecond);
  if(!MoveFileExA("performance.previous.csv",name,MOVEFILE_REPLACE_EXISTING)) return;
  // Names sort by time; drop the oldest beyond the limit.
  static char names[256][40]; int count=0;
  WIN32_FIND_DATAA found; HANDLE search=FindFirstFileA("trace-history\\performance-*.csv",&found);
  if(search==INVALID_HANDLE_VALUE) return;
  do {
    if(count<256 && strlen(found.cFileName)<sizeof(names[0])) strcpy(names[count++],found.cFileName);
  } while(FindNextFileA(search,&found));
  FindClose(search);
  for(int extra=count-TRACE_HISTORY_KEEP;extra>0;extra--) {
    int oldest=-1;
    for(int i=0;i<count;i++) if(names[i][0] && (oldest<0 || strcmp(names[i],names[oldest])<0)) oldest=i;
    if(oldest<0) break;
    snprintf(name,sizeof(name),"trace-history\\%s",names[oldest]);
    DeleteFileA(name); names[oldest][0]=0;
  }
}
static FILE* open_trace(void) {
  archive_previous();
  MoveFileExA("performance.csv","performance.previous.csv",MOVEFILE_REPLACE_EXISTING);
  FILE* f=fopen("performance.csv","wb");
  if(f) fputs("seconds,frames,mean_ms,max_ms,over33,wait_ms,network_ms,update_ms,geometry_ms,submit_ms,pacing_ms,rx_bytes,max_backlog_bytes,max_sprites,max_foods,present_mode,images,last_ping_ms,max_ping_ms,bot_ms,commands_ms,queue_submit_ms,present_ms,playing_frames,worst_playing,worst_wait_ms,worst_network_ms,worst_update_ms,worst_geometry_ms,worst_submit_ms,worst_pacing_ms,worst_bot_ms,worst_commands_ms,worst_queue_submit_ms,worst_present_ms,connection_id,tcp_rtt_ms,tcp_min_rtt_ms,tcp_retrans_bytes,ping_send_queue_ms,max_poll_gap_ms,prompt_ack,gateway_ms,wifi_signal,wifi_scans,wifi_roams,wifi_tuned,draw_game_ms,draw_minimap_ms,draw_ui_ms,events_ms,oversleep_ms,worst_events_ms,worst_oversleep_ms,safe_wall,safe_wall_steers,wall_gap_min,gpu_game_ms,worst_gpu_game_ms,unfocused_frames,minimized_ms,gpu_bg_ms,gpu_food_ms,gpu_glow_ms,gpu_body_ms,gpu_border_ms,gpu_minimap_ms,gpu_ui_ms,clock\n",f);
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
        for(int j=0;j<=PERF_PRESENT;j++) fprintf(f,",%.3f",r->worst[j]);
        fprintf(f,",%u,%.3f,%.3f,%u,%.3f,%.3f,%u",r->session,r->tcp_rtt,r->tcp_min,r->retrans,r->send_queue,r->poll_gap,r->prompt_ack);
        fprintf(f,",%.2f,%d,%u,%u,%u",r->gateway,r->signal,r->scans,r->roams,r->tuned);
        // Parts of commands_ms: game draw (includes the minimap upload) and UI / text draw.
        fprintf(f,",%.3f,%.3f,%.3f",r->stage[PERF_DRAW_GAME],r->stage[PERF_DRAW_MINIMAP],r->stage[PERF_DRAW_UI]);
        // Window messages / input between and inside frames, and how much later than asked
        // Windows woke the frame cap's sleeps; both also for the slowest frame of the second.
        fprintf(f,",%.3f,%.3f,%.3f,%.3f",r->stage[PERF_EVENTS],r->stage[PERF_OVERSLEEP],r->worst[PERF_EVENTS],r->worst[PERF_OVERSLEEP]);
        // Safewall on (1/0), input updates in which it steered along the wall, nearest the head came to the wall (-1: unknown).
        fprintf(f,",%u,%u,%.1f",r->safe_on,r->safe_steers,r->wall_gap);
        // GPU time of the game scene per frame (timestamps; 0 where the GPU cannot time), and in the slowest frame.
        fprintf(f,",%.3f,%.3f",r->stage[PERF_GPU_GAME],r->worst[PERF_GPU_GAME]);
        // Frames drawn while another window had focus, and time the game window spent minimized:
        // a long frame with either of these is the window being away, not the game freezing.
        fprintf(f,",%u,%.0f",r->unfocused,r->minimized_ms);
        // GPU time of each pass (background, food, glows incl. the fire skin, bodies and names'
        // accessories, wall, minimap) and of the UI pass (the game image scaled up, text, windows).
        fprintf(f,",%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f",r->stage[PERF_GPU_BG],r->stage[PERF_GPU_FOOD],r->stage[PERF_GPU_GLOW],
                r->stage[PERF_GPU_BODY],r->stage[PERF_GPU_BORDER],r->stage[PERF_GPU_MINIMAP],r->stage[PERF_GPU_UI]);
        // Local wall-clock time, to match a row with what the player saw.
        fprintf(f,",%02d:%02d:%02d",r->clock.wHour,r->clock.wMinute,r->clock.wSecond);
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
static double mark;
void perf_mark(void) { mark=perf_now(); }
void perf_since_mark(int stage) { if(mark) perf_add(stage,mark); mark=0; }
void perf_add(int stage,double start) { perf_add_duration(stage,perf_now()-start); }
void perf_context(int value) { playing=value!=0; }
void perf_received(size_t bytes) { rx+=(unsigned)bytes; }
void perf_backlog(unsigned long bytes) { if(bytes>backlog) backlog=bytes; }
void perf_ping(double milliseconds) { last_ping=milliseconds; if(milliseconds>max_ping) max_ping=milliseconds; }
void perf_connection(unsigned id,float rtt,float minimum,unsigned lost,float queue,double gap,bool ack) {
  session=id; tcp_rtt=rtt; tcp_min=minimum; retrans=lost; send_queue=queue;
  if(gap>poll_gap) poll_gap=gap; prompt_ack=ack;
}
void perf_link(float gateway_ms,int signal,unsigned scans,unsigned roams,unsigned tuned) {
  if(gateway_ms>link_gateway) link_gateway=gateway_ms;
  link_signal=signal; link_scans=scans; link_roams=roams; link_tuned=tuned;
}
void perf_safe_wall(bool on,bool steering,float gap) {
  safe_on=on; safe_steers+=steering;
  if(gap>=0 && (wall_gap<0 || gap<wall_gap)) wall_gap=gap;
}
void perf_window(bool focused) { if(!focused) unfocused_frames++; }
void perf_minimized(double ms) { if(ms>0) minimized_total+=ms; }
void perf_frame(int sprites,int foods,int mode,int images) {
  double now=perf_now();
  if(last_frame) { double dt=now-last_frame; sum+=dt; if(dt>max_frame) { max_frame=dt; memcpy(worst_stages,frame_stages,sizeof(worst_stages)); worst_playing=playing; } if(dt>33.34) slow++; frames++; playing_frames+=playing; }
  memset(frame_stages,0,sizeof(frame_stages));
  last_frame=now;
  if((unsigned)sprites>sprites_max) sprites_max=sprites;
  if((unsigned)foods>foods_max) foods_max=foods;
  if(now-period<1000 || !frames) return;
  trace_row row={.seconds=(now-started)/1000,.frames=frames,.mean_ms=sum/frames,.max_ms=max_frame,.slow=slow,.rx=rx,.backlog=backlog,.sprites=sprites_max,.foods=foods_max,.mode=mode,.images=images,.ping_ms=last_ping,.max_ping_ms=max_ping,.playing=playing_frames,.worst_playing=worst_playing};
  row.session=session; row.tcp_rtt=tcp_rtt; row.tcp_min=tcp_min; row.retrans=retrans;
  row.send_queue=send_queue; row.poll_gap=poll_gap; row.prompt_ack=prompt_ack; poll_gap=0;
  row.gateway=link_gateway; row.signal=link_signal; row.scans=link_scans; row.roams=link_roams; row.tuned=link_tuned; link_gateway=-1;
  row.safe_on=safe_on; row.safe_steers=safe_steers; row.wall_gap=wall_gap; safe_steers=0; wall_gap=-1;
  row.unfocused=unfocused_frames; row.minimized_ms=minimized_total; unfocused_frames=0; minimized_total=0;
  GetLocalTime(&row.clock);
  for(int i=0;i<PERF_COUNT;i++) { row.stage[i]=stages[i]/frames; row.worst[i]=worst_stages[i]; }
  if(thread && TryEnterCriticalSection(&lock)) {
    unsigned next=(head+1)%64;
    if(next!=tail) { rows[head]=row; head=next; }
    LeaveCriticalSection(&lock);
  }
  period=now; playing_frames=worst_playing=0; memset(worst_stages,0,sizeof(worst_stages)); frames=slow=rx=backlog=sprites_max=foods_max=0; sum=max_frame=max_ping=0; memset(stages,0,sizeof(stages));
}
