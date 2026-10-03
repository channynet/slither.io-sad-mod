#include "../app/src/game/perf_trace.h"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static double column(const char* row,int index) {
  while(index--) { row=strchr(row,','); assert(row); row++; }
  return strtod(row,NULL);
}
int main(void) {
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
  perf_add_duration(PERF_PRESENT,17);
  Sleep(1010); perf_frame(30,40,2,3);
  perf_context(0);
  perf_add_duration(PERF_PRESENT,.5);
  Sleep(1010); perf_frame(5,6,2,3);
  perf_trace_destroy();
  FILE* f=fopen("performance.csv","rb"); assert(f);
  char header[1024],row[1024]; assert(fgets(header,sizeof(header),f)); assert(fgets(row,sizeof(row),f)); fclose(f);
  assert(strstr(header,"max_ms") && strstr(header,"network_ms"));
  assert(strstr(header,"worst_present_ms") && strstr(header,"playing_frames"));
  assert(strstr(row,",100,200,30,40,2,3"));
  assert(strstr(row,",200.000,250.000"));
  assert(column(row,22)==13 && column(row,23)==2 && column(row,24)==1 && column(row,34)==17);
  f=fopen("performance.csv","rb"); assert(f);
  assert(fgets(header,sizeof(header),f)); assert(fgets(row,sizeof(row),f));
  assert(fgets(row,sizeof(row),f)); fclose(f);
  assert(column(row,22)==.5 && column(row,23)==0 && column(row,24)==0 && column(row,34)==.5);
  f=fopen("performance.previous.csv","rb"); assert(f);
  assert(fgets(row,sizeof(row),f)); fclose(f); assert(!strcmp(row,"previous session"));
  puts("Performance trace: asynchronous write, worst-frame attribution/reset, gameplay context and previous-session rotation passed");
}
