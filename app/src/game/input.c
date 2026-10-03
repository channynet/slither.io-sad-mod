#include "input.h"

#include "../user.h"
#include "../network/ntl.h"
#include "../network/input_timing.h"
#include "target_marker.h"

void input(tenv* env) {
  tuser_data* usr = env->usr;
  game_data* gdata = &usr->gdata;
  user_settings* usrs = &usr->usrs;
  struct mg_connection* connection = gdata->connection;

  if (igGetIO_Nil()->WantTextInput || ntl_mouse_captured()) {
    if (gdata->data.md) {
      gdata->data.md = gdata->data.wmd = false;
      mg_ws_send(connection, (uint8_t[]){254}, 1, WEBSOCKET_OP_BINARY);
    }
    return;
  }

  if (gdata->data.follow_view) {
    int snakes_len = tdarray_length(gdata->data.snakes);
    snake* me = gdata->data.snakes + (snakes_len - 1);

    if (!usrs->hotkeys[HOTKEY_BOT].active) {
      if (twindow_key_down(env->wnd, GLFW_KEY_LEFT))
        gdata->data.kd_l_frb += gdata->data.vfrb;
      if (twindow_key_down(env->wnd, GLFW_KEY_RIGHT))
        gdata->data.kd_r_frb += gdata->data.vfrb;

      if (gdata->data.kd_l_frb > 0 || gdata->data.kd_r_frb > 0)
        if (input_due(gdata->data.ctm,gdata->data.lkstm,KEY_INTERVAL_MS)) {
          gdata->data.lkstm = gdata->data.ctm;
          if (gdata->data.kd_r_frb > 0)
            if (gdata->data.kd_l_frb > gdata->data.kd_r_frb) {
              gdata->data.kd_l_frb -= gdata->data.kd_r_frb;
              gdata->data.kd_r_frb = 0;
            }
          if (gdata->data.kd_l_frb > 0)
            if (gdata->data.kd_r_frb > gdata->data.kd_l_frb) {
              gdata->data.kd_r_frb -= gdata->data.kd_l_frb;
              gdata->data.kd_l_frb = 0;
            }
          if (gdata->data.kd_l_frb > 0) {
            int v = gdata->data.kd_l_frb;
            if (v > 127) v = 127;
            gdata->data.kd_l_frb -= v;
            me->eang -= gdata->data.mamu * v * me->scang * me->spang;
            mg_ws_send(connection, (uint8_t[]){252, (uint8_t)v}, 2,
                       WEBSOCKET_OP_BINARY);
          } else if (gdata->data.kd_r_frb > 0) {
            int v = gdata->data.kd_r_frb;
            if (v > 127) v = 127;
            gdata->data.kd_r_frb -= v;
            me->eang += gdata->data.mamu * v * me->scang * me->spang;
            v += 128;
            mg_ws_send(connection, (uint8_t[]){252, (uint8_t)v}, 2,
                       WEBSOCKET_OP_BINARY);
          }
        }

    }

  }

  gdata->data.ms_zoom *= expf(env->ms->dwheel * usrs->zoom_step);

  if (!target_marker_captured() && tkeyboard_key_pressed(env->kb, GLFW_KEY_M))
    gdata->data.ms_zoom *= expf(1 * usrs->zoom_step);
  else if (!target_marker_captured() && tkeyboard_key_pressed(env->kb, GLFW_KEY_N))
    gdata->data.ms_zoom *= expf(-1 * usrs->zoom_step);

  gdata->data.ms_zoom =
      GLM_MAX(MAX_ZOOM_OUT, GLM_MIN(gdata->data.ms_zoom, MAX_ZOOM_IN));

  if (gdata->data.follow_view) {
    snake* me = gdata->data.snakes + (tdarray_length(gdata->data.snakes) - 1);
    int score = (int)floorf((gdata->data.fpsls[me->sct] +
                             me->fam / gdata->data.fmlts[me->sct] - 1) *
                                15 -
                            5) /
                1;
    if (score >= 1000) {
      usrs->hotkeys[HOTKEY_RESTART].active = false;
    }
  }

  gameplay_mode* mode = usrs->modes + usrs->hotkeys[HOTKEY_ASSIST].active;
  if (mode->show_crosshair) igSetMouseCursor(ImGuiMouseCursor_None);
}

// Runs on the main thread during frame pacing and GPU waits as well as frames.
// Packet cadence is independent of the display refresh rate; game simulation
// remains in game_loop, and no game-state pointers cross threads.
void input_network(tenv* env) {
  tuser_data* usr=env->usr;
  game_data* gdata=&usr->gdata;
  user_settings* usrs=&usr->usrs;
  tcontext* ctx=env->ctx;
  struct mg_connection* connection=gdata->connection;
  if(gdata->conn!=CONNECTED || !connection || connection->is_closing || gdata->closed) return;
  float now=(float)(glfwGetTime()*1000.0);
  if(!gdata->data.wfpr && input_due(now,gdata->data.last_ping_mtm,250)) {
    gdata->data.last_ping_mtm=now; gdata->data.wfpr=true;
    mg_ws_send(connection,(uint8_t[]){251},1,WEBSOCKET_OP_BINARY);
  }
  if(igGetIO_Nil()->WantTextInput || ntl_mouse_captured() ||
      !glfwGetWindowAttrib(env->wnd->handle,GLFW_FOCUSED)) {
    if(gdata->data.md) {
      gdata->data.md=gdata->data.wmd=false;
      mg_ws_send(connection,(uint8_t[]){254},1,WEBSOCKET_OP_BINARY);
    }
    return;
  }
  if(!gdata->data.follow_view) return;
  snake* me=get_snake(gdata,gdata->data.snake_id);
  if(!me || me->dead) return;
  double mouse_x,mouse_y;
  glfwGetCursorPos(env->wnd->handle,&mouse_x,&mouse_y);
  env->ms->pos[0]=(float)mouse_x; env->ms->pos[1]=(float)mouse_y;
  int xm=(int)mouse_x-ctx->size[0]/2,ym=(int)mouse_y-ctx->size[1]/2;
  if(usrs->hotkeys[HOTKEY_BOT].active) { xm=gdata->bot.output.xm; ym=gdata->bot.output.ym; }
    gdata->data.wmd = usrs->hotkeys[HOTKEY_BOT].active ? gdata->bot.output.accel : (twindow_button_down(env->wnd, GLFW_MOUSE_BUTTON_LEFT) ||
                      twindow_key_down(env->wnd, GLFW_KEY_SPACE) ||
                      twindow_key_down(env->wnd, GLFW_KEY_UP) ||
                      twindow_key_down(env->wnd, GLFW_KEY_LEFT_SHIFT) ||
                      twindow_key_down(env->wnd, GLFW_KEY_RIGHT_SHIFT) ||
                      false);

    if (gdata->data.md != gdata->data.wmd &&
        input_due(now,gdata->data.last_accel_mtm,BOOST_INTERVAL_MS)) {
      gdata->data.md = gdata->data.wmd;
      gdata->data.last_accel_mtm = now;
      mg_ws_send(connection, (uint8_t[]){gdata->data.md ? 253 : 254}, 1,
                 WEBSOCKET_OP_BINARY);
    }

    bool want_e = false;
    if (xm != gdata->data.lsxm || ym != gdata->data.lsym) want_e = true;
    me->eang = atan2f(ym, xm);
    float ang;
    if (want_e && input_due(now,gdata->data.last_e_mtm,STEER_INTERVAL_MS)) {
      want_e = false;
      gdata->data.last_e_mtm = now;
      gdata->data.lsxm = xm;
      gdata->data.lsym = ym;
      float d2 = xm * xm + ym * ym;
      if (d2 > 256) {
        ang = atan2f(ym, xm);
        me->eang = ang;
      } else
        ang = me->wang;
      ang = fmodf(ang, PI2);
      if (ang < 0) ang += PI2;
      int sang = (int)floorf((250 + 1) * ang / PI2);
      if (sang != gdata->data.lsang) {
        gdata->data.lsang = sang;
        mg_ws_send(connection, (uint8_t[]){sang & 255}, 1, WEBSOCKET_OP_BINARY);
      }
    }
}
