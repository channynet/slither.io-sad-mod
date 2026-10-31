#include "server_address.h"
#include "../external/mongoose.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include "../platform/compat.h"
#include <stdlib.h>

typedef struct { char name[MAX_IPV4_LEN+1]; char number[32]; volatile LONG users; HANDLE done; } lookup;
static void release(lookup* l) { if(!InterlockedDecrement(&l->users)) { CloseHandle(l->done); free(l); } }
static DWORD WINAPI look_up(void* data) {
  lookup* l=data;
  struct addrinfo hints={0},*found=NULL; hints.ai_family=AF_INET; hints.ai_socktype=SOCK_STREAM;
  if(!getaddrinfo(l->name,NULL,&hints,&found) && found) {
    const unsigned char* a=(const unsigned char*)&((struct sockaddr_in*)found->ai_addr)->sin_addr;
    snprintf(l->number,sizeof(l->number),"%u.%u.%u.%u",a[0],a[1],a[2],a[3]);
  }
  if(found) freeaddrinfo(found);
  SetEvent(l->done); release(l);
  return 0;
}
bool server_address_url(const char* clean,const char* path,char* url,size_t url_size,char* host,size_t host_size,int timeout_ms) {
  if(host_size) host[0]=0;
  if(!clean || !*clean) return false;
  if(!server_address_named(clean)) return snprintf(url,url_size,"ws://%s%s",clean,path)<(int)url_size;
  const char* colon=strchr(clean,':'); size_t length=colon?(size_t)(colon-clean):strlen(clean);
  unsigned port=colon?(unsigned)atoi(colon+1):80;
  if(length>MAX_IPV4_LEN || length>=host_size) return false;
#ifdef _WIN32
  WSADATA wsa; WSAStartup(MAKEWORD(2,2),&wsa);     // counted by Windows; harmless when already started
#endif
  lookup* l=calloc(1,sizeof(*l)); if(!l) return false;
  memcpy(l->name,clean,length); l->users=2; l->done=CreateEventW(NULL,TRUE,FALSE,NULL);
  HANDLE thread=l->done?CreateThread(NULL,0,look_up,l,0,NULL):NULL;
  if(!thread) { if(l->done) CloseHandle(l->done); free(l); return false; }
  CloseHandle(thread);
  char number[32]={0};
  if(WaitForSingleObject(l->done,timeout_ms)==WAIT_OBJECT_0) strcpy(number,l->number);
  release(l);                                     // a slow lookup finishes by itself and frees this
  if(!number[0]) return false;
  memcpy(host,clean,length); host[length]=0;
  return snprintf(url,url_size,"ws://%s:%u%s",number,port,path)<(int)url_size;
}
bool server_address_host(struct mg_connection* c,const char* host) {
  if(!c || !host || !*host) return false;
  static const char label[]="\r\nHost: ";
  char* text=(char*)c->send.buf; size_t size=c->send.len,at=0; bool found=false;
  for(;at+sizeof(label)-1<=size;at++) if(!memcmp(text+at,label,sizeof(label)-1)) { found=true; break; }
  if(!found) return false;
  size_t start=at+sizeof(label)-1,end=start;
  while(end<size && text[end]!='\r') end++;
  if(end>=size) return false;
  mg_iobuf_del(&c->send,start,end-start);
  return mg_iobuf_add(&c->send,start,host,strlen(host))==strlen(host);
}
