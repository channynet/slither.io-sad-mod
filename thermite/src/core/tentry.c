#include "tentry.h"

int tentry(int argc, char** argv, size_t usr_size) {
  tenv env = {0};

  env.config.argc = argc;
  env.config.argv = argv;
  env.config.vsync = true;
  env.config.running = true;
  env.config.fullscreen = false;
  env.config.resizable = true;
  env.config.aspect_ratio = 16 / 9.0f;
  env.config.fif = 3;
  env.config.title = "app";
  env.usr = malloc(usr_size);

  tlaunch(&env);

  env.wnd = twindow_create(&env, trender, tresize);
  if (!env.wnd) {
    free(env.usr);
    return 1;
  }
  env.kb = tkeyboard_create(env.wnd);
  env.ms = tmouse_create(env.wnd);
  if ((env.ctx = tcontext_create(env.wnd, env.config.vsync, env.config.fif)) == NULL) {
    printf("Error creating context.\n");
    exit(1);
  }
  tinit(&env);
  twindow_finish_init(env.wnd);

  while (env.config.running) {
    // Nothing can be shown while minimized (size 0 on Windows) or, on macOS, while the window
    // is minimized, hidden or covered: there the system gives out no image to draw into and
    // waiting for one would stall the game for up to a second per frame.
    bool drawable = env.wnd->size[0] > 0 && env.wnd->size[1] > 0 && twindow_visible(env.wnd);
    if (drawable) {
      twindow_poll_input(env.wnd);
    } else {
      // Minimized: nothing to draw, but sockets and game state must not stall.
      glfwWaitEventsTimeout(1.0 / 60);
      twindow_poll_input(env.wnd);
      if (env.config.running) tbackground(&env);
    }

    env.ctx->frame_submitted=false;
    tinput(&env);
    if (env.config.running && env.ctx->swapchain_ok && drawable &&
        env.wnd->size[0] > 0 && env.wnd->size[1] > 0 && !env.wnd->_refresh)
      trender(&env);

    if(env.ctx->frame_submitted) {
      tkeyboard_update(env.kb);
      tmouse_update(env.ms);
      tidle(&env);
    }
  }
  tcontext_wait_idle(env.ctx);

  tdestroy(&env);
  tcontext_destroy(env.ctx);
  tmouse_destroy(env.ms);
  tkeyboard_destroy(env.kb);
  twindow_destroy(env.wnd);
  free(env.usr);

  return 0;
}
