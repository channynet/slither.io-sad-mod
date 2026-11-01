#include <assert.h>
#include <stdio.h>
#include "../thermite/src/core/tentry.h"

static twindow window;
static tcontext context;
static tkeyboard keyboard;
static tmouse mouse;
static int initialized, finished, destroyed;
static bool fullscreen, fail_window;

void tlaunch(tenv* e) {
  assert(!e->wnd && !e->ctx && !e->kb && !e->ms);
  e->config.fullscreen = fullscreen;
}
twindow* twindow_create(tenv* e, trender_func r, tresize_func s) {
  assert(!e->wnd && !e->ctx);
  if (fail_window) return NULL;
  window.env = e;
  return &window;
}
tkeyboard* tkeyboard_create(twindow* w) { return &keyboard; }
tmouse* tmouse_create(twindow* w) { return &mouse; }
tcontext* tcontext_create(twindow* w, bool v, int f) { return &context; }
void tinit(tenv* e) {
  assert(e->wnd == &window && e->ctx == &context);
  assert(e->kb == &keyboard && e->ms == &mouse && !finished);
  initialized++;
  e->config.running = false;
}
void twindow_finish_init(twindow* w) {
  assert(initialized == 1 && w->env->wnd == w && w->env->ctx == &context);
  assert(w->env->config.fullscreen == fullscreen);
  finished++;
}
void tcontext_wait_idle(tcontext* c) { assert(finished == 1); }
void tdestroy(tenv* e) { destroyed++; }
void tcontext_destroy(tcontext* c) {}
void tmouse_destroy(tmouse* m) {}
void tkeyboard_destroy(tkeyboard* k) {}
void twindow_destroy(twindow* w) {}
bool twindow_visible(twindow* w) { assert(0); return false; }
void twindow_poll_input(twindow* w) { assert(0); }
void glfwWaitEventsTimeout(double t) { assert(0); }
void tbackground(tenv* e) { assert(0); }
void tinput(tenv* e) { assert(0); }
void trender(tenv* e) { assert(0); }
void tresize(tenv* e) { assert(0); }
void tidle(tenv* e) { assert(0); }
void tkeyboard_update(tkeyboard* k) { assert(0); }
void tmouse_update(tmouse* m) { assert(0); }

int main(void) {
  for (int mode = 0; mode < 2; mode++) {
    fullscreen = mode != 0;
    initialized = finished = destroyed = 0;
    assert(tentry(0, NULL, 16) == 0);
    assert(initialized == 1 && finished == 1 && destroyed == 1);
  }
  fail_window = true;
  initialized = finished = destroyed = 0;
  assert(tentry(0, NULL, 16) == 1);
  assert(!initialized && !finished && !destroyed);
  puts("Window startup: initialization precedes fullscreen; failed creation is handled");
}
