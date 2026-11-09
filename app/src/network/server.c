#include "server.h"

#include "../user.h"
#include "callback.h"
#include "network_pump.h"
#include "path_choice.h"
#include "../game/native_preferences.h"
#include "socket_latency.h"
#include "server_history.h"
#include "server_address.h"
#include "server_status.h"
#include "client_access.h"
#include "../game/input.h"
#include "../game/perf_trace.h"
#include "../game/duel_board.h"
#include "../game/death_review.h"

void server_init(tenv* env) {
  tuser_data* usr = env->usr;
  game_data* gdata = &usr->gdata;
  user_settings* usrs = &usr->usrs;
  mg_log_set(MG_LL_NONE);
  mg_mgr_init(&gdata->network_manager);
}

static path_choice paths;
static bool path_retry;
static bool access_waiting;

static void open_connection(tenv* env) {
  tuser_data* usr = env->usr;
  game_data* gdata = &usr->gdata;
  user_settings* usrs = &usr->usrs;

  memset(&gdata->ping_tracker,0,sizeof(gdata->ping_tracker));
  gdata->prompt_ack=false; gdata->network_polled_at=gdata->network_poll_gap=0;
  gdata->network_session++;
  path_choice_opened(&paths,perf_now());

  // An address by name (a private server's online address) is looked up first; the relay
  // behind it needs the name back as the Host header.
  char url[256] = {}, host[MAX_IPV4_LEN + 1] = {}, clean[MAX_IPV4_LEN + 1];
  if (!server_address_clean(usrs->ipv4, clean)) strcpy(clean, usrs->ipv4);
  if (!server_address_url(clean, "/slither", url, sizeof(url), host, sizeof(host), 2500)) {
    server_set_error("서버 주소를 찾지 못했습니다: %.60s (주소가 바뀌었거나 인터넷 연결 문제)", clean);
    gdata->connection = NULL;
    gdata->closed = true;
    return;
  }
  gdata->connection =
    mg_ws_connect(&gdata->network_manager, url, server_callback, env,
                  "%s:%s\r\n",
                  "Origin", "https://slither.com");
  server_address_host(gdata->connection, host);
  if (!gdata->connection) gdata->closed = true;
}

void server_connect(tenv* env) {
  tuser_data* usr = env->usr;
  user_settings* usrs = &usr->usrs;

  server_status_menu(false);
  server_clear_error();
  char clean[MAX_IPV4_LEN + 1];              // a pasted link becomes the plain address
  if (server_address_clean(usrs->ipv4, clean)) strcpy(usrs->ipv4, clean);
  server_history_remember(usrs->ipv4);
  path_choice_begin(&paths); path_retry=false;
  duel_connected();
  usr->gdata.closed = false;
  access_waiting = true;
  client_access_request();
}

bool server_path_check(tenv* env, struct mg_connection* c) {
  // Off by default: every retry is one more connection, and servers refuse bursts of them.
  if(!native_prefs.path_retry) return false;
  socket_latency latency=socket_latency_read((SOCKET)(uintptr_t)c->fd);
  if(!path_choice_retry(&paths,env->usr->usrs.ipv4,latency.min_rtt_ms,perf_now())) return false;
  path_retry=true;
  return true;
}
bool server_path_retrying(void) { return path_retry; }

void server_poll(tenv* env) {
  tuser_data* usr = env->usr;
  game_data* gdata = &usr->gdata;

  client_access_heartbeat(gdata->curr_screen == PLAYING && gdata->conn == CONNECTED && !access_waiting);
  client_access_result access = client_access_state();
  if (access_waiting && access != ACCESS_PENDING) {
    access_waiting = false;
    if (access != ACCESS_BANNED) { glfwSetTime(0); open_connection(env); }
  }
  if (access == ACCESS_BANNED) {
    access_waiting = path_retry = gdata->restart_req = false;
    death_review_reset();
    server_set_error("관리자가 이 접속 IP 주소의 클라이언트 사용을 차단했습니다.");
    if (gdata->connection) gdata->connection->is_closing = true;
    if (gdata->curr_screen == PLAYING) {
      gdata->closed = true;
      if (!gdata->connection) gdata->conn = DISCONNECTED;
    }
  }

  double start=perf_now();
  if(gdata->network_polled_at && start>=gdata->network_polled_at)
    gdata->network_poll_gap=fmax(gdata->network_poll_gap,start-gdata->network_polled_at);
  gdata->network_polled_at=start;
  gdata->data.ctm=(float)(glfwGetTime()*1000.0);
  game_poll_network(&gdata->network_manager,&gdata->connection,&gdata->closed,perf_now,perf_backlog);
  // The slow connection has finished closing: open its replacement outside the socket callback.
  if(path_retry && !gdata->connection) { path_retry=false; open_connection(env); }
  perf_add(PERF_NETWORK,start);
}

void server_service(tenv* env) {
  game_data* game=&env->usr->gdata;
  if(game->curr_screen!=PLAYING || !env->config.running) { return; }
  input_network(env);
  server_poll(env);
}

void server_destroy(tenv* env) {
  tuser_data* usr = env->usr;
  game_data* gdata = &usr->gdata;

  mg_mgr_free(&gdata->network_manager);
}
