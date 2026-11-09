#include "../app/src/network/client_access.c"
#include <assert.h>
static volatile LONG calls, mode, hold;
static HANDLE release_reply;

int http_request(const char* method, const char* url, const char* type, const void* body, size_t body_len,
    unsigned char** out, size_t* len, size_t max_out, int timeout_ms) {
  int snapshot = InterlockedCompareExchange(&mode, 0, 0);
  assert(!strcmp(method, "POST") && !strcmp(url, CLIENT_ACCESS_URL));
  assert(!strcmp(type, "application/json") && !memcmp(body, "{\"version\":", 11));
  assert(body_len == strlen(body) && max_out == 257 && timeout_ms == 3000);
  InterlockedIncrement(&calls);
  if (InterlockedCompareExchange(&hold, 0, 0)) WaitForSingleObject(release_reply, 3000);
  const char* reply = snapshot == 0 ? "{\"allowed\":true}" : snapshot == 1 ? "{\"allowed\":false}" : "{}";
  *len = strlen(reply); *out = malloc(*len + 1); memcpy(*out, reply, *len + 1);
  return snapshot == 0 ? 200 : snapshot == 1 ? 403 : 503;
}
static void finish(void) {
  ULONGLONG start = GetTickCount64();
  for (;;) {
    EnterCriticalSection(&access_state.lock); bool pending = access_state.pending; LeaveCriticalSection(&access_state.lock);
    if (!pending) break;
    assert(GetTickCount64() - start < 4000); Sleep(1);
  }
}
static void due(void) {
  EnterCriticalSection(&access_state.lock); access_state.due = 0; LeaveCriticalSection(&access_state.lock);
}
int main(void) {
  assert(parse_reply(200, "{\"allowed\":true}", 16) == ACCESS_ALLOWED);
  assert(parse_reply(403, "{\"allowed\":false}", 17) == ACCESS_BANNED);
  const char* invalid[] = {"{}", "{\"allowed\":\"false\"}", "{\"allowed\":false} trailing", "{\"allowed\":false", "[]"};
  for (size_t i=0; i<sizeof(invalid)/sizeof(*invalid); i++) assert(parse_reply(403, invalid[i], strlen(invalid[i])) == ACCESS_OFFLINE);
  assert(parse_reply(503, "{\"allowed\":false}", 17) == ACCESS_OFFLINE);
  assert(parse_reply(403, "{\"allowed\":true}", 16) == ACCESS_OFFLINE);
  release_reply = CreateEventW(NULL, TRUE, FALSE, NULL);
  client_access_init(); Sleep(10); assert(calls == 0); // no telemetry before Play
  client_access_request(); finish(); assert(client_access_state() == ACCESS_ALLOWED && calls == 1);
  for (int i=0; i<100; i++) client_access_heartbeat(true);
  Sleep(10); assert(calls == 1); // frame-independent throttle
  due(); client_access_heartbeat(false); Sleep(10); assert(calls == 1);
  InterlockedExchange(&mode, 1); client_access_heartbeat(true); finish();
  assert(client_access_state() == ACCESS_BANNED && calls == 2);
  due(); client_access_heartbeat(true); Sleep(10); assert(calls == 2);
  InterlockedExchange(&mode, 0); client_access_request(); finish(); assert(client_access_state() == ACCESS_ALLOWED);
  InterlockedExchange(&mode, 2); client_access_request(); finish(); assert(client_access_state() == ACCESS_OFFLINE);
  // In-flight replies for an earlier Play request cannot change the new request's decision.
  InterlockedExchange(&hold, 1); InterlockedExchange(&mode, 1);
  int before = calls; client_access_request();
  ULONGLONG start = GetTickCount64();
  while (calls == before) { assert(GetTickCount64()-start < 2000); Sleep(1); }
  InterlockedExchange(&mode, 0); client_access_request();
  assert(client_access_state() == ACCESS_PENDING);
  InterlockedExchange(&hold, 0); SetEvent(release_reply); finish();
  assert(client_access_state() == ACCESS_ALLOWED && calls == before + 2);
  client_access_destroy(); CloseHandle(release_reply);
  assert(client_access_state() == ACCESS_IDLE);
  puts("Client access: strict decisions, outage fallback, request throttle, live bans, unblock and stale replies passed");
}
