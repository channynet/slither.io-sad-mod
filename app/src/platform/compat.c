// POSIX versions of the Windows calls in compat.h (nothing here is built on Windows).
#ifndef _WIN32
#define _GNU_SOURCE
#include "compat.h"
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <fnmatch.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

static void posix_path(const char* in, char* out, size_t cap) {
  size_t i = 0;
  for (; in[i] && i + 1 < cap; i++) out[i] = in[i] == '\\' ? '/' : in[i];
  out[i] = 0;
}

ULONGLONG GetTickCount64(void) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return (ULONGLONG)t.tv_sec * 1000u + (ULONGLONG)(t.tv_nsec / 1000000);
}
BOOL QueryPerformanceCounter(LARGE_INTEGER* out) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  out->QuadPart = (int64_t)t.tv_sec * 1000000000LL + t.tv_nsec;
  return TRUE;
}
BOOL QueryPerformanceFrequency(LARGE_INTEGER* out) { out->QuadPart = 1000000000LL; return TRUE; }
void Sleep(DWORD ms) {
  struct timespec t = {ms / 1000, (long)(ms % 1000) * 1000000L};
  while (nanosleep(&t, &t) && errno == EINTR) {}
}
BOOL SwitchToThread(void) { sched_yield(); return TRUE; }
static void fill(SYSTEMTIME* out, const struct tm* tm, int ms) {
  out->wYear = tm->tm_year + 1900; out->wMonth = tm->tm_mon + 1; out->wDayOfWeek = tm->tm_wday;
  out->wDay = tm->tm_mday; out->wHour = tm->tm_hour; out->wMinute = tm->tm_min; out->wSecond = tm->tm_sec;
  out->wMilliseconds = ms;
}
void GetLocalTime(SYSTEMTIME* out) {
  struct timeval tv; gettimeofday(&tv, NULL);
  time_t s = tv.tv_sec; struct tm tm; localtime_r(&s, &tm);
  fill(out, &tm, (int)(tv.tv_usec / 1000));
}
DWORD GetLastError(void) { return (DWORD)errno; }

int MultiByteToWideChar(unsigned codepage, DWORD flags, const char* in, int len, wchar_t* out, int cap) {
  (void)codepage;
  const unsigned char* s = (const unsigned char*)in;
  size_t n = len < 0 ? strlen(in) + 1 : (size_t)len;
  int w = 0;
  for (size_t i = 0; i < n;) {
    uint32_t c = s[i], need = 0;
    if (c < 0x80) need = 0;
    else if ((c & 0xE0) == 0xC0) { need = 1; c &= 0x1F; }
    else if ((c & 0xF0) == 0xE0) { need = 2; c &= 0x0F; }
    else if ((c & 0xF8) == 0xF0) { need = 3; c &= 0x07; }
    else { if (flags & MB_ERR_INVALID_CHARS) return 0; c = 0xFFFD; }
    bool bad = false;
    uint32_t used = 0;                           // continuation bytes actually consumed
    for (uint32_t k = 1; k <= need; k++) {
      if (i + k >= n || (s[i + k] & 0xC0) != 0x80) { bad = true; break; }   // never eat the end
      c = (c << 6) | (s[i + k] & 0x3F); used = k;
    }
    if (bad) { if (flags & MB_ERR_INVALID_CHARS) return 0; c = 0xFFFD; need = used; }
    i += need + 1;
    if (cap) { if (w >= cap) { errno = ERANGE; return 0; } out[w] = (wchar_t)c; }
    w++;
  }
  return w;
}
int WideCharToMultiByte(unsigned codepage, DWORD flags, const wchar_t* in, int len, char* out, int cap, const char* def, BOOL* used) {
  (void)codepage; (void)flags; (void)def; if (used) *used = FALSE;
  size_t n = len < 0 ? wcslen(in) + 1 : (size_t)len;
  int w = 0;
  for (size_t i = 0; i < n; i++) {
    uint32_t c = (uint32_t)in[i]; unsigned char b[4]; int k;
    if (c < 0x80) { b[0] = (unsigned char)c; k = 1; }
    else if (c < 0x800) { b[0] = 0xC0 | (c >> 6); b[1] = 0x80 | (c & 0x3F); k = 2; }
    else if (c < 0x10000) { b[0] = 0xE0 | (c >> 12); b[1] = 0x80 | ((c >> 6) & 0x3F); b[2] = 0x80 | (c & 0x3F); k = 3; }
    else if (c < 0x110000) { b[0] = 0xF0 | (c >> 18); b[1] = 0x80 | ((c >> 12) & 0x3F); b[2] = 0x80 | ((c >> 6) & 0x3F); b[3] = 0x80 | (c & 0x3F); k = 4; }
    else { b[0] = '?'; k = 1; }
    if (cap) { if (w + k > cap) { errno = ERANGE; return 0; } memcpy(out + w, b, (size_t)k); }
    w += k;
  }
  return w;
}
FILE* _wfopen(const wchar_t* path, const wchar_t* mode) {
  char p[4096], m[16];
  if (!WideCharToMultiByte(CP_UTF8, 0, path, -1, p, sizeof(p), NULL, NULL)) return NULL;
  if (!WideCharToMultiByte(CP_UTF8, 0, mode, -1, m, sizeof(m), NULL, NULL)) return NULL;
  char q[4096]; posix_path(p, q, sizeof(q));
  return fopen(q, m);
}

BOOL MoveFileExA(const char* from, const char* to, DWORD flags) {
  char a[1024], b[1024]; posix_path(from, a, sizeof(a)); posix_path(to, b, sizeof(b));
  if (!(flags & MOVEFILE_REPLACE_EXISTING) && access(b, F_OK) == 0) return FALSE;
  if (flags & MOVEFILE_WRITE_THROUGH) {         // the new content reaches the disk before the swap
    int fd = open(a, O_RDONLY);
    if (fd >= 0) { fsync(fd); close(fd); }
  }
  return rename(a, b) == 0;
}
BOOL DeleteFileA(const char* path) { char p[1024]; posix_path(path, p, sizeof(p)); return unlink(p) == 0; }
BOOL CreateDirectoryA(const char* path, void* security) {
  (void)security; char p[1024]; posix_path(path, p, sizeof(p)); return mkdir(p, 0755) == 0;
}
BOOL GetFileAttributesExA(const char* path, int level, WIN32_FILE_ATTRIBUTE_DATA* out) {
  (void)level; char p[1024]; posix_path(path, p, sizeof(p));
  struct stat st; if (stat(p, &st)) return FALSE;
  memset(out, 0, sizeof(*out));
  out->ftLastWriteTime.unix_seconds = st.st_mtime;
  out->nFileSizeLow = (DWORD)st.st_size; out->nFileSizeHigh = (DWORD)((uint64_t)st.st_size >> 32);
  return TRUE;
}
BOOL FileTimeToSystemTime(const FILETIME* in, SYSTEMTIME* out) {
  time_t s = (time_t)in->unix_seconds; struct tm tm; if (!gmtime_r(&s, &tm)) return FALSE;
  fill(out, &tm, 0); return TRUE;
}
BOOL SystemTimeToTzSpecificLocalTime(const void* zone, const SYSTEMTIME* utc, SYSTEMTIME* local) {
  (void)zone;
  struct tm tm = {.tm_year = utc->wYear - 1900, .tm_mon = utc->wMonth - 1, .tm_mday = utc->wDay,
                  .tm_hour = utc->wHour, .tm_min = utc->wMinute, .tm_sec = utc->wSecond};
  time_t s = timegm(&tm); struct tm lt; if (!localtime_r(&s, &lt)) return FALSE;
  fill(local, &lt, utc->wMilliseconds); return TRUE;
}

typedef struct { DIR* dir; char pattern[256]; } find_state;
static bool find_next(find_state* f, WIN32_FIND_DATAA* out) {
  struct dirent* e;
  while ((e = readdir(f->dir))) {
    if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
    if (fnmatch(f->pattern, e->d_name, 0) == 0) { snprintf(out->cFileName, sizeof(out->cFileName), "%s", e->d_name); return true; }
  }
  return false;
}
HANDLE FindFirstFileA(const char* pattern, WIN32_FIND_DATAA* out) {
  char p[1024]; posix_path(pattern, p, sizeof(p));
  char* slash = strrchr(p, '/');
  find_state* f = calloc(1, sizeof(*f)); if (!f) return INVALID_HANDLE_VALUE;
  snprintf(f->pattern, sizeof(f->pattern), "%s", slash ? slash + 1 : p);
  if (slash) *slash = 0;
  f->dir = opendir(slash ? p : ".");
  if (!f->dir || !find_next(f, out)) { if (f->dir) closedir(f->dir); free(f); return INVALID_HANDLE_VALUE; }
  return f;
}
BOOL FindNextFileA(HANDLE search, WIN32_FIND_DATAA* out) { return find_next(search, out); }
BOOL FindClose(HANDLE search) {
  if (!search || search == INVALID_HANDLE_VALUE) return FALSE;
  find_state* f = search; closedir(f->dir); free(f); return TRUE;
}

void InitializeCriticalSection(CRITICAL_SECTION* c) {
  pthread_mutexattr_t a; pthread_mutexattr_init(&a);
  pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);        // Windows sections are re-entrant
  pthread_mutex_init(&c->mutex, &a); pthread_mutexattr_destroy(&a);
}
void DeleteCriticalSection(CRITICAL_SECTION* c) { pthread_mutex_destroy(&c->mutex); }
void EnterCriticalSection(CRITICAL_SECTION* c) { pthread_mutex_lock(&c->mutex); }
void LeaveCriticalSection(CRITICAL_SECTION* c) { pthread_mutex_unlock(&c->mutex); }
BOOL TryEnterCriticalSection(CRITICAL_SECTION* c) { return pthread_mutex_trylock(&c->mutex) == 0; }

// One object for events and threads: a thread's handle becomes signalled when it ends.
typedef struct {
  pthread_mutex_t mutex; pthread_cond_t cond;
  bool signalled, manual, is_thread, joined, detached;
  pthread_t thread; LPTHREAD_START_ROUTINE start; void* arg;
} waitable;
// Any object becoming signalled wakes WaitForMultipleObjects (no polling, which kept a
// waiting thread waking 500 times a second).
static pthread_mutex_t any_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t any_cond = PTHREAD_COND_INITIALIZER;
static void notify_any(void) {
  pthread_mutex_lock(&any_mutex); pthread_cond_broadcast(&any_cond); pthread_mutex_unlock(&any_mutex);
}
static waitable* waitable_new(bool manual, bool initial) {
  waitable* w = calloc(1, sizeof(*w)); if (!w) return NULL;
  pthread_mutex_init(&w->mutex, NULL); pthread_cond_init(&w->cond, NULL);
  w->manual = manual; w->signalled = initial;
  return w;
}
HANDLE CreateEventW(void* security, BOOL manual_reset, BOOL initial, const wchar_t* name) {
  (void)security; (void)name; return waitable_new(manual_reset, initial);
}
BOOL SetEvent(HANDLE h) {
  waitable* w = h; if (!w) return FALSE;
  pthread_mutex_lock(&w->mutex); w->signalled = true; pthread_cond_broadcast(&w->cond); pthread_mutex_unlock(&w->mutex);
  notify_any();
  return TRUE;
}
BOOL ResetEvent(HANDLE h) {
  waitable* w = h; if (!w) return FALSE;
  pthread_mutex_lock(&w->mutex); w->signalled = false; pthread_mutex_unlock(&w->mutex); return TRUE;
}
static void waitable_free(waitable* w) {
  pthread_cond_destroy(&w->cond); pthread_mutex_destroy(&w->mutex); free(w);
}
static void* thread_main(void* p) {
  waitable* w = p; w->start(w->arg);
  pthread_mutex_lock(&w->mutex);
  w->signalled = true; pthread_cond_broadcast(&w->cond);
  bool orphan = w->detached;                   // its handle was closed while it ran: free it here
  pthread_mutex_unlock(&w->mutex);
  notify_any();
  if (orphan) waitable_free(w);
  return NULL;
}
HANDLE CreateThread(void* security, size_t stack, LPTHREAD_START_ROUTINE start, void* arg, DWORD flags, DWORD* id) {
  (void)security; (void)stack; (void)flags;
  waitable* w = waitable_new(true, false); if (!w) return NULL;
  w->is_thread = true; w->start = start; w->arg = arg;
  // macOS gives new threads only 512 KB of stack (Windows: 1 MB); give them room.
  pthread_attr_t attr; pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, stack > (size_t)8 << 20 ? stack : (size_t)8 << 20);
  int failed = pthread_create(&w->thread, &attr, thread_main, w);
  pthread_attr_destroy(&attr);
  if (failed) { free(w); return NULL; }
  if (id) *id = 1;
  return w;
}
static bool deadline_of(DWORD ms, struct timespec* at) {
  if (ms == INFINITE) return false;
  clock_gettime(CLOCK_REALTIME, at);
  at->tv_sec += ms / 1000; at->tv_nsec += (long)(ms % 1000) * 1000000L;
  if (at->tv_nsec >= 1000000000L) { at->tv_sec++; at->tv_nsec -= 1000000000L; }
  return true;
}
DWORD WaitForSingleObject(HANDLE h, DWORD ms) {
  waitable* w = h; if (!w) return WAIT_FAILED;
  struct timespec at; bool timed = deadline_of(ms, &at);
  pthread_mutex_lock(&w->mutex);
  while (!w->signalled) {
    if (!timed) pthread_cond_wait(&w->cond, &w->mutex);
    else if (pthread_cond_timedwait(&w->cond, &w->mutex, &at) == ETIMEDOUT) break;
  }
  bool got = w->signalled;
  if (got && !w->manual) w->signalled = false;
  pthread_mutex_unlock(&w->mutex);
  return got ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
}
DWORD WaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL all, DWORD ms) {
  (void)all;                                   // only "any" is used
  struct timespec at; bool timed = deadline_of(ms, &at);
  pthread_mutex_lock(&any_mutex);
  for (;;) {
    // Signalling takes the object's lock and then any_mutex (never both at once), so checking
    // under any_mutex and then waiting on any_cond cannot miss a wake-up.
    for (DWORD i = 0; i < count; i++)
      if (WaitForSingleObject(handles[i], 0) == WAIT_OBJECT_0) { pthread_mutex_unlock(&any_mutex); return WAIT_OBJECT_0 + i; }
    if (!timed) pthread_cond_wait(&any_cond, &any_mutex);
    else if (pthread_cond_timedwait(&any_cond, &any_mutex, &at) == ETIMEDOUT) {
      for (DWORD i = 0; i < count; i++)
        if (WaitForSingleObject(handles[i], 0) == WAIT_OBJECT_0) { pthread_mutex_unlock(&any_mutex); return WAIT_OBJECT_0 + i; }
      pthread_mutex_unlock(&any_mutex);
      return WAIT_TIMEOUT;
    }
  }
}
BOOL CloseHandle(HANDLE h) {
  waitable* w = h; if (!w) return FALSE;
  if (w->is_thread) {
    pthread_mutex_lock(&w->mutex);
    bool done = w->signalled;
    if (!done) w->detached = true;             // still running: the thread frees it when it ends
    pthread_mutex_unlock(&w->mutex);
    if (!done) { pthread_detach(w->thread); return TRUE; }
    pthread_join(w->thread, NULL);
  }
  waitable_free(w);
  return TRUE;
}
#endif
