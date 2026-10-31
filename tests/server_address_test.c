#include "../app/src/network/server_address.h"
#include "../app/src/external/mongoose.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void same(const char* input,const char* expected) {
  char out[MAX_IPV4_LEN+1]="?";
  if(!server_address_clean(input,out)) { printf("rejected: %s\n",input); assert(0); }
  if(strcmp(out,expected)) { printf("%s -> %s, expected %s\n",input,out,expected); assert(0); }
}
static void handler(struct mg_connection* c,int ev,void* data) { (void)c; (void)ev; (void)data; }
int main(void) {
  char out[MAX_IPV4_LEN+1];
  // Numbers: exactly as before.
  same(" 127.000.0.1:00444 \r\n","127.0.0.1:444"); same("10.0.0.2:65535","10.0.0.2:65535");
  // Names: the online address of a private server, typed or pasted in any of these ways.
  same("kingston-inside-best-graphic.trycloudflare.com","kingston-inside-best-graphic.trycloudflare.com");
  same("  https://Kingston-Inside-Best-Graphic.TryCloudflare.com/  ","kingston-inside-best-graphic.trycloudflare.com");
  same("http://name.example.com/slither?x=1","name.example.com");
  same("wss://name.example.com:443","name.example.com:443"); same("ws://name.example.com:80","name.example.com");
  same("name.example.com:8080","name.example.com:8080"); same("name.example.com.","name.example.com");
  same("localhost:4444","localhost:4444"); same("a1.b2","a1.b2");
  const char* bad[]={"",NULL,"1.2.3.256:444","1.2.3.4:0","1.2.3.4:65536","1.2.3.4:444/x","1.2.3:444","ws://1.2.3.4:444",
    "1.2.3.4:444\n5.6.7.8:444","1.2.3.4","name","name..com","-name.com","name.com-","na me.com",
    "name.com:0","name.com:99999","name.com:","name.com:80x","name_x.com","이름.com","name.com extra","https://",
    "a-name-that-is-far-too-long-to-be-a-real-relay-address-and-does-not-fit.trycloudflare.com"};
  for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++) if(server_address_clean(bad[i],out)) { printf("accepted: %s\n",bad[i]); assert(0); }
  assert(strlen("kingston-inside-best-graphic.trycloudflare.com")<=MAX_IPV4_LEN && MAX_IPV4_LEN-OLD_IPV4_LEN==48);
  assert(!server_address_named("1.2.3.4:444") && server_address_named("a.b") && server_address_named("localhost:4444"));

  // Where to connect. A number needs no lookup and sends no Host of its own.
  char url[160],host[MAX_IPV4_LEN+1]="x";
  assert(server_address_url("1.2.3.4:444","/slither",url,sizeof(url),host,sizeof(host),1000));
  assert(!strcmp(url,"ws://1.2.3.4:444/slither") && !host[0]);
  // A name is looked up by Windows and keeps its name for the Host header.
  assert(server_address_url("localhost:4444","/ptc",url,sizeof(url),host,sizeof(host),3000));
  assert(!strcmp(url,"ws://127.0.0.1:4444/ptc") && !strcmp(host,"localhost"));
  assert(server_address_url("localhost","/slither",url,sizeof(url),host,sizeof(host),3000) && !strcmp(url,"ws://127.0.0.1:80/slither"));
  // A name that does not exist fails, within the time allowed.
  assert(!server_address_url("no-such-name-for-this-test.invalid","/slither",url,sizeof(url),host,sizeof(host),3000) && !host[0]);
  assert(!server_address_url("","/slither",url,sizeof(url),host,sizeof(host),100) && !server_address_url(NULL,"/",url,sizeof(url),host,sizeof(host),100));

  // The request mongoose writes names the number; the relay must be given the name instead.
  struct mg_mgr mgr; mg_mgr_init(&mgr);
  struct mg_connection* c=mg_ws_connect(&mgr,"ws://127.0.0.1:80/slither",handler,NULL,"%s:%s\r\n","Origin","https://slither.com");
  assert(c && c->send.len);
  char before[600]={0}; memcpy(before,c->send.buf,c->send.len<599?c->send.len:599);
  assert(strstr(before,"\r\nHost: 127.0.0.1\r\n"));
  assert(server_address_host(c,"kingston-inside-best-graphic.trycloudflare.com"));
  char after[700]={0}; memcpy(after,c->send.buf,c->send.len<699?c->send.len:699);
  assert(strstr(after,"\r\nHost: kingston-inside-best-graphic.trycloudflare.com\r\nConnection: Upgrade\r\n"));
  assert(!strstr(after,"127.0.0.1") && strstr(after,"GET /slither HTTP/1.1\r\nUpgrade: websocket\r\n"));
  assert(strstr(after,"Origin:https://slither.com\r\n\r\n") && after[strlen(after)-1]=='\n');
  assert(strlen(after)==strlen(before)-strlen("127.0.0.1")+strlen("kingston-inside-best-graphic.trycloudflare.com"));
  assert(!server_address_host(c,"") && !server_address_host(NULL,"a.b"));   // a number: nothing to change
  mg_mgr_free(&mgr);
  puts("server address test passed");
  return 0;
}
