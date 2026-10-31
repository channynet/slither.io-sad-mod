#ifndef PERF_TRACE_H
#define PERF_TRACE_H
#include <stddef.h>
#include <stdbool.h>
enum { PERF_WAIT,PERF_NETWORK,PERF_UPDATE,PERF_GEOMETRY,PERF_SUBMIT,PERF_PACING,PERF_BOT,PERF_COMMANDS,PERF_QUEUE_SUBMIT,PERF_PRESENT,PERF_DRAW_GAME,PERF_DRAW_MINIMAP,PERF_DRAW_UI,PERF_EVENTS,PERF_OVERSLEEP,PERF_GPU_GAME,PERF_GPU_BG,PERF_GPU_FOOD,PERF_GPU_GLOW,PERF_GPU_BODY,PERF_GPU_BORDER,PERF_GPU_MINIMAP,PERF_GPU_UI,PERF_COUNT };
void perf_trace_init(void);
void perf_trace_destroy(void);
double perf_now(void);
// Time spent between frames outside drawing and the frame cap (window messages, input):
// perf_mark() where a frame's work ends, perf_since_mark() where the next one starts.
void perf_mark(void);
void perf_since_mark(int stage);
void perf_add(int stage,double start);
void perf_add_duration(int stage,double milliseconds);
void perf_context(int playing);
void perf_received(size_t bytes);
void perf_backlog(unsigned long bytes);
void perf_ping(double milliseconds);
void perf_connection(unsigned session,float tcp_rtt,float tcp_min,unsigned retrans,float send_queue,double poll_gap,bool prompt_ack);
// Latest local-link sample (see network/link_probe.h); the row keeps the slowest gateway reply.
void perf_link(float gateway_ms,int signal,unsigned scans,unsigned roams,unsigned tuned);
void perf_frame(int sprites,int foods,int present_mode,int images);
// Safewall: whether it is on, whether it is steering along the wall now, and how far the head is from the wall.
void perf_safe_wall(bool on,bool steering,float wall_gap);
// Window state each frame (focused or not) and time spent minimized, to tell a freeze from the
// game window simply being away.
void perf_window(bool focused);
void perf_minimized(double milliseconds);
#endif
