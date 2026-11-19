#include "../app/src/network/server_address.h"
#include "../app/src/external/mongoose.h"
#include <stdio.h>
#include <stdlib.h>

static int echoes;
static bool done, opened;
static char error[200];
static uint64_t next_ping;
static void event(struct mg_connection* c,int ev,void* data) {
  if(ev==MG_EV_WS_OPEN) { opened=true; mg_ws_send(c,"p",1,WEBSOCKET_OP_BINARY); }
  if(ev==MG_EV_WS_MSG) {
    struct mg_ws_message* m=data;
    if(m->data.len!=1 || m->data.buf[0]!='p') { snprintf(error,sizeof(error),"bad echo"); done=true; }
    else { echoes++; next_ping=mg_millis()+250; }
  }
  if(ev==MG_EV_POLL && opened && next_ping && mg_millis()>=next_ping) {
    next_ping=0; mg_ws_send(c,"p",1,WEBSOCKET_OP_BINARY);
  }
  if(ev==MG_EV_ERROR) { snprintf(error,sizeof(error),"%s",(char*)data); done=true; }
  if(ev==MG_EV_CLOSE) done=true;
}
int main(int argc,char** argv) {
  if(argc<2) return 2;
  bool reject=argc>2 && !strcmp(argv[2],"reject");
  uint64_t duration=argc>3?(uint64_t)atoi(argv[3])*1000:1000;
  char clean[MAX_IPV4_LEN+1],url[256],host[MAX_IPV4_LEN+1];
  if(!server_address_clean(argv[1],clean) ||
     !server_address_url(clean,"/ptc",url,sizeof(url),host,sizeof(host),3000)) return 3;
  mg_log_set(MG_LL_ERROR);
  struct mg_mgr mgr; mg_mgr_init(&mgr);
  struct mg_connection* c=mg_ws_connect(&mgr,url,event,NULL,"Origin: https://slither.com\r\n");
  if(!c) { mg_mgr_free(&mgr); return 4; }
  bool configured=server_address_host(c,host);
  uint64_t start=mg_millis(),until=0;
  while(configured && !done && mg_millis()-start<duration+12000) {
    mg_mgr_poll(&mgr,5);
    if(echoes && !until) until=mg_millis()+duration;
    if(until && mg_millis()>=until) break;
  }
  bool certificate_error=strstr(error,"certificate") || strstr(error,"hostname") ||
                         strstr(error,"CA") || strstr(error,"trust store");
  bool pass=reject?(!opened && certificate_error):(opened && echoes>=3 && !done && !error[0]);
  printf("%s url=%s opened=%d echoes=%d error=%s\n",pass?"PASS":"FAIL",url,opened,echoes,error);
  mg_mgr_free(&mgr);
  return pass?0:1;
}
