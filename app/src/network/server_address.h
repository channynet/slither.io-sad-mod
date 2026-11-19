#ifndef SERVER_ADDRESS_H
#define SERVER_ADDRESS_H
// What may be typed into the server box:
//   1.2.3.4:444                              a server by number (as before)
//   some-words.trycloudflare.com             a server by name: the "online address" a private
//   name.example.com:8080                    server prints when it is reachable through a relay
// Cloudflare quick tunnels use TLS automatically. Explicit https/wss links retain TLS.
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "../constants.h"

// Strict IPv4:port, written back in its plain form.
static inline bool server_address_number(const char* input,char output[MAX_IPV4_LEN+1]) {
  if(!input) return false;
  const char* p=input; while(isspace((unsigned char)*p)) p++;
  unsigned parts[5]={0};
  for(int i=0;i<5;i++) {
    int digits=0;
    while(isdigit((unsigned char)*p)) {
      if(++digits>5) return false;
      parts[i]=parts[i]*10+(unsigned)(*p++-'0');
    }
    if(!digits || parts[i]>(i<4?255u:65535u)) return false;
    if(i<4 && *p++!=(i<3?'.':':')) return false;
  }
  while(isspace((unsigned char)*p)) p++;
  if(*p || !parts[4]) return false;
  int n=snprintf(output,MAX_IPV4_LEN+1,"%u.%u.%u.%u:%u",parts[0],parts[1],parts[2],parts[3],parts[4]);
  return n>0 && n<=MAX_IPV4_LEN;
}
static inline bool server_address_relay(const char* host) {
  const char* suffix=".trycloudflare.com";
  size_t n=strcspn(host,":"), k=strlen(suffix);
  return n>k && !strncmp(host+n-k,suffix,k);
}
// A host name with an optional port. Keep secure schemes when normalizing saved links.
static inline bool server_address_name(const char* input,char output[MAX_IPV4_LEN+1]) {
  if(!input) return false;
  const char* p=input; while(isspace((unsigned char)*p)) p++;
  static const char* schemes[]={"https://","http://","wss://","ws://"};
  bool secure=false;
  for(int i=0;i<4;i++) {
    size_t n=strlen(schemes[i]); bool same=true;
    for(size_t k=0;k<n && same;k++) same=tolower((unsigned char)p[k])==schemes[i][k];
    if(same) { p+=n; secure=i==0 || i==2; break; }
  }
  char host[MAX_IPV4_LEN+1]; int n=0; bool letter=false,dot=false; char last='.';
  for(;*p && *p!=':' && *p!='/' && !isspace((unsigned char)*p);p++) {
    char c=(char)tolower((unsigned char)*p);
    if(!(isalnum((unsigned char)c) || c=='-' || c=='.')) return false;
    if(c=='.' && last=='.') return false;          // empty label
    if(c=='.') dot=true; else if(isalpha((unsigned char)c)) letter=true;
    if(n>=MAX_IPV4_LEN) return false;
    host[n++]=last=c;
  }
  if(n && host[n-1]=='.') n--;                     // "name.com." is the same name
  host[n]=0;
  if(!n || !letter || host[0]=='-' || host[n-1]=='-' || (!dot && strcmp(host,"localhost"))) return false;
  unsigned port=secure?443:80;
  if(*p==':') {
    p++; port=0; int digits=0;
    while(isdigit((unsigned char)*p)) { if(++digits>5) return false; port=port*10+(unsigned)(*p++-'0'); }
    if(!digits || !port || port>65535) return false;
  }
  if(*p=='/') while(*p && !isspace((unsigned char)*p)) p++;   // the rest of a pasted link
  while(isspace((unsigned char)*p)) p++;
  if(*p) return false;
  // Old saved quick-tunnel names (including :80) must also upgrade to TLS.
  if(server_address_relay(host) && (port==80 || port==443)) { secure=false; port=80; }
  int total;
  if(secure) total=port==443?snprintf(output,MAX_IPV4_LEN+1,"wss://%s",host):snprintf(output,MAX_IPV4_LEN+1,"wss://%s:%u",host,port);
  else total=port==80?snprintf(output,MAX_IPV4_LEN+1,"%s",host):snprintf(output,MAX_IPV4_LEN+1,"%s:%u",host,port);
  return total>0 && total<=MAX_IPV4_LEN;
}
static inline bool server_address_clean(const char* input,char output[MAX_IPV4_LEN+1]) {
  return server_address_number(input,output) || server_address_name(input,output);
}
// True for an address by name (already cleaned).
static inline bool server_address_named(const char* clean) {
  for(;clean && *clean && *clean!=':';clean++) if(isalpha((unsigned char)*clean)) return true;
  return false;
}

struct mg_connection;
// Works out where to connect: `url` is ws[s]://number:port/path, and for a named address
// `host` is its HTTP authority (including a non-default port; empty for plain IPv4). The name
// is looked up with Windows' own resolver, on another thread, for at most timeout_ms.
bool server_address_url(const char* clean,const char* path,char* url,size_t url_size,char* host,size_t host_size,int timeout_ms);
// A relay tells its servers apart by the Host header: put the name in place of the number
// in the WebSocket request mongoose has just written, and configure verified TLS if needed.
// Call right after mg_ws_connect, before polling the connection.
bool server_address_host(struct mg_connection* c,const char* host);
#endif
