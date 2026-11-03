#include "../app/src/network/network_pump.h"
#include "../app/src/network/socket_latency.h"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { struct mg_connection* live; bool ready,closed,improved; int received; } peer;
static double clock_ms(void) { return (double)GetTickCount64(); }
static void server_event(struct mg_connection* c,int ev,void* data) {
  peer* p=c->fn_data;
  if(ev==MG_EV_HTTP_MSG) mg_ws_upgrade(c,data,NULL);
  if(ev==MG_EV_WS_OPEN) p->live=c;
  if(ev==MG_EV_CLOSE && p->live==c) p->live=NULL;
}
static void client_event(struct mg_connection* c,int ev,void* data) {
  peer* p=c->fn_data;
  if(ev==MG_EV_OPEN && p->improved) assert(game_prepare_receive(c));
  if(ev==MG_EV_CONNECT && p->improved) socket_prompt_ack((SOCKET)(uintptr_t)c->fd);
  if(ev==MG_EV_WS_OPEN) p->ready=true;
  if(ev==MG_EV_WS_MSG) {
    struct mg_ws_message* msg=data; unsigned sequence;
    assert(msg->data.len==96); memcpy(&sequence,msg->data.buf,4);
    assert(sequence==(unsigned)p->received);
    for(int i=4;i<96;i++) assert((unsigned char)msg->data.buf[i]==(unsigned char)sequence);
    p->received++;
  }
  if(ev==MG_EV_CLOSE) { p->live=NULL; p->closed=true; }
}
static int burst(bool improved) {
  struct mg_mgr server,client; mg_mgr_init(&server); mg_mgr_init(&client);
  peer sp={0},cp={.improved=improved};
  struct mg_connection* listener=mg_http_listen(&server,"http://127.0.0.1:0",server_event,&sp);
  assert(listener);
  struct sockaddr_in addr; int addrlen=sizeof(addr);
  assert(!getsockname((SOCKET)(uintptr_t)listener->fd,(struct sockaddr*)&addr,&addrlen));
  char url[128]; snprintf(url,sizeof(url),"ws://127.0.0.1:%u/game",ntohs(addr.sin_port));
  cp.live=mg_ws_connect(&client,url,client_event,&cp,NULL); assert(cp.live);
  for(int i=0;!cp.ready && i<1000;i++) { mg_mgr_poll(&server,0); mg_mgr_poll(&client,0); Sleep(1); }
  assert(cp.ready && sp.live);
  socket_latency transport=socket_latency_read((SOCKET)(uintptr_t)cp.live->fd);
  // TCP_INFO is optional (for example under Wine). Missing telemetry is the
  // documented sentinel, not a failed WebSocket receive path.
  if(transport.rtt_ms<0) {
    assert(transport.rtt_ms==-1 && transport.min_rtt_ms==-1 && transport.retrans==0);
    puts("TCP latency telemetry unavailable: unknown-value fallback verified");
  } else assert(transport.min_rtt_ms>=0);
  if(improved) assert(cp.live->recv.size>=GAME_RECEIVE_BYTES);
  for(unsigned i=0;i<256;i++) { unsigned char msg[96]; memset(msg,i,sizeof(msg)); memcpy(msg,&i,4); mg_ws_send(sp.live,msg,sizeof(msg),WEBSOCKET_OP_BINARY); }
  for(int i=0;sp.live->send.len && i<1000;i++) { mg_mgr_poll(&server,0); Sleep(1); }
  int calls=0;
  while(cp.received<256 && calls<1000) {
    if(improved) assert(game_poll_network(&client,&cp.live,&cp.closed,clock_ms,NULL)<=GAME_POLL_LIMIT);
    else mg_mgr_poll(&client,0);
    calls++;
  }
  assert(cp.received==256);
  // Exercise close handling: the pump must not dereference its freed socket.
  cp.live->is_closing=true;
  game_poll_network(&client,&cp.live,&cp.closed,clock_ms,NULL);
  assert(cp.closed && !cp.live);
  mg_mgr_free(&client); mg_mgr_free(&server);
  return calls;
}
int main(void) {
  mg_log_set(MG_LL_NONE);
  printf("Mongoose default IO growth: %u bytes\n",(unsigned)MG_IO_SIZE);
  int old=burst(false),now=burst(true);
  printf("WebSocket burst: 256 x 96-byte messages, old %d service calls, new %d; order/data/close verified\n",old,now);
  // OS scheduling and packet segmentation vary. The burst tests above assert
  // delivery, ordering, content, bounded polling and close handling. Call counts
  // are diagnostic measurements, not a deterministic correctness condition.
}
