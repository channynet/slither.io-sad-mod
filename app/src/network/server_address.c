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
#include <time.h>

// Desktop systems already have wall-clock time. Do not require a separate UDP/NTP
// connection (often blocked on other networks) just to verify a server certificate.
static volatile LONG tls_clock_ready;
static void initialize_tls_clock(void) {
  if(InterlockedCompareExchange(&tls_clock_ready,1,0)==0) {
    mg_boot_timestamp_ms=(uint64_t)time(NULL)*1000-mg_millis();
    InterlockedExchange(&tls_clock_ready,2);
  } else while(InterlockedCompareExchange(&tls_clock_ready,2,2)!=2) Sleep(1);
}

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
  bool secure=!strncmp(clean,"wss://",6);
  if(secure) clean+=6;
  const char* colon=strchr(clean,':'); size_t length=colon?(size_t)(colon-clean):strlen(clean);
  unsigned port=colon?(unsigned)atoi(colon+1):(secure?443:80);
  if(server_address_relay(clean) && (port==80 || port==443)) { secure=true; port=443; }
  if(port==443) secure=true;  // compatibility with previously saved https links
  if(length>MAX_IPV4_LEN || strlen(clean)>=host_size) return false;
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
  if((secure && port==443) || (!secure && port==80)) { memcpy(host,clean,length); host[length]=0; }
  else snprintf(host,host_size,"%.*s:%u",(int)length,clean,port);
  return snprintf(url,url_size,"%s://%s:%u%s",secure?"wss":"ws",number,port,path)<(int)url_size;
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
  if(mg_iobuf_add(&c->send,start,host,strlen(host))!=strlen(host)) { c->is_closing=true; return false; }
  if(c->is_tls && !c->tls) {
    initialize_tls_clock();
    struct mg_str ca=mg_file_read(&mg_fs_posix,"app/res/certs/ca.pem");
    if(!ca.len) { mg_error(c,"TLS trust store missing: app/res/certs/ca.pem"); return false; }
    char name[MAX_IPV4_LEN+1]; size_t n=strcspn(host,":");
    if(n>=sizeof(name)) { mg_free((void*)ca.buf); c->is_closing=true; return false; }
    memcpy(name,host,n); name[n]=0;
    struct mg_tls_opts opts={0}; opts.ca=ca; opts.name=mg_str(name);
    mg_tls_init(c,&opts);
    mg_free((void*)ca.buf);
    if(!c->tls || c->is_closing) return false;
  }
  return true;
}
