#include "../app/src/network/ping_tracker.h"
#include "../app/src/network/socket_latency.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  ping_tracker p={0}; int display=0;
  assert(!ping_received(&p,40,&display));
  ping_begin(&p,1000,21); ping_written(&p,1002,14); assert(!p.flushed);
  ping_written(&p,1003,7); assert(p.flushed && p.sent==1003);
  assert(ping_received(&p,1040,&display) && display==40 && p.queue_ms==3);
  assert(!ping_received(&p,1041,&display));
  for(int i=0;i<8;i++) { ping_begin(&p,2000+i*250,7); ping_written(&p,2001+i*250,7); assert(ping_received(&p,2080+i*250,&display)); }
  assert(display==80);
  for(int i=0;i<8;i++) { ping_begin(&p,5000+i*250,7); assert(ping_received(&p,5040+i*250,&display)); }
  assert(display==40 && p.queue_ms==-1);
  memset(&p,0,sizeof(p)); ping_begin(&p,10,7); assert(ping_received(&p,50,&display) && display==40);
  ping_begin(&p,100,7); assert(!ping_received(&p,50,&display));
  WSADATA ws; assert(!WSAStartup(MAKEWORD(2,2),&ws));
  SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP); assert(s!=INVALID_SOCKET);
  bool ack=socket_prompt_ack(s); int nodelay=0,len=sizeof(nodelay);
  assert(!getsockopt(s,IPPROTO_TCP,TCP_NODELAY,(char*)&nodelay,&len) && nodelay==1);
  assert(!socket_prompt_ack(INVALID_SOCKET));
  printf("Ping: each-sample display, 80->40 recovery, partial writes, duplicate rejection, reset; prompt ACK support %s\n",ack?"yes":"no (fallback)");
  closesocket(s); WSACleanup();
}
