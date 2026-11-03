#ifndef HTTP_SIMPLE_H
#define HTTP_SIMPLE_H
// One blocking HTTP(S) request (for worker threads): WinHTTP on Windows, libcurl elsewhere.
#include <stddef.h>
// url: "http(s)://host[:port]/path[?query]". body may be NULL. *out (malloc'd, 0-terminated, may
// be NULL) gets at most max_out bytes of the reply. Returns the HTTP status, 0 when it failed.
int http_request(const char* method, const char* url, const char* content_type, const void* body, size_t body_len,
                 unsigned char** out, size_t* out_len, size_t max_out, int timeout_ms);
#endif
