#ifndef SERVER_STATUS_H
#define SERVER_STATUS_H
#include <stdbool.h>
#include <stdint.h>
#include "server_history.h"
typedef struct {
  int players,ping_ms;
  int64_t players_at,ping_at;
  bool pending,failed,game_ping;
} server_status;
void server_status_init(void);
void server_status_destroy(void);
void server_status_menu(bool allowed);
void server_status_request(const char* address);
bool server_status_get(const char* address,server_status* out);
void server_status_population(const char* address,int players);
void server_status_game_ping(const char* address,float milliseconds);
#endif
