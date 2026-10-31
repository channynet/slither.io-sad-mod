#ifndef NETWORK_PUMP_H
#define NETWORK_PUMP_H
#include <stdint.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/ioctl.h>
#endif
#include "../external/mongoose.h"
#define GAME_RECEIVE_BYTES (64u*1024u)
#define GAME_POLL_LIMIT 8
#define GAME_POLL_BUDGET_MS 2.0
static inline unsigned long game_pending_bytes(struct mg_connection* c) {
  unsigned long bytes=0;
  if(c && !c->is_resolving && !c->is_connecting && !c->is_closing) {
#ifdef _WIN32
    ioctlsocket((SOCKET)(uintptr_t)c->fd,FIONREAD,&bytes);
#else
    int n=0; if(ioctl((int)(intptr_t)c->fd,FIONREAD,&n)==0 && n>0) bytes=(unsigned long)n;
#endif
  }
  return bytes;
}
static inline bool game_prepare_receive(struct mg_connection* c) {
  c->recv.align=16u*1024u;
  return mg_iobuf_resize(&c->recv,GAME_RECEIVE_BYTES);
}
static inline int game_poll_network(struct mg_mgr* mgr,struct mg_connection** live,
    bool* closed,double (*clock_ms)(void),void (*record_backlog)(unsigned long)) {
  double start=clock_ms(); int polls=0;
  for(int i=0;i<GAME_POLL_LIMIT;i++) {
    if(record_backlog) record_backlog(game_pending_bytes(*live));
    mg_mgr_poll(mgr,0); polls++;
    struct mg_connection* c=*live;
    if(!c || *closed || c->is_closing || c->is_resolving || c->is_connecting) break;
    unsigned long pending=game_pending_bytes(c);
    if(record_backlog) record_backlog(pending);
    if((!pending && !c->send.len) || clock_ms()-start>=GAME_POLL_BUDGET_MS) break;
  }
  return polls;
}
#endif
