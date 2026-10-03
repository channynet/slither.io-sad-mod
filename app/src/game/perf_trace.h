#ifndef PERF_TRACE_H
#define PERF_TRACE_H
#include <stddef.h>
enum { PERF_WAIT,PERF_NETWORK,PERF_UPDATE,PERF_GEOMETRY,PERF_SUBMIT,PERF_PACING,PERF_BOT,PERF_COMMANDS,PERF_QUEUE_SUBMIT,PERF_PRESENT,PERF_COUNT };
void perf_trace_init(void);
void perf_trace_destroy(void);
double perf_now(void);
void perf_add(int stage,double start);
void perf_add_duration(int stage,double milliseconds);
void perf_context(int playing);
void perf_received(size_t bytes);
void perf_backlog(unsigned long bytes);
void perf_ping(double milliseconds);
void perf_frame(int sprites,int foods,int present_mode,int images);
#endif
