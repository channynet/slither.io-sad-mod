#include "client_access.h"
#include "http_simple.h"
#include "../external/mongoose.h"
#include "../platform/compat.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef CLIENT_ACCESS_URL
#define CLIENT_ACCESS_URL "https://slither-tag-share.sadxxx.workers.dev/v1/access"
#endif
#ifndef APP_VERSION
#define APP_VERSION "unknown"
#endif
#define ACCESS_INTERVAL_MS 60000
static struct {
  bool initialized, pending;
  unsigned generation;
  client_access_result result;
  ULONGLONG due;
  CRITICAL_SECTION lock;
  HANDLE wake, thread;
  volatile LONG stop;
} access_state;

static client_access_result parse_reply(int status, const char* body, size_t length) {
  if (!body || !length || length > 256) return ACCESS_OFFLINE;
  struct mg_str json = mg_str_n(body, length);
  int root_length = 0, at = mg_json_get(json, "$", &root_length);
  if (at < 0 || root_length < 2 || body[at] != '{') return ACCESS_OFFLINE;
  for (size_t i = (size_t)at + root_length; i < length; i++)
    if (!body[i] || !strchr(" \t\r\n", body[i])) return ACCESS_OFFLINE;
  bool allowed;
  if (!mg_json_get_bool(json, "$.allowed", &allowed)) return ACCESS_OFFLINE;
  if (status == 200 && allowed) return ACCESS_ALLOWED;
  if (status == 403 && !allowed) return ACCESS_BANNED;
  return ACCESS_OFFLINE;
}

static DWORD WINAPI access_worker(void* unused) {
  (void)unused;
  while (!InterlockedCompareExchange(&access_state.stop, 0, 0)) {
    WaitForSingleObject(access_state.wake, INFINITE);
    while (!InterlockedCompareExchange(&access_state.stop, 0, 0)) {
      EnterCriticalSection(&access_state.lock);
      bool pending = access_state.pending;
      unsigned generation = access_state.generation;
      LeaveCriticalSection(&access_state.lock);
      if (!pending) break;
      const char body[] = "{\"version\":\"" APP_VERSION "\"}";
      unsigned char* reply = NULL;
      size_t length = 0;
      int status = http_request("POST", CLIENT_ACCESS_URL, "application/json", body, sizeof(body)-1,
                                &reply, &length, 257, 3000);
      client_access_result result = parse_reply(status, (const char*)reply, length);
      free(reply);
      EnterCriticalSection(&access_state.lock);
      // A result for an earlier Play request cannot release a newer pending request.
      if (generation == access_state.generation) {
        access_state.result = result;
        access_state.pending = false;
        access_state.due = GetTickCount64() + ACCESS_INTERVAL_MS;
      }
      LeaveCriticalSection(&access_state.lock);
    }
  }
  return 0;
}

void client_access_init(void) {
  if (access_state.initialized) return;
  InitializeCriticalSection(&access_state.lock);
  access_state.initialized = true;
  access_state.result = ACCESS_IDLE;
  access_state.wake = CreateEventW(NULL, FALSE, FALSE, NULL);
  if (access_state.wake) access_state.thread = CreateThread(NULL, 0, access_worker, NULL, 0, NULL);
}

void client_access_request(void) {
  client_access_init();
  EnterCriticalSection(&access_state.lock);
  access_state.generation++;
  access_state.pending = access_state.thread != NULL;
  access_state.result = access_state.pending ? ACCESS_PENDING : ACCESS_OFFLINE;
  LeaveCriticalSection(&access_state.lock);
  if (access_state.thread) SetEvent(access_state.wake);
}

void client_access_heartbeat(bool playing) {
  if (!playing || !access_state.initialized || !access_state.thread) return;
  EnterCriticalSection(&access_state.lock);
  bool refresh = !access_state.pending && access_state.result != ACCESS_BANNED &&
                 GetTickCount64() >= access_state.due;
  if (refresh) { access_state.pending = true; access_state.generation++; }
  LeaveCriticalSection(&access_state.lock);
  if (refresh) SetEvent(access_state.wake);
}

client_access_result client_access_state(void) {
  if (!access_state.initialized) return ACCESS_IDLE;
  EnterCriticalSection(&access_state.lock);
  client_access_result result = access_state.result;
  LeaveCriticalSection(&access_state.lock);
  return result;
}

void client_access_destroy(void) {
  if (!access_state.initialized) return;
  InterlockedExchange(&access_state.stop, 1);
  if (access_state.wake) SetEvent(access_state.wake);
  if (access_state.thread) { WaitForSingleObject(access_state.thread, INFINITE); CloseHandle(access_state.thread); }
  if (access_state.wake) CloseHandle(access_state.wake);
  DeleteCriticalSection(&access_state.lock);
  memset(&access_state, 0, sizeof(access_state));
}
