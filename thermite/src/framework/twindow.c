#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "../core/tenv.h"

#ifdef __APPLE__
// macOS full screen is the system's own (its own Space, like the green window button): the menu
// bar and Dock get out of the way, and switching apps with Cmd+Tab never minimizes the game.
#include <objc/message.h>
#include <objc/runtime.h>
id glfwGetCocoaWindow(GLFWwindow* window);     // from glfw3native.h, whose Carbon headers are not needed here
int glfwGetCocoaWindowVisibleSN(GLFWwindow* window);   // added to our GLFW copy (cocoa_window.m)
static id mac_msg(id target, const char* selector) {
  return ((id (*)(id, SEL))objc_msgSend)(target, sel_registerName(selector));
}
// Keep macOS from napping the app while it is in the background: App Nap slows the timers and
// the game would stop answering the server (and be disconnected) while another app is in front.
static void mac_disable_app_nap(void) {
  static id activity;
  if (activity) return;
  id pool = mac_msg(mac_msg((id)objc_getClass("NSAutoreleasePool"), "alloc"), "init");
  id info = mac_msg((id)objc_getClass("NSProcessInfo"), "processInfo");
  id reason = ((id (*)(id, SEL, const char*))objc_msgSend)((id)objc_getClass("NSString"),
      sel_registerName("stringWithUTF8String:"), "Online game: keeps the server connection alive");
  // NSActivityUserInitiatedAllowingIdleSystemSleep | NSActivityLatencyCritical
  unsigned long long options = 0x00EFFFFFull | 0xFF00000000ull;
  if (info && reason)
    activity = ((id (*)(id, SEL, unsigned long long, id))objc_msgSend)(info,
        sel_registerName("beginActivityWithOptions:reason:"), options, reason);
  if (activity) mac_msg(activity, "retain");
  mac_msg(pool, "drain");
}
static bool mac_is_fullscreen(GLFWwindow* handle) {
  id ns = glfwGetCocoaWindow(handle);
  if (!ns) return false;
  unsigned long mask = ((unsigned long (*)(id, SEL))objc_msgSend)(ns, sel_registerName("styleMask"));
  return (mask & (1ul << 14)) != 0;           // NSWindowStyleMaskFullScreen
}
static void mac_set_fullscreen(GLFWwindow* handle, bool want) {
  id ns = glfwGetCocoaWindow(handle);
  if (ns && mac_is_fullscreen(handle) != want)
    ((void (*)(id, SEL, id))objc_msgSend)(ns, sel_registerName("toggleFullScreen:"), (id)0);
}
#endif

void twindow_request_refresh(twindow* twindow) {
  twindow->_refresh = true;
}

void window_resize_callback(GLFWwindow* window, int width, int height) {
  tenv* env = glfwGetWindowUserPointer(window);
  env->wnd->size[0] = width;
  env->wnd->size[1] = height;

  // GLFW can invoke this inside glfwSetWindowMonitor. Defer GPU work until
  // the monitor transition has returned; never render inside a callback.
  env->wnd->_refresh = true;
}

twindow* twindow_create(tenv* env, trender_func render_func,
                        tresize_func resize_func) {
  twindow* window = malloc(sizeof(twindow));
  window->_render_func = render_func;
  window->_resize_func = resize_func;
  window->_refresh = false;
  window->env = env;
#ifdef __APPLE__
  // MoltenVK is linked into the app; hand GLFW its entry point instead of a system Vulkan loader.
  glfwInitVulkanLoader(vkGetInstanceProcAddr);
  // Draw at the window's size in points; macOS scales it up on Retina screens. Mouse positions,
  // the UI and the game all use one coordinate space, and the GPU fills a quarter of the pixels.
  glfwInitHint(GLFW_COCOA_MENUBAR, GLFW_TRUE);
  // GLFW would otherwise change the working folder to the bundle's Resources, undoing the
  // data folder chosen at launch: settings and logs would be written into the app itself.
  glfwInitHint(GLFW_COCOA_CHDIR_RESOURCES, GLFW_FALSE);
#endif
  if (glfwInit() == GLFW_FALSE) {
    printf("Error initializing window\n");
    return NULL;
  }

  glfwWindowHint(GLFW_RESIZABLE,
                 env->config.resizable ? GLFW_TRUE : GLFW_FALSE);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#ifdef __APPLE__
  glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
#endif

  GLFWmonitor* primary = glfwGetPrimaryMonitor();
  const GLFWvidmode* vidmode = glfwGetVideoMode(primary);

  float padding = 0.8f;
  int mw = vidmode->width * padding;
  int mh = mw / env->config.aspect_ratio;
  if (mh > vidmode->height * padding) {
    mh = vidmode->height * padding;
    mw = mh * env->config.aspect_ratio;
  }
  int mx = (vidmode->width - mw) / 2;
  int my = (vidmode->height - mh) / 2;

#ifdef __APPLE__
  if (false) {                                 // windowed first, then the system full screen below
#else
  if (env->config.fullscreen) {
#endif
    window->size[0] = vidmode->width;
    window->size[1] = vidmode->height;
    window->lsize[0] = mw;
    window->lsize[1] = mh;
    window->lpos[0] = mx;
    window->lpos[1] = my;

    // Borderless window covering the screen, not an exclusive full-screen mode: an exclusive
    // window is minimized by Windows whenever anything else takes focus (a messenger popup, a
    // notification), and the game then stops until it is clicked back.
    int px, py; glfwGetMonitorPos(primary, &px, &py);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_POSITION_X, px);
    glfwWindowHint(GLFW_POSITION_Y, py);
    window->handle =
        glfwCreateWindow(window->size[0], window->size[1], env->config.title, NULL, NULL);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
  } else {
    window->size[0] = mw;
    window->size[1] = mh;
    window->lpos[0] = 0;
    window->lpos[1] = 0;
    window->lsize[0] = vidmode->width;
    window->lsize[1] = vidmode->height;

    glfwWindowHint(GLFW_POSITION_X, mx);
    glfwWindowHint(GLFW_POSITION_Y, my);

    window->handle = glfwCreateWindow(window->size[0], window->size[1],
                                      env->config.title, NULL, NULL);
  }

  if (!window->handle) {
    printf("Error creating window\n");
    return NULL;
  }

  glfwSetWindowUserPointer(window->handle, env);
#ifdef __APPLE__
  // Keep the whole window on screen, centred in the area the menu bar and Dock leave free.
  {
    int ax, ay, aw, ah, w, h, left, top, right, bottom;
    glfwGetMonitorWorkarea(primary, &ax, &ay, &aw, &ah);
    glfwGetWindowSize(window->handle, &w, &h);
    glfwGetWindowFrameSize(window->handle, &left, &top, &right, &bottom);
    int max_w = aw - left - right, max_h = ah - top - bottom;
    if (max_w > 200 && max_h > 200) {
      if (w > max_w || h > max_h) {
        float k = fminf((float)max_w / w, (float)max_h / h);
        w = (int)(w * k); h = (int)(h * k);
        glfwSetWindowSize(window->handle, w, h);
      }
      glfwSetWindowPos(window->handle, ax + left + (max_w - w) / 2, ay + top + (max_h - h) / 2);
    }
  }
#endif
  // Registered only now: resizing above calls back at once, before env->wnd is set.
  glfwGetFramebufferSize(window->handle, &window->size[0], &window->size[1]);
  glfwSetFramebufferSizeCallback(window->handle, window_resize_callback);
#ifdef __APPLE__
  if (env->config.fullscreen) mac_set_fullscreen(window->handle, true);
  mac_disable_app_nap();
#endif

  return window;
}

static void twindow_refresh(twindow* window) {
  if (window->size[0] <= 0 || window->size[1] <= 0) return;
  // OUT_OF_DATE may occur without a framebuffer-size event.
  if (window->_refresh || !window->env->ctx->swapchain_ok) {
    glfwGetFramebufferSize(window->handle, &window->size[0], &window->size[1]);
    if (window->size[0] <= 0 || window->size[1] <= 0) return;
    tcontext_resize(window->env->ctx, window->env->wnd->size, window->env->config.vsync);
    window->_refresh = false;
    window->_resize_func(window->env);
  }
}
void twindow_poll_input(twindow* window) {
  glfwPollEvents();
  twindow_refresh(window);
}
void twindow_wait_input(twindow* window) {
  glfwWaitEvents();
  twindow_refresh(window);
}

void twindow_toggle_fullscreen(twindow* window) {
#ifdef __APPLE__
  bool want = !mac_is_fullscreen(window->handle);   // the green button may have changed it
  mac_set_fullscreen(window->handle, want);
  window->env->config.fullscreen = want;
  return;
#endif
  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);

  if (!window->env->config.fullscreen) {
    glfwGetWindowPos(window->handle, &window->lpos[0], &window->lpos[1]);
    glfwGetWindowSize(window->handle, &window->lsize[0], &window->lsize[1]);

    // Borderless over the whole screen (see twindow_create), so focus changes never minimize it.
    int px, py; glfwGetMonitorPos(monitor, &px, &py);
    glfwRestoreWindow(window->handle);
    glfwSetWindowAttrib(window->handle, GLFW_DECORATED, GLFW_FALSE);
    glfwSetWindowMonitor(window->handle, NULL, px, py, mode->width, mode->height, GLFW_DONT_CARE);

    window->env->config.fullscreen = true;
  } else {
    glfwRestoreWindow(window->handle);
    glfwSetWindowAttrib(window->handle, GLFW_DECORATED, GLFW_TRUE);
    glfwSetWindowMonitor(window->handle, NULL, window->lpos[0], window->lpos[1],
                         window->lsize[0], window->lsize[1], GLFW_DONT_CARE);
    window->env->config.fullscreen = false;
  }
}

bool twindow_is_fullscreen(twindow* window) {
#ifdef __APPLE__
  return mac_is_fullscreen(window->handle);
#else
  return window->env->config.fullscreen;
#endif
}

bool twindow_visible(twindow* window) {
#ifdef __APPLE__
  return glfwGetCocoaWindowVisibleSN(window->handle) != 0;
#else
  (void)window;
  return true;
#endif
}

bool twindow_key_down(twindow* window, int key) {
  return glfwGetKey(window->handle, key) == GLFW_PRESS;
}

bool twindow_button_down(twindow* window, int button) {
  return glfwGetMouseButton(window->handle, button) == GLFW_PRESS;
}

bool twindow_closed(twindow* window) {
  return glfwWindowShouldClose(window->handle);
}

void twindow_destroy(twindow* window) {
  glfwTerminate();
  free(window);
}
