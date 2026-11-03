#include "input.h"

#include "../user.h"
#include "../network/ntl.h"
#include "../network/input_timing.h"
#include "target_marker.h"
#include "safe_wall.h"
#include "body_hug.h"
#include "body_hug_worker.h"
#include <stdarg.h>
#include <stdio.h>
#include <GLFW/glfw3.h>
#include "native_preferences.h"
#include "native_shortcuts.h"
#include "perf_trace.h"
#include "death_review.h"

bool safe_wall_steering;
float safe_wall_angle;
bool body_hug_active, body_hug_steering;
int native_coil;
float body_hug_tx, body_hug_ty;
bool missile_steering;
float missile_angle;
const char* missile_status;
static bool missile_enabled, missile_was_down;

void missile_reset(void) {
  missile_enabled=missile_steering=false;
  missile_was_down=true; // A held key cannot carry into a new game.
  missile_status=NULL;
}

static bool missile_requested(tenv* env) {
  bool down=twindow_key_down(env->wnd,GLFW_KEY_G);
  bool pressed=down && !missile_was_down;
  missile_was_down=down;
  missile_status=NULL;
  if(!down && !missile_enabled) return false;
  game_data* g=&env->usr->gdata;
  snake* me=g->data.follow_view ? get_snake(g,g->data.snake_id) : NULL;
  if(g->curr_screen!=PLAYING || g->conn!=CONNECTED || !g->connection || g->closed ||
      g->connection->is_closing || !me || me->dead || death_review.frozen)
    missile_status="G 돌진: 플레이 중에 사용할 수 있습니다";
  else if(igGetIO_Nil()->WantTextInput) missile_status="G 돌진: 채팅 입력을 끝내세요";
  else if(native_settings_open) missile_status="G 돌진: 설정 창을 닫으세요";
  else if(target_marker_captured()) missile_status="G 돌진: 대상 선택을 끝내세요";
  else if(!glfwGetWindowAttrib(env->wnd->handle,GLFW_FOCUSED)) missile_status="G 돌진: 게임 창을 선택하세요";
  else if(twindow_key_down(env->wnd,GLFW_KEY_LEFT_CONTROL) || twindow_key_down(env->wnd,GLFW_KEY_RIGHT_CONTROL) ||
      twindow_key_down(env->wnd,GLFW_KEY_LEFT_ALT) || twindow_key_down(env->wnd,GLFW_KEY_RIGHT_ALT) ||
      twindow_key_down(env->wnd,GLFW_KEY_LEFT_SUPER) || twindow_key_down(env->wnd,GLFW_KEY_RIGHT_SUPER))
    missile_status="G 돌진: Ctrl, Alt, Windows 키를 놓으세요";
  for(int i=0;i<NUM_HOTKEYS;i++) if(env->usr->usrs.hotkeys[i].key==GLFW_KEY_G)
    missile_status="G 돌진: 설정에서 다른 기능의 G 단축키를 바꾸세요";
  if(missile_status) { missile_enabled=false; return false; }
  if(pressed) missile_enabled=!missile_enabled;
  if(missile_enabled) missile_status="G 돌진 중: 마우스로 방향 지정 / G를 다시 누르면 종료";
  else if(down && !pressed) missile_status="G 돌진: G를 놓았다가 다시 누르세요";
  return missile_enabled;
}

// Body hug (E): the enemy nearest when E goes down (within 900, not a teammate) is the one for
// as long as E is held: never another body, however near, until that one dies or leaves the
// screen. Fills `out` with its body, tail first, head last.
static int hug_target_id=-1;
static hug_state hug_st;
static hug_point hug_body[HUG_MAX_POINTS];
static const snake* hug_pick(tenv* env,game_data* g,const snake* me,float hx,float hy,int* count) {
  const snake* best=NULL; double best_d=1e18, kept_d=1e18; const snake* kept=NULL;
  int n=tdarray_length(g->data.snakes);
  for(int i=0;i<n;i++) {
    const snake* o=g->data.snakes+i;
    if(o==me || o->id<0 || o->dead || !o->pts) continue;
    if(o->id==hug_target_id && hug_target_id>=0 && o->alive_amt>=.5f) { kept=o; continue; }
    if(o->sp<0.01f || o->alive_amt<.5f) continue;
    if(ntl_is_teammate(env,o->ntl_id)) continue;
    double r=14.5*o->sc, d2min=1e18;
    int m=tdarray_length(o->pts);
    for(int k=0;k<m;k+=2) {
      const body_part* p=o->pts+k; if(p->dying) continue;
      double dx=p->xx-hx, dy=p->yy-hy, d2=dx*dx+dy*dy;
      if(d2<d2min) d2min=d2;
    }
    double dx=o->xx-hx, dy=o->yy-hy;
    if(dx*dx+dy*dy<d2min) d2min=dx*dx+dy*dy;
    double d=sqrt(d2min)-r;
    if(d<best_d) { best_d=d; best=o; }
  }
  (void)kept_d;
  if(kept) best=kept, best_d=0;                      // the one picked when E went down
  else if(hug_target_id>=0) body_hug_log(-1,"target %d is gone (died or left the screen): the nearest is taken",hug_target_id);
  if(!best || best_d>900) { hug_target_id=-1; body_hug_log(1,"no target: nearest enemy body %.0f away (limit 900)%s",best ? best_d : -1.0,best ? "" : " (none at all)"); return NULL; }
  if(best->id!=hug_target_id) { hug_target_id=best->id; hug_st=(hug_state){0}; }
  int m=tdarray_length(best->pts), c=0;
  for(int k=m>HUG_MAX_POINTS-1 ? m-(HUG_MAX_POINTS-1) : 0;k<m && c<HUG_MAX_POINTS-1;k++) {   // (a very long one: its front)
    const body_part* p=best->pts+k; if(p->dying) continue;
    hug_body[c++]=(hug_point){p->xx,p->yy};
  }
  hug_body[c++]=(hug_point){best->xx,best->yy};
  *count=c;
  return best;
}

// E diagnostics: hug-log.txt beside the program, written only while E is held (started afresh
// at the first E of each run, kept under 2 MB). Lines: time, what happened.
static FILE* hug_log_file; static long hug_log_bytes; static double hug_log_last[12];
void body_hug_log(int kind,const char* fmt,...) {
  double t=glfwGetTime();
  if(kind>=0 && kind<12) { if(t-hug_log_last[kind]<.25) return; hug_log_last[kind]=t; }   // the same kind at most 4 times a second
  if(!hug_log_file) { hug_log_file=fopen("hug-log.txt","w"); if(!hug_log_file) return; }
  if(hug_log_bytes>2000000) return;
  va_list ap; va_start(ap,fmt);
  hug_log_bytes+=fprintf(hug_log_file,"%.2f ",t);
  hug_log_bytes+=vfprintf(hug_log_file,fmt,ap);
  hug_log_bytes+=fprintf(hug_log_file,"\n");
  va_end(ap); fflush(hug_log_file);
}

// A planned answer arrived (the lab build logs it here).
static void body_hug_report(const hug_output* o,int target) { (void)o; (void)target; }

// The angles sent lately (ms, radians), so the hug can carry its own head forward through the
// ones still on their way: an angle acts at the server half a ping after it is sent, and what
// is on screen is half a ping old.
static double sent_t[64], sent_a[64]; static int sent_n, sent_i;
static void sent_record(double t,double a) { sent_t[sent_i]=t; sent_a[sent_i]=a; sent_i=(sent_i+1)%64; if(sent_n<64) sent_n++; }
// How long an angle sent takes to show on screen: from sending it until the game reports the
// snake steering to it (the network both ways, the server's step, our frames). What is on
// screen is that far behind what an angle sent now will act on.
// The median of the last few, as single ones jump with slow frames.
static double echo_t=-1, echo_a, echo_ms=-1, echo_seen[9]; static int echo_n, echo_i;
static void echo_check(double now,double wang) {
  if(echo_t<0) return;
  if(fabs(hug_wrap(wang-echo_a))<.02) {
    echo_seen[echo_i]=now-echo_t; echo_i=(echo_i+1)%9; if(echo_n<9) echo_n++;
    double v[9]; for(int i=0;i<echo_n;i++) v[i]=echo_seen[i];
    for(int i=1;i<echo_n;i++) for(int j=i;j>0 && v[j-1]>v[j];j--) { double t=v[j]; v[j]=v[j-1]; v[j-1]=t; }
    echo_ms=v[echo_n/2]; echo_t=-1;
  } else if(now-echo_t>1500) echo_t=-1;
}
static double sent_at(double t,double fallback) {        // the angle last sent at or before t
  double best_t=-1e300, a=fallback;
  for(int i=0;i<sent_n;i++) if(sent_t[i]<=t && sent_t[i]>best_t) { best_t=sent_t[i]; a=sent_a[i]; }
  return a;
}


void input(tenv* env) {
  tuser_data* usr = env->usr;
  game_data* gdata = &usr->gdata;
  user_settings* usrs = &usr->usrs;
  struct mg_connection* connection = gdata->connection;

  if (igGetIO_Nil()->WantTextInput || ntl_mouse_captured()) {
    // Text/navigation keys stay in the UI. The network input path still
    // handles mouse steering and independently suppresses accidental boost.
    return;
  }

  if (gdata->data.follow_view) {
    int snakes_len = tdarray_length(gdata->data.snakes);
    snake* me = gdata->data.snakes + (snakes_len - 1);

    if (!usrs->hotkeys[HOTKEY_BOT].active && !missile_steering) {
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
  bool missile_was_steering=missile_steering;
  missile_steering=missile_requested(env);
  struct mg_connection* connection=gdata->connection;
  if(gdata->conn!=CONNECTED || !connection || connection->is_closing || gdata->closed) return;
  float now=(float)(glfwGetTime()*1000.0);
  if(!gdata->data.wfpr && input_due(now,gdata->data.last_ping_mtm,250)) {
    if(mg_ws_send(connection,(uint8_t[]){251},1,WEBSOCKET_OP_BINARY)) {
      gdata->data.last_ping_mtm=now; gdata->data.wfpr=true;
      ping_begin(&gdata->ping_tracker,glfwGetTime()*1000.0,connection->send.len);
    }
  }
  // Chat captures typing/clicks, but the cursor still steers the snake.
  // Other UI and focus loss continue to suspend manual steering.
  bool bot_active=usrs->hotkeys[HOTKEY_BOT].active && !missile_steering;
  bool ui_capture=igGetIO_Nil()->WantTextInput || ntl_mouse_captured();
  // E (body hug) does not use the mouse, so the pointer resting on a panel (chat, team list,
  // settings) does not stop it; typing and an unfocused window still do.
  bool hug_held=body_hug_active && !igGetIO_Nil()->WantTextInput;
  bool coil_on=native_coil!=0;           // F1/F2 turning on: no mouse needed either
  if(body_hug_active && bot_active) body_hug_log(4,"E held but the bot (T) is on: the hug is off");
  if(body_hug_active && !glfwGetWindowAttrib(env->wnd->handle,GLFW_FOCUSED)) body_hug_log(5,"E held but the window is not focused");
  if(!bot_active && ((ui_capture && !ntl_chat_typing() && !hug_held && !coil_on && !missile_steering) ||
      !glfwGetWindowAttrib(env->wnd->handle,GLFW_FOCUSED))) {
    if(gdata->data.md) {
      gdata->data.md=gdata->data.wmd=false;
      mg_ws_send(connection,(uint8_t[]){254},1,WEBSOCKET_OP_BINARY);
    }
    return;
  }
  if(!gdata->data.follow_view) return;
  snake* me=get_snake(gdata,gdata->data.snake_id);
  static bool hug_dead_logged;
  if(!me || me->dead) {
    native_coil=0;                       // (as NTL: a new snake starts without it)
    bool logged=hug_dead_logged;
    if(body_hug_active && !logged) body_hug_log(-1,"own snake dead or gone while E held (target %d)",hug_target_id);
    hug_dead_logged=true;
    return;
  }
  hug_dead_logged=false;
  echo_check(now,me->wang);
  double mouse_x,mouse_y;
  glfwGetCursorPos(env->wnd->handle,&mouse_x,&mouse_y);
  env->ms->pos[0]=(float)mouse_x; env->ms->pos[1]=(float)mouse_y;
  int xm=(int)mouse_x-ctx->size[0]/2,ym=(int)mouse_y-ctx->size[1]/2;
  if(bot_active) { xm=gdata->bot.output.xm; ym=gdata->bot.output.ym; }
  // F1/F2 (as NTL's Coil Left/Right): point 135 degrees round to that side of the heading, so
  // the snake keeps turning as hard as it can (the mouse and the bot are ignored meanwhile).
  if(native_coil && !missile_steering) {
    double a=me->ang+native_coil*.75*PI2/2;
    xm=(int)lround(300*cos(a)); ym=(int)lround(300*sin(a));
  }
  // Safewall (NTL's scripts 1 and 2): near the wall, a mouse pointing out of the map steers to
  // a point on a safe circle inside the wall instead, so the snake slides along the border.
  static bool safe_pending;              // the last angle sent was a safewall one
  static safe_wall_state safe_state;
  bool safe_steer=false; double safe_want=0;
  double wall=gdata->data.flux_announced ? gdata->data.real_flux_grd : gdata->data.grd*.98;
  if(native_prefs.safe_wall && !bot_active && gdata->data.grd>0) {
    safe_wall_input in={
      .cx=gdata->data.grd,.cy=gdata->data.grd,.grd=gdata->data.grd,
      .dynamic=gdata->data.flux_announced,.wall=gdata->data.real_flux_grd,
      .head_x=me->xx,.head_y=me->yy,.view_x=gdata->data.view_xx,.view_y=gdata->data.view_yy,
      .ang=me->ang,.wang=me->wang,.mouse_x=xm,.mouse_y=ym,.gsc=gdata->data.gsc,
      .sp=me->sp,.scang=me->scang,.spang=me->spang,.mamu=gdata->data.mamu,.nsp3=gdata->data.nsp3,
      .boosting=gdata->data.wmd,.script=native_prefs.safe_wall_script,.offset=native_prefs.safe_wall_offset,.gap=native_prefs.safe_wall_tight ? native_prefs.safe_wall_distance : -1};
    safe_wall_output out;
    safe_steer=safe_wall_steer(&in,&safe_state,&out);
    if(safe_steer) safe_want=atan2(out.dy,out.dx);
  } else safe_state.latched=false;
  safe_wall_steering=safe_steer; safe_wall_angle=(float)safe_want;
  // Body hug (E held): along the nearest enemy body. Planned on its own thread from a snapshot
  // taken every 33 ms; each answer is sent the moment it is ready. The safewall wins near the wall.
  static bool hug_pending, hug_last_steer, hug_fresh; static double hug_last_want;
  static int hug_last_delay, hug_last_period, hug_last_tier; static double hug_last_noise, hug_last_gap=1e9;
  static int hug_job, hug_job_target[16]; static double hug_job_t[16], hug_think_ms=4, hug_submit_t, hug_plan_ms=4;
  static double hug_prev_t, hug_period_ms=STEER_INTERVAL_MS;
  bool hug_steer=false; double hug_want=0;
  // The player's boost (mouse button or keys; not with the pointer over a panel).
  bool boost_keys=missile_steering || (!ui_capture && (twindow_button_down(env->wnd, GLFW_MOUSE_BUTTON_LEFT) ||
                                  twindow_key_down(env->wnd, GLFW_KEY_SPACE) ||
                                  twindow_key_down(env->wnd, GLFW_KEY_UP) ||
                                  twindow_key_down(env->wnd, GLFW_KEY_LEFT_SHIFT) ||
                                  twindow_key_down(env->wnd, GLFW_KEY_RIGHT_SHIFT)));
  static bool hug_slow;
  if(body_hug_active && !bot_active && !missile_steering) {
    hug_output o; hug_state ost; int id;
    if(hug_worker_take(&o,&ost,&id) && hug_job_target[id&15]==hug_target_id && hug_target_id>=0 && isfinite(o.angle)) {
      hug_st=ost; hug_last_steer=true; hug_last_want=o.angle; hug_fresh=true; hug_last_tier=o.tier; hug_last_gap=o.edge_gap; hug_slow=o.slow;
      body_hug_tx=(float)o.tx; body_hug_ty=(float)o.ty;
      hug_think_ms+=(now-hug_job_t[id&15]-hug_think_ms)*.2;
      if(o.ms>0) hug_plan_ms+=(o.ms-hug_plan_ms)*.2;
      // How often angles really go out (a slow frame rate stretches it): the planner holds each that long.
      if(hug_prev_t>0 && now-hug_prev_t<250) hug_period_ms+=(now-hug_prev_t-hug_period_ms)*.2;
      hug_prev_t=now;
      body_hug_report(&o,hug_target_id);
      body_hug_log(-1,"hug target %d gap %.1f tier %d margin %.1f angle %.3f heading %.3f head %.1f,%.1f sc %.2f sp %.2f%s side %d dir %d aim %.0f,%.0f plan %.1fms wait %.1fms delay %d noise %.2f period %d",
                   hug_target_id,o.edge_gap,o.tier,o.margin,o.angle,me->ang,me->xx,me->yy,me->sc,me->sp,o.slow ? " slow" : "",ost.side,ost.dir,o.tx,o.ty,
                   o.ms,hug_think_ms,hug_last_delay,hug_last_noise,hug_last_period);
    }
    // A plan is started so that it is ready just before the next angle may be sent (at most one
    // every 33 ms: never more often than the mouse would, whatever the planning takes).
    double lead=fmin(hug_think_ms+2,20);
    if(hug_worker_busy() && now-hug_submit_t>200) body_hug_log(0,"planner still busy after %.0f ms",now-hug_submit_t);
    if(!hug_worker_busy() && now-hug_submit_t>=12 && now>=gdata->data.last_e_mtm+STEER_INTERVAL_MS-lead) {
      hug_submit_t=now;
      float hx=me->xx, hy=me->yy; int count=0;   // where the game has things (on screen they glide by fx)
      const snake* target=hug_pick(env,gdata,me,hx,hy,&count);
      if(!target) hug_last_steer=false;
      else {
        double turn=gdata->data.mamu*me->scang*me->spang, step=me->sp/4, radius=turn>0 ? step/turn : 50;   // per 8 ms (oef: sp/4)
        // The delay: an angle sent now shows on screen this much later (measured; at the least a
        // ping and a server step), so carry the head forward that far through the angles in flight;
        // the planning itself takes a few ms more.
        const ping_tracker* pt=&gdata->ping_tracker;
        double ping=(pt->count ? pt->sum/pt->count : 0)+16, lag=echo_ms>ping ? echo_ms : ping;
        int delay=(int)lround((lag+lead)/8); if(delay<0) delay=0; if(delay>48) delay=48;
        static double pre[64];
        for(int k=0;k<delay;k++) pre[k]=sent_at(now-lag+k*8.0,me->wang);
        // How far the head has lately strayed sideways from where its motion model put it (jerky
        // frames, network jitter): the hug keeps that much more room.
        static double prev_t, prev_x, prev_y, prev_a, noise2;
        if(prev_t>0 && now-prev_t<200) {
          int n=(int)lround((now-prev_t)/8); double x=prev_x, y=prev_y, a=prev_a;
          for(int k=0;k<n;k++) {
            double d=hug_wrap(sent_at(prev_t+k*8.0-lag,me->wang)-a);
            a+=fabs(d)<turn ? d : (d>0 ? turn : -turn);
            x+=cos(a)*step; y+=sin(a)*step;
          }
          double side=fabs(-sin(a)*(hx-x)+cos(a)*(hy-y));
          noise2+=(side*side-noise2)*.1;
        }
        prev_t=now; prev_x=hx; prev_y=hy; prev_a=me->ang;
        // Everyone close by: their bodies now, and where their heads can be (the hugged one's
        // too): heads swing sideways much faster than bodies move.
        static double others[HUG_OTHER*HUG_MAX_OTHERS]; int others_count=0;
        int all=tdarray_length(gdata->data.snakes);
        for(int i=0;i<all && others_count<HUG_MAX_OTHERS-200;i++) {
          const snake* os=gdata->data.snakes+i;
          if(os==me || os->id<0 || os->dead || !os->pts) continue;
          float ox=os->xx, oy=os->yy; double r=14.5*os->sc;
          if(fabsf(ox-hx)<500 && fabsf(oy-hy)<500 && os->sp>0.01f) {
            // How it has lately been turning (a step of 8 ms): a head curling round is followed on round.
            static int spin_id[1024]; static float spin_ang[1024]; static double spin_t[1024], spin_v[1024];
            int si=os->id&1023;
            if(spin_id[si]==os->id && now-spin_t[si]>1 && now-spin_t[si]<200) {
              double v=hug_wrap(os->ang-spin_ang[si])/((now-spin_t[si])/8);
              spin_v[si]+=(v-spin_v[si])*.5;
            } else if(spin_id[si]!=os->id || now-spin_t[si]>=200) spin_v[si]=0;
            spin_id[si]=os->id; spin_ang[si]=os->ang; spin_t[si]=now;
            others_count=hug_head_paths(others,others_count,HUG_MAX_OTHERS,ox,oy,os->ang,os->wang,
                                        gdata->data.mamu*os->scang*os->spang,os->sp/4,delay,r,spin_v[si]);
          }
          if(os==target) continue;            // its body is the hugged polyline
          // Its body as discs close enough together to leave no gap (body points are 42 apart,
          // wider than a thin snake: in between too).
          int m=tdarray_length(os->pts); bool have=false; float lx=0, ly=0;
          for(int k=0;k<=m && others_count<HUG_MAX_OTHERS;k++) {
            float px=k<m ? os->pts[k].xx : ox, py=k<m ? os->pts[k].yy : oy;
            if(k<m && os->pts[k].dying) { have=false; continue; }
            bool close_by=fabsf(px-hx)<=450 && fabsf(py-hy)<=450;
            if(!have && close_by) { double* q=others+HUG_OTHER*others_count++; q[0]=px; q[1]=py; q[2]=r; q[3]=0; q[4]=0; }
            if(have && (close_by || (fabsf(lx-hx)<=450 && fabsf(ly-hy)<=450))) {
              double len=hypot(px-lx,py-ly); int parts=(int)ceil(len/(.7*r));
              if(parts>8) parts=8;
              for(int j=1;j<=parts && others_count<HUG_MAX_OTHERS;j++) {
                double* q=others+HUG_OTHER*others_count++;
                q[0]=lx+(px-lx)*j/parts; q[1]=ly+(py-ly)*j/parts; q[2]=r; q[3]=0; q[4]=0;
              }
            }
            have=true; lx=px; ly=py;
          }
        }
        hug_input in={hug_body,count,14.5*target->sc,hx,hy,me->ang,14.5*me->sc,native_prefs.hug_gap,fmax(60,2.2*radius),radius,step,turn,
                      others,others_count,gdata->data.grd,gdata->data.grd,wall,pre,delay,target->sp/4,(int)lround(hug_period_ms/8),sqrt(noise2),
                      hug_plan_ms>HUG_LIGHT_MS,   // a slow computer: plan more lightly
                      // the speed: where the boost already sent takes it, without and with boosting
                      (gdata->data.md ? me->msp : me->ssp)/4,me->ssp/4,boost_keys ? me->msp/4 : 0,HUG_SPEED_RATE/4};
        hug_last_delay=delay; hug_last_noise=in.noise; hug_last_period=in.period;
        // (the hugged one's head, for the log)
        body_hug_log(-1,"  target %d head %.1f,%.1f ang %.3f sc %.2f sp %.2f body %d pts from %.0f,%.0f; others %d%s",target->id,target->xx,target->yy,target->ang,target->sc,target->sp,
                     count,count ? hug_body[0].x : 0,count ? hug_body[0].y : 0,others_count,boost_keys ? "; boost held" : "");
        hug_job++; hug_job_target[hug_job&15]=target->id; hug_job_t[hug_job&15]=now;
        hug_worker_submit(&in,&hug_st,hug_job);
      }
    }
    hug_steer=hug_last_steer; hug_want=hug_last_want;
  } else { hug_target_id=-1; hug_st=(hug_state){0}; hug_last_steer=false; hug_fresh=false; hug_slow=false; }
  if(safe_steer && hug_steer) body_hug_log(3,"safewall overrides the hug (near the wall, mouse pointing out)");
  if(safe_steer) hug_steer=false;
  body_hug_steering=hug_steer;
  if(gdata->data.grd>0)
    perf_safe_wall(native_prefs.safe_wall,safe_steer,
                   (float)(wall-hypot(me->xx+me->fx-gdata->data.grd,me->yy+me->fy-gdata->data.grd)));
    missile_angle=safe_steer ? (float)safe_want : (xm*xm+ym*ym>256 ? atan2f(ym,xm) : me->wang);
    gdata->data.wmd = bot_active ? gdata->bot.output.accel : boost_keys;
    // E: the hug plans with the boost held and lets go of it when boosting on leaves no way out
    // (the turning circle more than doubles), or would only run out ahead of the hugged head.
    if (hug_steer && gdata->data.wmd && hug_slow) {
      gdata->data.wmd = false;
      body_hug_log(8, "boost let go (tier %d, gap %.1f)", hug_last_tier, hug_last_gap);
    }

    if (gdata->data.md != gdata->data.wmd &&
        ((missile_was_steering && !missile_steering && !gdata->data.wmd) ||
         (!bot_active && ui_capture) || input_due(now,gdata->data.last_accel_mtm,BOOST_INTERVAL_MS))) {
      gdata->data.md = gdata->data.wmd;
      gdata->data.last_accel_mtm = now;
      mg_ws_send(connection, (uint8_t[]){gdata->data.md ? 253 : 254}, 1,
                 WEBSOCKET_OP_BINARY);
    }

    bool want_e = false;
    if (xm != gdata->data.lsxm || ym != gdata->data.lsym) want_e = true;
    if (safe_steer || safe_pending) want_e = true;   // the wall angle moves while the mouse stays
    if (hug_steer || hug_pending) want_e = true;
    me->eang = atan2f(ym, xm);
    float ang;
    if (want_e && input_due(now,gdata->data.last_e_mtm,STEER_INTERVAL_MS)) {
      hug_fresh = false;
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
      if (safe_steer) ang = (float)safe_want;
      else if (hug_steer) ang = (float)hug_want;
      safe_pending = safe_steer;
      hug_pending = hug_steer;
      if (!isfinite(ang)) ang = me->wang;          // never a broken angle on the wire (it would become a control byte)
      ang = fmodf(ang, PI2);
      if (ang < 0) ang += PI2;
      int sang = (int)floorf((250 + 1) * ang / PI2 + .5f) % 251;   // the nearest of the 251 directions
      sent_record(now, sang * PI2 / 251);
      if (echo_t < 0 && fabs(hug_wrap(sang * PI2 / 251 - me->wang)) > .05) { echo_t = now; echo_a = sang * PI2 / 251; }
      if (sang != gdata->data.lsang) {
        gdata->data.lsang = sang;
        mg_ws_send(connection, (uint8_t[]){sang & 255}, 1, WEBSOCKET_OP_BINARY);
      }
    }
}
