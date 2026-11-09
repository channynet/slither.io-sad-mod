#ifndef CLIENT_ACCESS_H
#define CLIENT_ACCESS_H
#include <stdbool.h>
typedef enum { ACCESS_IDLE, ACCESS_PENDING, ACCESS_ALLOWED, ACCESS_BANNED, ACCESS_OFFLINE } client_access_result;
// Only the fixed HTTPS management service decides access. A custom tag relay cannot replace it.
void client_access_init(void);
void client_access_request(void);
void client_access_heartbeat(bool playing);
client_access_result client_access_state(void);
void client_access_destroy(void);
#endif
