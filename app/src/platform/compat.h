#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H
// The Windows calls this client uses, for macOS (and Linux, for testing the port).
// On Windows this is just <windows.h>; elsewhere it provides the same names on top of POSIX:
// clocks, files, threads, locks, events and atomics. Paths may use '\' as on Windows.
#ifdef _WIN32
#include <windows.h>
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <time.h>
#include <pthread.h>

typedef unsigned long DWORD;                     // as on Windows, so "%lu" matches
typedef int32_t LONG;
typedef unsigned long long ULONGLONG;
typedef int BOOL;
typedef void* HANDLE;
typedef union { struct { uint32_t LowPart; int32_t HighPart; }; int64_t QuadPart; } LARGE_INTEGER;
typedef struct { uint16_t wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds; } SYSTEMTIME;
typedef struct { int64_t unix_seconds; } FILETIME;
typedef struct { DWORD dwFileAttributes; FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime; DWORD nFileSizeHigh, nFileSizeLow; } WIN32_FILE_ATTRIBUTE_DATA;
typedef struct { char cFileName[260]; } WIN32_FIND_DATAA;
typedef struct { pthread_mutex_t mutex; } CRITICAL_SECTION;
typedef DWORD (*LPTHREAD_START_ROUTINE)(void*);

#define WINAPI
#define TRUE 1
#define FALSE 0
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#define INFINITE 0xFFFFFFFFu
#define WAIT_OBJECT_0 0u
#define WAIT_TIMEOUT 258u
#define WAIT_FAILED 0xFFFFFFFFu
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define MOVEFILE_REPLACE_EXISTING 1u
#define MOVEFILE_WRITE_THROUGH 8u
#define GetFileExInfoStandard 0
#define ERROR_INVALID_DATA 13u
#define ERROR_NOT_ENOUGH_MEMORY 8u
#define ERROR_TIMEOUT 1460u
#define ERROR_INSUFFICIENT_BUFFER 122u
#define ERROR_NOT_READY 21u
#define CP_UTF8 65001u
#define MB_ERR_INVALID_CHARS 8u
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

ULONGLONG GetTickCount64(void);
BOOL QueryPerformanceCounter(LARGE_INTEGER* out);
BOOL QueryPerformanceFrequency(LARGE_INTEGER* out);
void Sleep(DWORD ms);
BOOL SwitchToThread(void);
void GetLocalTime(SYSTEMTIME* out);
DWORD GetLastError(void);

BOOL MoveFileExA(const char* from, const char* to, DWORD flags);
BOOL DeleteFileA(const char* path);
BOOL CreateDirectoryA(const char* path, void* security);
BOOL GetFileAttributesExA(const char* path, int level, WIN32_FILE_ATTRIBUTE_DATA* out);
BOOL FileTimeToSystemTime(const FILETIME* in, SYSTEMTIME* out);   // UTC
BOOL SystemTimeToTzSpecificLocalTime(const void* zone, const SYSTEMTIME* utc, SYSTEMTIME* local);
HANDLE FindFirstFileA(const char* pattern, WIN32_FIND_DATAA* out);
BOOL FindNextFileA(HANDLE search, WIN32_FIND_DATAA* out);
BOOL FindClose(HANDLE search);

void InitializeCriticalSection(CRITICAL_SECTION* c);
void DeleteCriticalSection(CRITICAL_SECTION* c);
void EnterCriticalSection(CRITICAL_SECTION* c);
void LeaveCriticalSection(CRITICAL_SECTION* c);
BOOL TryEnterCriticalSection(CRITICAL_SECTION* c);

// UTF-8 <-> wchar_t (UTF-32 here). Same contract as Windows: counts include the 0 when len is -1.
int MultiByteToWideChar(unsigned codepage, DWORD flags, const char* in, int len, wchar_t* out, int cap);
int WideCharToMultiByte(unsigned codepage, DWORD flags, const wchar_t* in, int len, char* out, int cap, const char* def, BOOL* used);
#include <stdio.h>
FILE* _wfopen(const wchar_t* path, const wchar_t* mode);

HANDLE CreateEventW(void* security, BOOL manual_reset, BOOL initial, const wchar_t* name);
BOOL SetEvent(HANDLE event);
BOOL ResetEvent(HANDLE event);
HANDLE CreateThread(void* security, size_t stack, LPTHREAD_START_ROUTINE start, void* arg, DWORD flags, DWORD* id);
DWORD WaitForSingleObject(HANDLE handle, DWORD ms);                     // events and threads
DWORD WaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL all, DWORD ms);  // events, any
BOOL CloseHandle(HANDLE handle);

static inline LONG InterlockedCompareExchange(volatile LONG* target, LONG value, LONG expected) {
  LONG old = expected;
  __atomic_compare_exchange_n(target, &old, value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
  return old;
}
static inline LONG InterlockedExchange(volatile LONG* target, LONG value) { return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST); }
static inline LONG InterlockedIncrement(volatile LONG* target) { return __atomic_add_fetch(target, 1, __ATOMIC_SEQ_CST); }
static inline LONG InterlockedDecrement(volatile LONG* target) { return __atomic_sub_fetch(target, 1, __ATOMIC_SEQ_CST); }
static inline void SecureZeroMemory(void* p, size_t n) { volatile unsigned char* b = p; while (n--) *b++ = 0; }
#endif
#endif
