#include <assert.h>
#include <stdio.h>
#include "../thermite/src/core/tenv.h"

static tenv env;
static twindow window;
static tcontext context;
static int gpu_resizes, app_resizes, polls, waits;
static int framebuffer_width = 1280, framebuffer_height = 720;
extern void window_resize_callback(GLFWwindow*, int, int);

// Unused platform entry points must not be reached by this headless test.
int glfwInit(void) { assert(0); return 0; }
void glfwTerminate(void) { assert(0); }
void glfwWindowHint(int a, int b) { assert(0); }
GLFWmonitor* glfwGetPrimaryMonitor(void) { assert(0); return NULL; }
const GLFWvidmode* glfwGetVideoMode(GLFWmonitor* m) { assert(0); return NULL; }
GLFWwindow* glfwCreateWindow(int a,int b,const char* c,GLFWmonitor* d,GLFWwindow* e) { assert(0); return NULL; }
void glfwSetWindowUserPointer(GLFWwindow* w,void* p) { assert(0); }
GLFWframebuffersizefun glfwSetFramebufferSizeCallback(GLFWwindow* w,GLFWframebuffersizefun f) { assert(0); return NULL; }
void glfwRestoreWindow(GLFWwindow* w) { assert(0); }
void glfwSetWindowMonitor(GLFWwindow* w,GLFWmonitor* m,int x,int y,int a,int b,int r) { assert(0); }
void glfwGetMonitorPos(GLFWmonitor* m,int* x,int* y) { assert(0); }
void glfwSetWindowAttrib(GLFWwindow* w,int a,int v) { assert(0); }
void glfwGetWindowPos(GLFWwindow* w,int* x,int* y) { assert(0); }
void glfwGetWindowSize(GLFWwindow* w,int* x,int* y) { assert(0); }
int glfwGetKey(GLFWwindow* w,int k) { assert(0); return 0; }
int glfwGetMouseButton(GLFWwindow* w,int b) { assert(0); return 0; }
int glfwWindowShouldClose(GLFWwindow* w) { assert(0); return 0; }

void* glfwGetWindowUserPointer(GLFWwindow* handle) { return &env; }
void glfwPollEvents(void) { polls++; }
void glfwWaitEvents(void) { waits++; }
void glfwGetFramebufferSize(GLFWwindow* handle, int* w, int* h) {
  *w = framebuffer_width; *h = framebuffer_height;
}
void tcontext_resize(tcontext* ctx, const ivec2 size, bool vsync) {
  assert(size[0] > 0 && size[1] > 0);
  gpu_resizes++; ctx->swapchain_ok = true;
}
static void resize(tenv* e) { app_resizes++; }
static void render(tenv* e) { assert(!"resize callbacks must never render"); }

int main(void) {
  env.wnd = &window; env.ctx = &context;
  window.env = &env; window._resize_func = resize; window._render_func = render;
  context.swapchain_ok = true;
  window_resize_callback(NULL, 1280, 720);
  window_resize_callback(NULL, 1920, 1080);
  assert(gpu_resizes == 0 && window._refresh);
  twindow_poll_input(&window);
  assert(gpu_resizes == 1 && app_resizes == 1 && !window._refresh);
  assert(window.size[0] == 1280 && window.size[1] == 720);
  twindow_poll_input(&window);
  assert(gpu_resizes == 1);
  context.swapchain_ok = false;
  twindow_poll_input(&window); // Recover without waiting for another size event.
  assert(gpu_resizes == 2 && context.swapchain_ok);
  window_resize_callback(NULL, 0, 0);
  twindow_wait_input(&window);
  assert(gpu_resizes == 2 && window._refresh);
  window_resize_callback(NULL, 1280, 720);
  twindow_poll_input(&window);
  assert(gpu_resizes == 3 && app_resizes == 3 && polls == 4 && waits == 1);
  puts("window refresh tests passed");
  return 0;
}
