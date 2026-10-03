#include "server.h"

#include "../user.h"
#include "callback.h"
#include "network_pump.h"
#include "server_history.h"
#include "server_status.h"
#include "../game/input.h"
#include "../game/perf_trace.h"

void server_init(tenv* env) {
  tuser_data* usr = env->usr;
  game_data* gdata = &usr->gdata;
  user_settings* usrs = &usr->usrs;
  mg_log_set(MG_LL_NONE);
  mg_mgr_init(&gdata->network_manager);
}

void server_connect(tenv* env) {
  tuser_data* usr = env->usr;
  game_data* gdata = &usr->gdata;
  user_settings* usrs = &usr->usrs;

  server_status_menu(false);
  server_history_remember(usrs->ipv4);

  char url[256] = {};
  sprintf(url, "ws://%s/slither", usrs->ipv4);
  gdata->connection =
    mg_ws_connect(&gdata->network_manager, url, server_callback, env,
                  "%s:%s\r\n",
                  "Origin", "https://slither.com");
  if (!gdata->connection) gdata->closed = true;
}

void server_poll(tenv* env) {
  tuser_data* usr = env->usr;
  game_data* gdata = &usr->gdata;

  double start=perf_now();
  gdata->data.ctm=(float)(glfwGetTime()*1000.0);
  game_poll_network(&gdata->network_manager,&gdata->connection,&gdata->closed,perf_now,perf_backlog);
  perf_add(PERF_NETWORK,start);
}

void server_service(tenv* env) {
  game_data* game=&env->usr->gdata;
  if(game->curr_screen!=PLAYING || !env->config.running) return;
  input_network(env);
  server_poll(env);
}

void server_destroy(tenv* env) {
  tuser_data* usr = env->usr;
  game_data* gdata = &usr->gdata;

  mg_mgr_free(&gdata->network_manager);
}
