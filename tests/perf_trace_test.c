#include "../app/src/game/perf_trace.h"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
static double column(const char* row,int index) {
  while(index--) { row=strchr(row,','); assert(row); row++; }
  return strtod(row,NULL);
}
int main(void) {
  // An older trace that a restart would have overwritten is archived by its end time.
  FILE* older=fopen("performance.previous.csv","wb"); assert(older); fputs("older session",older); fclose(older);
  FILE* previous=fopen("performance.csv","wb"); assert(previous);
  fputs("previous session",previous); fclose(previous);
  perf_trace_init();
  perf_frame(10,20,2,3);
  perf_context(1);
  perf_add_duration(PERF_PRESENT,9);
  Sleep(40); perf_frame(25,35,2,3);
  double start=perf_now(); Sleep(5); perf_add(PERF_NETWORK,start);
  perf_received(100); perf_backlog(200);
  perf_ping(250); perf_ping(200);
  perf_connection(7,40,38,1024,2.5,8,true);
  perf_link(2.5f,70,4,1,3); perf_link(1.25f,71,5,1,3);
  perf_add_duration(PERF_EVENTS,6); perf_add_duration(PERF_OVERSLEEP,1);
  perf_add_duration(PERF_GPU_GAME,3);
  perf_safe_wall(true,true,40); perf_safe_wall(true,false,25.5f); perf_safe_wall(true,true,-3);
  perf_window(false); perf_window(true); perf_minimized(120); perf_minimized(30);
  perf_add_duration(PERF_DRAW_GAME,4); perf_add_duration(PERF_DRAW_MINIMAP,1); perf_add_duration(PERF_DRAW_UI,2);
  perf_add_duration(PERF_PRESENT,17);
  Sleep(1010); perf_frame(30,40,2,3);
  perf_context(0);
  perf_add_duration(PERF_PRESENT,.5);
  Sleep(1010); perf_frame(5,6,2,3);
  perf_trace_destroy();
  FILE* f=fopen("performance.csv","rb"); assert(f);
  char header[2048],row[2048]; assert(fgets(header,sizeof(header),f)); assert(fgets(row,sizeof(row),f)); fclose(f);
  assert(strstr(header,"max_ms") && strstr(header,"network_ms"));
  assert(strstr(header,"worst_present_ms") && strstr(header,"playing_frames"));
  assert(strstr(row,",100,200,30,40,2,3"));
  assert(strstr(row,",200.000,250.000"));
  assert(column(row,22)==13 && column(row,23)==2 && column(row,24)==1 && column(row,34)==17);
  assert(column(row,35)==7 && column(row,36)==40 && column(row,37)==38 && column(row,38)==1024);
  assert(column(row,39)==2.5 && column(row,40)==8 && column(row,41)==1);
  assert(strstr(header,"prompt_ack,gateway_ms,wifi_signal,wifi_scans,wifi_roams,wifi_tuned"));
  // The slowest gateway reply of the second is kept; the other fields are the latest.
  assert(column(row,42)==2.5 && column(row,43)==71 && column(row,44)==5 && column(row,45)==1 && column(row,46)==3);
  // Two frames in this row: per-frame averages of the added durations.
  assert(strstr(header,"wifi_tuned,draw_game_ms,draw_minimap_ms,draw_ui_ms"));
  assert(column(row,47)==2 && column(row,48)==.5 && column(row,49)==1);
  assert(strstr(header,"draw_ui_ms,events_ms,oversleep_ms,worst_events_ms,worst_oversleep_ms,safe_wall,safe_wall_steers,wall_gap_min,gpu_game_ms,worst_gpu_game_ms,unfocused_frames,minimized_ms,gpu_bg_ms,gpu_food_ms,gpu_glow_ms,gpu_body_ms,gpu_border_ms,gpu_minimap_ms,gpu_ui_ms,clock"));
  // 6 ms of window messages over the row's two frames, all of it in the slower frame.
  assert(column(row,50)==3 && column(row,51)==.5 && column(row,52)==6 && column(row,53)==1);
  // Safewall: on, steered in 2 updates, nearest 25.5 from the wall (negative gaps are not distances).
  assert(column(row,54)==1 && column(row,55)==2 && column(row,56)==25.5);
  assert(column(row,57)==1.5 && column(row,58)==3);   // GPU scene time: 3 ms over two frames, all in the slower one
  assert(column(row,59)==1 && column(row,60)==150);   // one frame without focus, 150 ms minimized
  assert(strlen(strrchr(row,','))>=9 && strrchr(row,',')[3]==':');
  f=fopen("performance.csv","rb"); assert(f);
  assert(fgets(header,sizeof(header),f)); assert(fgets(row,sizeof(row),f));
  assert(fgets(row,sizeof(row),f)); fclose(f);
  assert(column(row,22)==.5 && column(row,23)==0 && column(row,24)==0 && column(row,34)==.5);
  assert(column(row,42)==-1 && column(row,43)==71);  // no new gateway sample in this second
  assert(column(row,54)==1 && column(row,55)==0 && column(row,56)==-1);  // still on, not steering, no sample
  assert(column(row,59)==0 && column(row,60)==0);
  f=fopen("performance.previous.csv","rb"); assert(f);
  assert(fgets(row,sizeof(row),f)); fclose(f); assert(!strcmp(row,"previous session"));
  WIN32_FIND_DATAA found; HANDLE search=FindFirstFileA("trace-history\\performance-*.csv",&found); assert(search!=INVALID_HANDLE_VALUE);
  char archived[300]; snprintf(archived,sizeof(archived),"trace-history\\%s",found.cFileName);
  assert(!FindNextFileA(search,&found)); FindClose(search);
  f=fopen(archived,"rb"); assert(f); assert(fgets(row,sizeof(row),f)); fclose(f); assert(!strcmp(row,"older session"));
  // Only the newest 40 archived traces are kept.
  for(int i=0;i<45;i++) { char path[80]; snprintf(path,sizeof(path),"trace-history\\performance-20200101-0000%02d.csv",i); f=fopen(path,"wb"); assert(f); fclose(f); }
  perf_trace_init(); perf_trace_destroy();
  int kept=0; bool oldest_left=false; search=FindFirstFileA("trace-history\\performance-*.csv",&found); assert(search!=INVALID_HANDLE_VALUE);
  do { kept++; if(!strcmp(found.cFileName,"performance-20200101-000000.csv")) oldest_left=true; } while(FindNextFileA(search,&found));
  FindClose(search); assert(kept==40 && !oldest_left);
  puts("Performance trace: asynchronous write, worst-frame attribution/reset, gameplay context, previous-session rotation and trace history passed");
}
