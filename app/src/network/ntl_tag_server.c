#include "ntl_tag_server.h"
#include "../game/ntl_tags.h"
#include "../user.h"
#include "ntl.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define NTL_TAG_SETTINGS "ntl-tags.json"
#ifndef NTL_TAG_SERVER_HOST
#define NTL_TAG_SERVER_HOST L"ws.ntl-slither.com"   // (NTL: ws://ws.ntl-slither.com:9000)
#endif

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>

// What the main thread wants the connection to say (under lock).
typedef struct {
  bool want;
  int generation;                 // changes whenever the first message would change: reconnect
  char hello[1024];               // NTL's first message
  char key[1024];                 // what in it would make a new connection (all but the position)
  int x, y;                       // latest position
} wish;
static struct {
  CRITICAL_SECTION lock;
  HANDLE thread, wake;
  wish w;
  volatile LONG stop;
  char status[64];
  // password check
  HANDLE check_thread;
  volatile LONG check_state;      // 0 idle, 1 busy, 2 ok, 3 refused
  int check_id;
  char check_pass[33], check_msg[256];
  // recheck of stored passwords: 1 per stored entry kept, 0 dropped (-1: no answer)
  volatile LONG recheck_ready;
  int recheck_count, recheck_ids[32], recheck_ok[32];
  bool ready;
} S;

static void set_status(const char* s) { EnterCriticalSection(&S.lock); snprintf(S.status, sizeof S.status, "%s", s); LeaveCriticalSection(&S.lock); }

static void json_string(char* out, size_t cap, const char* s) {
  size_t n = 0;
  if (n + 1 < cap) out[n++] = '"';
  for (; *s && n + 7 < cap; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '"' || c == '\\') { out[n++] = '\\'; out[n++] = (char)c; }
    else if (c < 32) n += snprintf(out + n, cap - n, "\\u%04x", c);
    else out[n++] = (char)c;
  }
  if (n + 1 < cap) out[n++] = '"';
  out[n] = 0;
}

// One connection: the first message, then the position each second; a reader thread takes the maps.
// (the reader uses only these, so it may outlive its connection's function safely)
static HINTERNET reader_ws;
static volatile LONG reader_done;
static HANDLE reader_thread;
static DWORD WINAPI reader(void* unused) {
  (void)unused;
  static uint8_t buffer[65536];
  static uint16_t ids[4096]; static int16_t tags[4096];
  DWORD total = 0;
  for (;;) {
    DWORD got = 0; WINHTTP_WEB_SOCKET_BUFFER_TYPE type;
    if (WinHttpWebSocketReceive(reader_ws, buffer + total, sizeof buffer - total, &got, &type) != NO_ERROR) break;
    if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) break;
    total += got;
    bool fragment = type == WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE || type == WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE;
    if (fragment && total < sizeof buffer) continue;
    if (type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE || type == WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE) {
      // NTL id (2 bytes) and tag (2 bytes, 65535: none) for each NTL player there
      int n = 0;
      for (DWORD i = 0; i + 3 < total && n < 4096; i += 4, n++) {
        ids[n] = (uint16_t)(buffer[i] << 8 | buffer[i + 1]);
        int t = buffer[i + 2] << 8 | buffer[i + 3];
        tags[n] = (int16_t)(t == 65535 || t >= NTL_TAG_MAX ? -1 : t);
      }
      ntl_tag_server_map_set(ids, tags, n);
    }
    total = 0;
  }
  InterlockedExchange(&reader_done, 1);
  return 0;
}
static bool send_text(HINTERNET ws, const char* text) {
  return WinHttpWebSocketSend(ws, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, (void*)text, (DWORD)strlen(text)) == NO_ERROR;
}
static void run_connection(HINTERNET session, const wish* first) {
  if (reader_thread) { WaitForSingleObject(reader_thread, INFINITE); CloseHandle(reader_thread); reader_thread = NULL; }   // (the last one's)
  HINTERNET connection = WinHttpConnect(session, NTL_TAG_SERVER_HOST, 9000, 0), request = NULL, ws = NULL;
  if (!connection) return;
  request = WinHttpOpenRequest(connection, L"GET", L"/", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
  if (!request || !WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0) ||
      !WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0) || !WinHttpReceiveResponse(request, NULL)) goto done;
  ws = WinHttpWebSocketCompleteUpgrade(request, 0);
  WinHttpCloseHandle(request); request = NULL;
  if (!ws || !send_text(ws, first->hello)) goto done;
  set_status("연결됨");
  reader_ws = ws; InterlockedExchange(&reader_done, 0);
  reader_thread = CreateThread(NULL, 0, reader, NULL, 0, NULL);
  if (!reader_thread) goto done;
  int last_x = -1, last_y = -1;
  for (;;) {
    if (WaitForSingleObject(S.wake, 1000) == WAIT_FAILED || S.stop || InterlockedCompareExchange(&reader_done, 0, 0)) break;
    EnterCriticalSection(&S.lock);
    bool still = S.w.want && S.w.generation == first->generation;
    int x = S.w.x, y = S.w.y;
    LeaveCriticalSection(&S.lock);
    if (!still) break;
    if (x != last_x || y != last_y) {
      char pos[64]; snprintf(pos, sizeof pos, "[%d,%d]", x, y);
      if (!send_text(ws, pos)) break;
      last_x = x; last_y = y;
    }
  }
  ntl_tag_server_map_clear();                  // (gone at once, whatever the close takes)
  WinHttpCloseHandle(ws); ws = NULL;            // closes the socket: the reader returns
  if (WaitForSingleObject(reader_thread, 5000) == WAIT_OBJECT_0) { CloseHandle(reader_thread); reader_thread = NULL; }
done:
  if (ws) WinHttpCloseHandle(ws);
  if (request) WinHttpCloseHandle(request);
  WinHttpCloseHandle(connection);
  ntl_tag_server_map_clear();
}
static DWORD WINAPI worker(void* unused) {
  (void)unused;
  HINTERNET session = WinHttpOpen(L"SlitherNative/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (session) WinHttpSetTimeouts(session, 4000, 4000, 4000, 0);
  int failures = 0;
  while (!S.stop) {
    EnterCriticalSection(&S.lock);
    wish w = S.w;
    LeaveCriticalSection(&S.lock);
    if (!w.want || !session) { set_status("꺼짐"); failures = 0; WaitForSingleObject(S.wake, 500); continue; }
    set_status("연결 중");
    run_connection(session, &w);
    if (S.stop) break;
    EnterCriticalSection(&S.lock);
    bool again = S.w.want && S.w.generation == w.generation;
    LeaveCriticalSection(&S.lock);
    if (again) {                                  // dropped: wait a little longer each time
      failures++;
      set_status("연결 안 됨 · 다시 시도");
      WaitForSingleObject(S.wake, failures < 5 ? 2000 * failures : 15000);
    } else failures = 0;
  }
  if (session) WinHttpCloseHandle(session);
  return 0;
}
static void start(void) {
  if (S.ready) return;
  S.ready = true;
  InitializeCriticalSection(&S.lock);
  S.wake = CreateEventW(NULL, FALSE, FALSE, NULL);
  snprintf(S.status, sizeof S.status, "꺼짐");
  S.thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
}

static bool private_address(const char* ip) {
  int a = -1, b = -1;
  if (sscanf(ip, "%d.%d", &a, &b) != 2) return true;            // (a name: no NTL tag server there)
  return a == 127 || a == 10 || (a == 192 && b == 168) || (a == 172 && b >= 16 && b <= 31);
}
void ntl_tag_server_tick(tenv* env) {
  start();
  // passwords: apply the answers that came back
  if (InterlockedCompareExchange(&S.recheck_ready, 0, 1) == 1) {
    bool changed = false;
    for (int i = 0; i < S.recheck_count; i++) if (!S.recheck_ok[i] && ntl_tag_auth_for(S.recheck_ids[i])) { ntl_tag_auth_set(S.recheck_ids[i], NULL); changed = true; }
    if (changed) ntl_tag_settings_save(NTL_TAG_SETTINGS);
  }
  game_data* g = &env->usr->gdata;
  snake* me = g->curr_screen == PLAYING && g->conn == CONNECTED && g->data.follow_view ? get_snake(g, g->data.snake_id) : NULL;
  int own = ntl_tag_own();
  bool bought = own >= 0 && ntl_tag_needs_password(own);
  bool want = me && !me->dead && (ntl_tag_prefs.show_others || bought) && !private_address(env->usr->usrs.ipv4);
  wish w = {0};
  w.want = want;
  if (want) {
    char identity[600], server[200];
    json_string(identity, sizeof identity, ntl_own_identity(env));
    json_string(server, sizeof server, env->usr->usrs.ipv4);
    const char* pass = bought ? ntl_tag_auth_for(own) : NULL;
    // [identity, "ip:port", password hash, tag, server number, x, y, NTL id] as NTL sends it
    snprintf(w.hello, sizeof w.hello, "[%s,%s,\"%s\",%d,%d,%d,%d,%d]", identity, server, pass ? pass : "", pass ? own : -1,
             g->data.server_number, (int)lroundf(me->xx), (int)lroundf(me->yy), me->ntl_id);
    w.x = (int)lroundf(me->xx); w.y = (int)lroundf(me->yy);
    snprintf(w.key, sizeof w.key, "%s|%s|%s|%d|%d|%d", identity, server, pass ? pass : "", pass ? own : -1, g->data.server_number, me->ntl_id);
  }
  EnterCriticalSection(&S.lock);
  bool same = S.w.want == w.want && !strcmp(S.w.key, w.key);
  if (same) { S.w.x = w.x; S.w.y = w.y; }
  else { int generation = S.w.generation + 1; S.w = w; S.w.generation = generation; }
  LeaveCriticalSection(&S.lock);
  if (!same) SetEvent(S.wake);
}
void ntl_tag_server_destroy(void) {
  if (!S.ready) return;
  InterlockedExchange(&S.stop, 1);
  SetEvent(S.wake);
  if (S.thread) { WaitForSingleObject(S.thread, 5000); CloseHandle(S.thread); S.thread = NULL; }
}
const char* ntl_tag_server_status(void) {
  static char copy[64];
  if (!S.ready) return "꺼짐";
  EnterCriticalSection(&S.lock); snprintf(copy, sizeof copy, "%s", S.status); LeaveCriticalSection(&S.lock);
  return copy;
}

// HTTPS to ntl-slither.com: GET (body NULL) or POST JSON; the reply text (at most cap-1 bytes).
static bool https(const wchar_t* method, const wchar_t* path, const char* body, char* reply, size_t cap) {
  bool ok = false; reply[0] = 0;
  HINTERNET session = WinHttpOpen(L"SlitherNative/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  HINTERNET connection = session ? WinHttpConnect(session, L"ntl-slither.com", 443, 0) : NULL;
  HINTERNET request = connection ? WinHttpOpenRequest(connection, method, path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : NULL;
  if (session) WinHttpSetTimeouts(session, 4000, 4000, 4000, 4000);
  if (request) {
    const wchar_t* headers = body ? L"Content-Type: application/json\r\n" : WINHTTP_NO_ADDITIONAL_HEADERS;
    DWORD length = body ? (DWORD)strlen(body) : 0;
    if (WinHttpSendRequest(request, headers, body ? (DWORD)-1L : 0, (void*)body, length, length, 0) && WinHttpReceiveResponse(request, NULL)) {
      DWORD status = 0, size = sizeof status;
      WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL);
      size_t total = 0; DWORD got = 0;
      while (total + 1 < cap && WinHttpReadData(request, reply + total, (DWORD)(cap - 1 - total), &got) && got) total += got;
      reply[total] = 0;
      ok = status == 200;
    }
  }
  if (request) WinHttpCloseHandle(request);
  if (connection) WinHttpCloseHandle(connection);
  if (session) WinHttpCloseHandle(session);
  return ok;
}
static DWORD WINAPI check_worker(void* unused) {
  (void)unused;
  wchar_t path[128]; char reply[256];
  swprintf(path, 128, L"/tags/tag?id=%d&pass=%hs", S.check_id, S.check_pass);
  bool ok = https(L"GET", path, NULL, reply, sizeof reply);
  for (char* p = reply; *p; p++) if ((unsigned char)*p < 32) *p = ' ';
  bool accepted = ok && !strcmp(reply, "ok");
  snprintf(S.check_msg, sizeof S.check_msg, "%s", ok ? reply : "NTL 태그 서버에 연결하지 못했습니다");
  InterlockedExchange(&S.check_state, accepted ? 2 : 3);
  return 0;
}
void ntl_tag_password_check(int id, const char* password) {
  start();
  if (InterlockedCompareExchange(&S.check_state, 1, 1) == 1) return;
  if (S.check_thread) { WaitForSingleObject(S.check_thread, 0); CloseHandle(S.check_thread); S.check_thread = NULL; }
  S.check_id = id; ntl_tag_md5_hex(password, S.check_pass);
  InterlockedExchange(&S.check_state, 1);
  S.check_thread = CreateThread(NULL, 0, check_worker, NULL, 0, NULL);
  if (!S.check_thread) { snprintf(S.check_msg, sizeof S.check_msg, "확인을 시작하지 못했습니다"); InterlockedExchange(&S.check_state, 3); }
}
int ntl_tag_password_result(char* msg, size_t cap) {
  LONG state = InterlockedCompareExchange(&S.check_state, 0, 0);
  if (state == 2 || state == 3) {
    if (msg) snprintf(msg, cap, "%s", S.check_msg);
    if (state == 2) { ntl_tag_auth_set(S.check_id, S.check_pass); ntl_tag_settings_save(NTL_TAG_SETTINGS); }
    InterlockedExchange(&S.check_state, 0);
    return state;
  }
  return state;
}
static DWORD WINAPI recheck_worker(void* unused) {
  (void)unused;
  char body[4096] = "[", reply[1024];
  size_t n = 1;
  for (int i = 0; i < S.recheck_count; i++) {
    const char* pass = ntl_tag_auth_for(S.recheck_ids[i]);
    n += snprintf(body + n, sizeof body - n, "%s[%d,\"%s\"]", i ? "," : "", S.recheck_ids[i], pass ? pass : "");
  }
  snprintf(body + n, sizeof body - n, "]");
  if (!https(L"POST", L"/tags/authorizetags.php", body, reply, sizeof reply)) return 0;
  // [1,0,1...]: one per tag asked
  int count = 0; const char* p = reply;
  while (*p && *p != '[') p++;
  while (*p && count < S.recheck_count) {
    if (*p == '0' || *p == '1') S.recheck_ok[count++] = *p == '1';
    p++;
  }
  if (count == S.recheck_count) InterlockedExchange(&S.recheck_ready, 1);
  return 0;
}
void ntl_tag_passwords_recheck(void) {
  start();
  S.recheck_count = 0;
  for (int i = 0; i < ntl_tag_prefs.auth_count && i < 32; i++) S.recheck_ids[S.recheck_count++] = ntl_tag_prefs.auth[i].id;
  if (!S.recheck_count) return;
  HANDLE t = CreateThread(NULL, 0, recheck_worker, NULL, 0, NULL);
  if (t) CloseHandle(t);
}

#else
// macOS: no NTL tag server (free tags still show: they come with the skin codes).
void ntl_tag_server_tick(tenv* env) { (void)env; }
void ntl_tag_server_destroy(void) {}
const char* ntl_tag_server_status(void) { return "macOS에서는 지원하지 않음"; }
void ntl_tag_password_check(int id, const char* password) { (void)id; (void)password; }
int ntl_tag_password_result(char* msg, size_t capacity) { if (msg && capacity) snprintf(msg, capacity, "macOS에서는 비밀번호 태그를 확인할 수 없습니다"); return 0; }
void ntl_tag_passwords_recheck(void) {}
#endif
