#include "tentry.h"

int tentry(int argc, char** argv, size_t usr_size) {
  tenv env;

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
  env.kb = tkeyboard_create(env.wnd);
  env.ms = tmouse_create(env.wnd);
  if ((env.ctx = tcontext_create(env.wnd, env.config.vsync, env.config.fif)) == NULL) {
    printf("Error creating context.\n");
    exit(1);
  }
  tinit(&env);

  while (env.config.running) {
    if (env.wnd->size[0] > 0 && env.wnd->size[1] > 0) {
      twindow_poll_input(env.wnd);
    } else {
      twindow_wait_input(env.wnd);
    }

    env.ctx->frame_submitted=false;
    tinput(&env);
    if (env.config.running && env.ctx->swapchain_ok &&
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
