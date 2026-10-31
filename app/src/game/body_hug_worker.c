#include "body_hug_worker.h"
#include "../platform/compat.h"
#include <GLFW/glfw3.h>
#include <string.h>

// A job: the input with its own copies of the arrays it points to.
typedef struct {
  hug_input in; hug_state st; int id;
  hug_point body[HUG_MAX_POINTS];
  double others[HUG_OTHER * HUG_MAX_OTHERS];
  double pre[64];
} hug_job;

static hug_job waiting, working;        // handed in / being planned (the worker's own)
static hug_output answer; static hug_state answer_st; static int answer_id;
static bool has_waiting, has_answer, running;
static CRITICAL_SECTION lock;
static HANDLE wake;
static volatile LONG started;

static void job_copy(hug_job* to, const hug_input* in, const hug_state* st, int id) {
  int n = in->count > HUG_MAX_POINTS ? HUG_MAX_POINTS : in->count;
  int m = in->others_count > HUG_MAX_OTHERS ? HUG_MAX_OTHERS : in->others_count;
  int p = in->pre_count > 64 ? 64 : in->pre_count;
  to->in = *in; to->st = *st; to->id = id;
  memcpy(to->body, in->body, sizeof(hug_point) * n);
  if (m > 0) memcpy(to->others, in->others, sizeof(double) * HUG_OTHER * m);
  if (p > 0) memcpy(to->pre, in->pre_ang, sizeof(double) * p);
  to->in.body = to->body; to->in.count = n;
  to->in.others = to->others; to->in.others_count = m;
  to->in.pre_ang = to->pre; to->in.pre_count = p;
}

static DWORD WINAPI hug_thread(void* arg) {
  (void)arg;
  for (;;) {
    WaitForSingleObject(wake, INFINITE);
    for (;;) {
      EnterCriticalSection(&lock);
      if (!has_waiting) { running = false; LeaveCriticalSection(&lock); break; }
      working = waiting;                  // (the arrays' pointers are fixed up below)
      has_waiting = false; running = true;
      LeaveCriticalSection(&lock);
      working.in.body = working.body; working.in.others = working.others; working.in.pre_ang = working.pre;
      hug_output out; hug_state st = working.st;
      double t0 = glfwGetTime();               // (callable from any thread)
      bool ok = body_hug_steer(&working.in, &st, &out);
      out.ms = (glfwGetTime() - t0) * 1000;
      EnterCriticalSection(&lock);
      if (ok) { answer = out; answer_st = st; answer_id = working.id; has_answer = true; }
      LeaveCriticalSection(&lock);
    }
  }
  return 0;
}

static void start(void) {
  if (InterlockedCompareExchange(&started, 1, 0) != 0) return;
  InitializeCriticalSection(&lock);
  wake = CreateEventW(NULL, FALSE, FALSE, NULL);
  HANDLE t = CreateThread(NULL, 0, hug_thread, NULL, 0, NULL);
  if (t) CloseHandle(t);
}

void hug_worker_submit(const hug_input* in, const hug_state* st, int id) {
  start();
  EnterCriticalSection(&lock);
  job_copy(&waiting, in, st, id);
  has_waiting = true;
  LeaveCriticalSection(&lock);
  SetEvent(wake);
}

bool hug_worker_take(hug_output* out, hug_state* st, int* id) {
  if (!started) return false;
  EnterCriticalSection(&lock);
  bool got = has_answer;
  if (got) { *out = answer; *st = answer_st; *id = answer_id; has_answer = false; }
  LeaveCriticalSection(&lock);
  return got;
}

bool hug_worker_busy(void) {
  if (!started) return false;
  EnterCriticalSection(&lock);
  bool busy = has_waiting || running;
  LeaveCriticalSection(&lock);
  return busy;
}
