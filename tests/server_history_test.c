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
  const char* invalid[]={"",NULL,"1.2.3.256:444","1.2.3.4:0","1.2.3.4:65536","1.2.3.4:444/x","1.2.3:444","ws://1.2.3.4:444","1.2.3.4:444\n5.6.7.8:444","1.2.3.4"};
  for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);i++) assert(!server_history_add(&history,invalid[i]));
  // A private server's online address is kept like any other, in its plain form.
  server_history named={0};
  assert(server_history_add(&named," https://Some-Words-Here.trycloudflare.com/ ") && !strcmp(named.addresses[0],"some-words-here.trycloudflare.com"));
  assert(!server_history_add(&named,"some-words-here.trycloudflare.com") && server_history_remove(&named,"some-words-here.trycloudflare.com") && !named.count);
  assert(history.count==2);
  for(int i=1;i<=25;i++) { char address[32]; snprintf(address,sizeof(address),"10.0.0.%d:444",i); assert(server_history_add(&history,address)); }
  assert(SERVER_HISTORY_LIMIT==20);
  assert(history.count==20 && !strcmp(history.addresses[0],"10.0.0.25:444") && !strcmp(history.addresses[19],"10.0.0.6:444"));
  // User-requested removal: middle, newest, oldest, unknown and malformed addresses.
  assert(server_history_remove(&history," 10.0.0.15:0444 "));
  assert(history.count==19 && !strcmp(history.addresses[9],"10.0.0.16:444") && !strcmp(history.addresses[10],"10.0.0.14:444"));
  assert(server_history_remove(&history,"10.0.0.25:444") && history.count==18 && !strcmp(history.addresses[0],"10.0.0.24:444"));
  assert(server_history_remove(&history,"10.0.0.6:444") && history.count==17 && !strcmp(history.addresses[16],"10.0.0.7:444"));
  assert(!history.addresses[17][0] && !history.addresses[19][0]);
  assert(!server_history_remove(&history,"10.0.0.15:444") && !server_history_remove(&history,"bad") && !server_history_remove(&history,NULL));
  assert(history.count==17);
  assert(server_history_add(&history,"10.0.0.15:444") && history.count==18 && !strcmp(history.addresses[0],"10.0.0.15:444"));
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
  // Forgetting persists: the removed address must not return after a reload.
  assert(server_history_forget("5.6.7.8:443") && !server_history_save_failed && !server_history_forget("5.6.7.8:443"));
  assert(recent_servers.count==1 && !strcmp(recent_servers.addresses[0],"1.2.3.4:444"));
  memset(&recent_servers,0,sizeof(recent_servers));
  server_history_init("");
  assert(recent_servers.count==1 && !strcmp(recent_servers.addresses[0],"1.2.3.4:444"));
  puts("Server history: validation, normalization, deduplication, 20-entry MRU limit, removal, persistence and previous-address migration passed");
}
