#include "http_simple.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { unsigned char* data; size_t length, cap, max; } reply_buffer;
static int append(reply_buffer* b, const void* bytes, size_t n) {
  if (b->length + n > b->max) n = b->max > b->length ? b->max - b->length : 0;
  if (b->length + n + 1 > b->cap) {
    size_t cap = b->cap ? b->cap * 2 : 4096;
    while (cap < b->length + n + 1) cap *= 2;
    unsigned char* grown = realloc(b->data, cap);
    if (!grown) return 0;
    b->data = grown; b->cap = cap;
  }
  memcpy(b->data + b->length, bytes, n); b->length += n; b->data[b->length] = 0;
  return 1;
}

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
int http_request_referer(const char* method, const char* url, const char* referer, const char* content_type, const void* body, size_t body_len,
                 unsigned char** out, size_t* out_len, size_t max_out, int timeout_ms) {
  if (out) *out = NULL;
  if (out_len) *out_len = 0;
  wchar_t wurl[1024], wmethod[16], host[256], path[1024], headers[160] = L"", wreferer[1024];
  if (referer && !MultiByteToWideChar(CP_UTF8, 0, referer, -1, wreferer, 1024)) return 0;
  if (!MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 1024) || !MultiByteToWideChar(CP_UTF8, 0, method, -1, wmethod, 16)) return 0;
  URL_COMPONENTS parts = {sizeof parts};
  parts.lpszHostName = host; parts.dwHostNameLength = 256;
  parts.lpszUrlPath = path; parts.dwUrlPathLength = 1024;
  wchar_t extra[1024]; parts.lpszExtraInfo = extra; parts.dwExtraInfoLength = 1024;
  if (!WinHttpCrackUrl(wurl, 0, 0, &parts)) return 0;
  if (wcslen(path) + wcslen(extra) < 1023) wcscat(path, extra);
  if (content_type) swprintf(headers, 160, L"Content-Type: %hs\r\n", content_type);
  int status = 0;
  HINTERNET session = WinHttpOpen(L"SlitherNative/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  HINTERNET connection = session ? WinHttpConnect(session, host, parts.nPort, 0) : NULL;
  HINTERNET request = connection ? WinHttpOpenRequest(connection, wmethod, path, NULL, referer ? wreferer : WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                      parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : NULL;
  if (session) WinHttpSetTimeouts(session, timeout_ms, timeout_ms, timeout_ms, timeout_ms);
  if (request && WinHttpSendRequest(request, headers[0] ? headers : WINHTTP_NO_ADDITIONAL_HEADERS, headers[0] ? (DWORD)-1L : 0,
                                    (void*)body, (DWORD)body_len, (DWORD)body_len, 0) && WinHttpReceiveResponse(request, NULL)) {
    DWORD code = 0, size = sizeof code;
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &code, &size, NULL);
    reply_buffer b = {NULL, 0, 0, max_out};
    char chunk[16384]; DWORD got = 0; int ok = 1;
    while (ok && b.length < max_out) {
      if (!WinHttpReadData(request, chunk, sizeof chunk, &got)) { ok = 0; break; }
      if (!got) break;
      ok = append(&b, chunk, got);
    }
    if (ok) {
      status = (int)code;
      if (out) *out = b.data; else free(b.data);
      if (out_len) *out_len = b.length;
    } else free(b.data);
  }
  if (request) WinHttpCloseHandle(request);
  if (connection) WinHttpCloseHandle(connection);
  if (session) WinHttpCloseHandle(session);
  return status;
}
#else
#include <curl/curl.h>
static size_t on_data(char* data, size_t size, size_t n, void* user) {
  reply_buffer* b = user;
  return append(b, data, size * n) ? size * n : 0;
}
int http_request_referer(const char* method, const char* url, const char* referer, const char* content_type, const void* body, size_t body_len,
                 unsigned char** out, size_t* out_len, size_t max_out, int timeout_ms) {
  if (out) *out = NULL;
  if (out_len) *out_len = 0;
  CURL* curl = curl_easy_init();
  if (!curl) return 0;
  reply_buffer b = {NULL, 0, 0, max_out};
  struct curl_slist* headers = NULL;
  char header[160];
  if (content_type) { snprintf(header, sizeof header, "Content-Type: %s", content_type); headers = curl_slist_append(headers, header); }
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
  curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)timeout_ms);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "SlitherNative/1.0");
  if (referer) curl_easy_setopt(curl, CURLOPT_REFERER, referer);
  if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  if (body) { curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body); curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body_len); }
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_data);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &b);
  int status = 0;
  if (curl_easy_perform(curl) == CURLE_OK) {
    long code = 0; curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    status = (int)code;
    if (out) { *out = b.data; b.data = NULL; }
    if (out_len) *out_len = b.length;
  }
  free(b.data);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return status;
}
#endif

int http_request(const char* method, const char* url, const char* content_type, const void* body, size_t body_len,
                 unsigned char** out, size_t* out_len, size_t max_out, int timeout_ms) {
  return http_request_referer(method, url, NULL, content_type, body, body_len, out, out_len, max_out, timeout_ms);
}
