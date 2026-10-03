#include "../app/src/network/server_status.h"
#include "../app/src/external/mongoose.h"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
static int queries,joins;
static void echo(struct mg_connection* c,int ev,void* data) {
  if(ev==MG_EV_HTTP_MSG) {
    struct mg_http_message* h=data;
    assert(mg_match(h->uri,mg_str("/ptc"),NULL)); mg_ws_upgrade(c,h,NULL);
  } else if(ev==MG_EV_WS_MSG) {
    struct mg_ws_message* m=data;
    if(m->data.len!=1 || m->data.buf[0]!='p') joins++;
    queries++; mg_ws_send(c,m->data.buf,m->data.len,WEBSOCKET_OP_BINARY);
  }
}
int main(void) {
  mg_log_set(MG_LL_NONE);
  struct mg_mgr mgr; mg_mgr_init(&mgr);
  struct mg_connection* listener=mg_http_listen(&mgr,"http://127.0.0.1:0",echo,NULL); assert(listener);
  char addr[32]; snprintf(addr,sizeof(addr),"127.0.0.1:%u",mg_ntohs(listener->loc.port));
  server_status_init(); server_status s;
  server_status_population(addr,321); server_status_game_ping(addr,25.5f);
  assert(server_status_get(addr,&s) && s.players==321 && s.ping_ms==26 && s.game_ping);
  server_status_game_ping(addr,NAN); server_status_population(addr,-1);
  assert(server_status_get(addr,&s) && s.players==321 && s.ping_ms==26);
  server_status_request(addr); Sleep(30); assert(queries==0); // no probes during gameplay
  server_status_menu(true); server_status_request(addr);
  ULONGLONG start=GetTickCount64();
  do { mg_mgr_poll(&mgr,5); server_status_get(addr,&s); } while(s.pending && GetTickCount64()-start<3000);
  assert(!s.pending && !s.failed && !s.game_ping && s.ping_ms>=0 && queries==3 && joins==0);
  for(int i=0;i<200;i++) server_status_request(addr);
  for(int i=0;i<10;i++) mg_mgr_poll(&mgr,5);
  assert(queries==3); // refresh throttle
  assert(s.players==321 && s.players_at>0);
  server_status_population("127.0.0.1:2",44);
  assert(server_status_get("127.0.0.1:2",&s) && s.players==44);
  assert(server_status_get(addr,&s) && s.players==321); // ports are not merged
  server_status_request("not-a-server"); assert(!server_status_get("not-a-server",&s) && s.players==-1);
  server_status_menu(false); server_status_destroy(); mg_mgr_free(&mgr);
  server_status_init(); assert(server_status_get(addr,&s) && s.players==321 && s.ping_ms==-1);
  server_status_destroy();
  puts("Server status: no-spawn three-echo median, menu-only throttle, population persistence, exact port identity, unknown/invalid values passed");
}
