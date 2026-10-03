#include "server_history.h"
#include <windows.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
server_history recent_servers;
bool server_history_save_failed;
static const char* history_path="server-history.txt";

// Accept only the IPv4:port format supported by the existing game input.
static bool normalize(const char* input,char output[MAX_IPV4_LEN+1]) {
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
bool server_history_add(server_history* history,const char* address) {
  char clean[MAX_IPV4_LEN+1]; if(!normalize(address,clean)) return false;
  int at=history->count;
  for(int i=0;i<history->count;i++) if(!strcmp(history->addresses[i],clean)) { at=i; break; }
  if(at==0 && history->count>0) return false;
  if(at==history->count && history->count<SERVER_HISTORY_LIMIT) history->count++;
  if(at>=SERVER_HISTORY_LIMIT) at=SERVER_HISTORY_LIMIT-1;
  memmove(history->addresses[1],history->addresses[0],(size_t)at*sizeof(history->addresses[0]));
  strcpy(history->addresses[0],clean); return true;
}
bool server_history_load(const char* path,server_history* history) {
  memset(history,0,sizeof(*history));
  FILE* f=fopen(path,"rb"); if(!f) return false;
  char data[2048]; size_t length=fread(data,1,sizeof(data)-1,f);
  bool ok=!ferror(f) && feof(f); fclose(f);
  if(!ok || memchr(data,0,length)) return false;
  data[length]=0;
  // The file is newest first. Retain that order and ignore malformed/duplicate rows.
  char* line=data;
  while(*line && history->count<SERVER_HISTORY_LIMIT) {
    char* end=strchr(line,'\n'); if(end) *end=0;
    char clean[MAX_IPV4_LEN+1];
    if(normalize(line,clean)) {
      bool duplicate=false;
      for(int i=0;i<history->count;i++) if(!strcmp(history->addresses[i],clean)) duplicate=true;
      if(!duplicate) strcpy(history->addresses[history->count++],clean);
    }
    if(!end) break; line=end+1;
  }
  return true;
}
bool server_history_save(const char* path,const server_history* history) {
  char temp[MAX_PATH]; if(snprintf(temp,sizeof(temp),"%s.tmp",path)>=(int)sizeof(temp)) return false;
  FILE* f=fopen(temp,"wb"); if(!f) return false;
  bool ok=true;
  for(int i=0;i<history->count;i++) if(fprintf(f,"%s\n",history->addresses[i])<0) ok=false;
  if(fclose(f)) ok=false;
  return ok && MoveFileExA(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
void server_history_remember(const char* address) {
  bool changed=server_history_add(&recent_servers,address);
  if(changed || server_history_save_failed)
    server_history_save_failed=!server_history_save(history_path,&recent_servers);
}
void server_history_init(const char* last_address) {
  server_history_load(history_path,&recent_servers);
  server_history_remember(last_address);
}
