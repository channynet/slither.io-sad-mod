#include "loop.h"

#include "../network/server.h"
#include "../network/client_access.h"
#include "../network/callback.h"
#include "../network/ntl.h"
#include "../user.h"
#include "input.h"
#include "oef.h"
#include "redraw.h"
#include "ui_overlay.h"
#include "perf_trace.h"
#include "death_review.h"
#include "duel_board.h"

void game_background(tenv* env) {
  game_data* gdata = &env->usr->gdata;
  if (gdata->curr_screen != PLAYING) return;
  if (gdata->conn != CONNECTED || death_review.frozen || gdata->closed || !gdata->connection) {
    // Connecting, frozen on the death scene or closing: only keep the socket moving.
    server_poll(env);
    return;
  }
  time_step(env);
  server_service(env);  // pings and steering go out, packets are applied
  // A close is handled by game_loop when the window is shown again.
  if (!death_review.frozen && !gdata->closed && gdata->connection) oef(env);
}

void game_loop(tenv* env) {
  tuser_data* usr = env->usr;
  tcontext* ctx = env->ctx;
  game_data* gdata = &usr->gdata;
  user_settings* usrs = &usr->usrs;

  if (!env->config.running) gdata->conn = DISCONNECTED;
  if(death_review.frozen) {
    server_poll(env);
    death_review_draw(env);
    duel_board_draw(env);
    return;
  }

  switch (gdata->conn) {
    case CONNECTING: {
      usr->r->global.bg_opacity = 0;
      usr->r->global.bd_opacity = 0;
      usr->r->global.minimap_opacity = 0;

      if (gdata->connection && glfwGetTime() > TIMEOUT) {
        server_note_end("no game start within the connect timeout");
        gdata->connection->is_closing = true;
        printf("Connection timed out.");
      }

      server_poll(env);

      vec2 loading_bar = {500, 12};
      igSetCursorPosX(ctx->size[0] * 0.5f - loading_bar[0] * 0.5f);
      igSetCursorPosY(ctx->size[1] * 0.5f - loading_bar[1] * 0.5f);

      igPushStyleColor_Vec4(ImGuiCol_PlotHistogram,
                            (ImVec4){0.168f, 0.668f, 0.375f, 1});
      igProgressBar(-glfwGetTime(), (ImVec2){loading_bar[0], loading_bar[1]},
                    NULL);
      igPopStyleColor(1);

      if(client_access_state() == ACCESS_PENDING) {
        igSetCursorPosX(ctx->size[0] * 0.5f - loading_bar[0] * 0.5f);
        igTextUnformatted("클라이언트 사용 허용 여부를 확인하고 있습니다.", NULL);
      }

      if (gdata->closed) {
        gdata->conn = DISCONNECTED;
        gdata->closed = false;
      }
      break;
    }
    case CONNECTED: {
      if(gdata->closed || !gdata->connection) goto connection_closed;
      time_step(env);
      input(env);
      server_poll(env);
      if(death_review.frozen) { death_review_draw(env); duel_board_draw(env); break; }
      // A close event invalidates the socket immediately, before the next frame.
      if(gdata->closed) goto connection_closed;
      double update_start=perf_now();
      oef(env);
      perf_add(PERF_UPDATE,update_start);
      double geometry_start=perf_now();
      redraw(env);
      ui_overlay(env);
      duel_board_draw(env);
      perf_add(PERF_GEOMETRY,geometry_start);

      // special hotkeys
      if (usrs->hotkeys[HOTKEY_QUIT].active ||
          (usrs->quit_mc && !ntl_mouse_captured() &&
           tmouse_button_pressed(env->ms, GLFW_MOUSE_BUTTON_MIDDLE))) {
        server_note_end("quit key or middle click");
        gdata->connection->is_closing = true;
      } else if (usrs->hotkeys[HOTKEY_RESTART].active ||
                 (usrs->restart_rc && !ntl_mouse_captured() &&
                  tmouse_button_pressed(env->ms, GLFW_MOUSE_BUTTON_RIGHT))) {
        server_note_end("restart key or right click");
        gdata->connection->is_closing = true;
        gdata->restart_req = true;
      }

connection_closed:
      if (gdata->closed) {
        game_data_reset(env);

        if (gdata->restart_req) {
          usr->gdata.conn = CONNECTING;
          glfwSetTime(0);
          server_connect(env);
          gdata->restart_req = false;
        } else {
          usr->gdata.conn = DISCONNECTED;
        }
        gdata->closed = false;
      }

      break;
    }
    case DISCONNECTED:
      usr->r->global.bg_opacity = 0;
      usr->r->global.bd_opacity = 0;
      usr->r->global.minimap_opacity = 0;

      gdata->curr_screen = TITLE_SCREEN;

      game_data_reset(env);
      server_poll(env);

      break;
  }
}
