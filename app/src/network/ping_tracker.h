#ifndef PING_TRACKER_H
#define PING_TRACKER_H
#include <stdbool.h>
#include <stddef.h>
#include <math.h>
typedef struct {
  double requested,sent;
  size_t remaining;
  float samples[8],sum,last,queue_ms;
  unsigned count,next;
  bool pending,flushed;
} ping_tracker;
static inline void ping_begin(ping_tracker* p,double now,size_t queued_through_ping) {
  p->requested=now; p->remaining=queued_through_ping; p->pending=true; p->flushed=false;
}
static inline void ping_written(ping_tracker* p,double now,size_t bytes) {
  if(!p->pending || p->flushed) return;
  if(bytes<p->remaining) p->remaining-=bytes;
  else { p->remaining=0; p->sent=now; p->flushed=true; }
}
static inline bool ping_received(ping_tracker* p,double now,int* display) {
  if(!p->pending || now<p->requested || now-p->requested>60000) return false;
  // Display the full experienced delay, including our send queue; never hide it.
  p->last=(float)(now-p->requested);
  p->queue_ms=p->flushed?(float)fmax(0,p->sent-p->requested):-1;
  p->sum-=p->samples[p->next]; p->samples[p->next]=p->last; p->sum+=p->last;
  p->next=(p->next+1)%8; if(p->count<8) p->count++;
  *display=(int)lroundf(p->sum/p->count); p->pending=false; return true;
}
#endif
