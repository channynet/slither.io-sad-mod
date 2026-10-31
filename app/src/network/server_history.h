#ifndef SERVER_HISTORY_H
#define SERVER_HISTORY_H
#include <stdbool.h>
#include "../constants.h"
#define SERVER_HISTORY_LIMIT 20
typedef struct {
  int count;
  char addresses[SERVER_HISTORY_LIMIT][MAX_IPV4_LEN+1];
} server_history;
extern server_history recent_servers;
extern bool server_history_save_failed;
bool server_history_add(server_history* history,const char* address);
bool server_history_remove(server_history* history,const char* address);
bool server_history_load(const char* path,server_history* history);
bool server_history_save(const char* path,const server_history* history);
void server_history_init(const char* last_address);
void server_history_remember(const char* address);
bool server_history_forget(const char* address);
#endif
