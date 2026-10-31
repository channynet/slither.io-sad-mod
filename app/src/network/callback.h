#ifndef CALLBACK_H
#define CALLBACK_H

#include "../external/mongoose.h"

void server_callback(struct mg_connection* c, int ev, void *ev_data);
// Why the last game connection ended early; empty when there is nothing to report.
const char* server_last_error(void);
// Record why the current game connection is about to end (first reason wins).
// One line per connection is appended to session-log.txt when it closes.
void server_note_end(const char* format, ...);
void server_clear_error(void);
void server_set_error(const char* format, ...);

#endif