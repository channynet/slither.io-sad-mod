#ifndef SERVER_H
#define SERVER_H

#include <thermite.h>
#include <stdbool.h>
#include "../external/mongoose.h"

void server_init(tenv* env);
void server_connect(tenv* env);
// At WebSocket open: true when this connection took the slow path and is being replaced.
bool server_path_check(tenv* env, struct mg_connection* c);
bool server_path_retrying(void);
void server_poll(tenv* env);
void server_service(tenv* env);
void server_destroy(tenv* env);

#endif
