#ifndef PATH_CHOICE_H
#define PATH_CHOICE_H
#include <stdbool.h>
#include <string.h>
// The same server is reached over one of several network paths, fixed when the
// connection is made (measured: ~38 ms or ~65 ms to the same address). Before
// the snake is created, a connection that is clearly slower than the best one
// seen for that address is dropped and made again.
#define PATH_SLOTS 8
#define PATH_MARGIN_MS 12.0f
#define PATH_MAX_ATTEMPTS 3
// Servers refuse clients that open many connections quickly; stay well below that.
#define PATH_WINDOW_MS 20000.0
#define PATH_WINDOW_LIMIT 5
typedef struct { char address[24]; float best; } path_slot;
typedef struct {
  path_slot slots[PATH_SLOTS];
  int count,next,attempt,recent_next;
  double recent[PATH_WINDOW_LIMIT];
} path_choice;
// A new Play / respawn: the attempt budget starts over.
static inline void path_choice_begin(path_choice* p) { p->attempt=0; }
// Every connection that is opened, retries included.
static inline void path_choice_opened(path_choice* p,double now_ms) {
  p->recent[p->recent_next]=now_ms; p->recent_next=(p->recent_next+1)%PATH_WINDOW_LIMIT;
}
// True when this connection should be dropped and opened again.
static inline bool path_choice_retry(path_choice* p,const char* address,float rtt_ms,double now_ms) {
  p->attempt++;
  if(!address || !(rtt_ms>0)) return false;
  path_slot* slot=NULL;
  for(int i=0;i<p->count;i++) if(!strncmp(p->slots[i].address,address,sizeof(p->slots[i].address)-1)) slot=p->slots+i;
  if(!slot) {
    slot=p->slots+(p->count<PATH_SLOTS?p->count++:(p->next++)%PATH_SLOTS);
    memset(slot,0,sizeof(*slot)); strncpy(slot->address,address,sizeof(slot->address)-1);
  }
  float before=slot->best;
  if(before<=0 || rtt_ms<before) slot->best=rtt_ms;
  if(p->attempt>=PATH_MAX_ATTEMPTS) return false;
  int busy=0;
  for(int i=0;i<PATH_WINDOW_LIMIT;i++) if(p->recent[i]>0 && now_ms-p->recent[i]<PATH_WINDOW_MS) busy++;
  if(busy>=PATH_WINDOW_LIMIT) return false;
  // Nothing to compare with yet: take one more sample to learn this address.
  if(before<=0) return true;
  return rtt_ms>before+PATH_MARGIN_MS;
}
#endif
