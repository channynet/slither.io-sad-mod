#ifndef SERVER_CATALOG_H
#define SERVER_CATALOG_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../constants.h"
#define SERVER_CATALOG_LIMIT 256
#define SERVER_CATALOG_URL "https://leaderboard.0x0efe.dev/api/leaderboard"
#define SERVER_CATALOG_SOURCE "https://leaderboard.0x0efe.dev/"
typedef struct { int rank, score; char name[128]; } server_leader;
typedef struct {
  char address[MAX_IPV4_LEN+1], country[3], region[3], location[80];
  int players, leader_count;
  int64_t updated_at;
  server_leader leaders[10];
} server_catalog_entry;
typedef struct {
  int count, http_status;
  bool pending, failed;
  int64_t received_at;
  server_catalog_entry entries[SERVER_CATALOG_LIMIT];
} server_catalog;
// Returns false for an invalid response. Never changes out on failure.
bool server_catalog_parse(const char* json, size_t length, server_catalog* out);
void server_catalog_init(void);
void server_catalog_destroy(void);
void server_catalog_request(void); // At most once per 30 seconds; call only while the picker is open.
void server_catalog_copy(server_catalog* out);
#endif
