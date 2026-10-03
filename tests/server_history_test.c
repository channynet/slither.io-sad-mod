#include "../app/src/network/server_history.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  server_history history={0},loaded;
  assert(server_history_add(&history," 127.000.0.1:00444 \r\n"));
  assert(history.count==1 && !strcmp(history.addresses[0],"127.0.0.1:444"));
  assert(!server_history_add(&history,"127.0.0.1:444"));
  assert(server_history_add(&history,"10.0.0.2:65535"));
  assert(server_history_add(&history,"127.0.0.1:444"));
  assert(history.count==2 && !strcmp(history.addresses[1],"10.0.0.2:65535"));
  const char* invalid[]={"",NULL,"1.2.3.256:444","1.2.3.4:0","1.2.3.4:65536","1.2.3.4:444/x","1.2.3:444","ws://1.2.3.4:444","1.2.3.4:444\n5.6.7.8:444","255.255.255.255:65535"};
  for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);i++) assert(!server_history_add(&history,invalid[i]));
  assert(history.count==2);
  for(int i=1;i<=20;i++) { char address[32]; snprintf(address,sizeof(address),"10.0.0.%d:444",i); assert(server_history_add(&history,address)); }
  assert(history.count==16 && !strcmp(history.addresses[0],"10.0.0.20:444") && !strcmp(history.addresses[15],"10.0.0.5:444"));
  assert(server_history_save("history.txt",&history));
  assert(server_history_load("history.txt",&loaded));
  assert(!memcmp(&history,&loaded,sizeof(history)));
  FILE* f=fopen("history.txt","wb"); assert(f);
  fputs("1.2.3.4:444\r\nbad\n1.2.3.4:444\n5.6.7.8:443\n",f); fclose(f);
  assert(server_history_load("history.txt",&loaded)); assert(loaded.count==2);
  assert(!strcmp(loaded.addresses[0],"1.2.3.4:444") && !strcmp(loaded.addresses[1],"5.6.7.8:443"));
  assert(!server_history_load("missing.txt",&loaded) && !loaded.count);
  assert(!server_history_save("missing-directory/history.txt",&history));
  server_history_init("1.2.3.4:444"); server_history_remember("5.6.7.8:443");
  memset(&recent_servers,0,sizeof(recent_servers));
  server_history_init("5.6.7.8:443");
  assert(recent_servers.count==2 && !strcmp(recent_servers.addresses[0],"5.6.7.8:443"));
  puts("Server history: validation, normalization, deduplication, MRU limit, persistence and previous-address migration passed");
}
