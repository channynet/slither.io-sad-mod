#include "server_history.h"
#include "server_address.h"
#include "../platform/compat.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
server_history recent_servers;
bool server_history_save_failed;
static const char* history_path="server-history.txt";

// IPv4:port or a server name; see server_address.h.
static bool normalize(const char* input,char output[MAX_IPV4_LEN+1]) { return server_address_clean(input,output); }
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
bool server_history_remove(server_history* history,const char* address) {
  char clean[MAX_IPV4_LEN+1]; if(!normalize(address,clean)) return false;
  for(int i=0;i<history->count;i++) if(!strcmp(history->addresses[i],clean)) {
    for(int j=i;j+1<history->count;j++) strcpy(history->addresses[j],history->addresses[j+1]);
    history->count--; memset(history->addresses[history->count],0,sizeof(history->addresses[0]));
    return true;
  }
  return false;
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
// Removes one saved address on the user's request and persists the shorter list.
bool server_history_forget(const char* address) {
  if(!server_history_remove(&recent_servers,address)) return false;
  server_history_save_failed=!server_history_save(history_path,&recent_servers);
  return true;
}
void server_history_init(const char* last_address) {
  server_history_load(history_path,&recent_servers);
  server_history_remember(last_address);
}
