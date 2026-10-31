#ifndef BODY_HUG_WORKER_H
#define BODY_HUG_WORKER_H
// The body hug's planning (a few milliseconds) runs on its own thread, so frames never wait for
// it: the main thread hands in a copy of what it sees and picks the answer up when it is ready.
#include "body_hug.h"
#include <stdbool.h>

// Hands in a job (copied; a job not yet started is replaced). `id` comes back with its answer.
void hug_worker_submit(const hug_input* in, const hug_state* st, int id);
// The latest answer not yet taken: true once per finished job, with the job's id.
bool hug_worker_take(hug_output* out, hug_state* st, int* id);
// True while a job is waiting or being worked on.
bool hug_worker_busy(void);
#endif
