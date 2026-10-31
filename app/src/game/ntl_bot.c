#include "ntl_bot.h"
#include "ntl_bot_core.h"
#include "native_preferences.h"
#include "perf_trace.h"
#include "../user.h"

#define MAX_SNAKES 1024
#define MAX_PARTS 131072
#define MAX_FOODS 16384
static nb_bot bot;
static nb_snake snakes[MAX_SNAKES];
static nb_part parts[MAX_PARTS];
static nb_food foods[MAX_FOODS];
static bool running;
static float last_run;
static int stage=-1;

int ntl_bot_stage(void) { return stage; }
void ntl_bot_go(tenv* env) {
  game_data* g=&env->usr->gdata;
  snake* me=g->data.follow_view ? get_snake(g,g->data.snake_id) : NULL;
  if(!env->usr->usrs.hotkeys[HOTKEY_BOT].active || !me || me->dead) {
    running=false; stage=-1; g->bot.output.accel=false;
    return;
  }
  float now=g->data.ctm;
  if(!running) { nb_reset(&bot); running=true; last_run=now-1000; }   // switched on: start from "grow", as NTL does
  if(now-last_run<15 && now>=last_run) return;                        // at most about 60 decisions a second
  last_run=now;
  double started=perf_now();

  int count=tdarray_length(g->data.snakes),ns=0,np=0,mine=-1;
  for(int i=0;i<count && ns<MAX_SNAKES;i++) {
    const snake* o=g->data.snakes+i;
    if(o->id<0) continue;                    // already removed, only fading out
    int n=o->pts ? tdarray_length(o->pts) : 0;
    if(np+n>MAX_PARTS) n=MAX_PARTS-np;
    nb_snake* s=snakes+ns;
    *s=(nb_snake){o->id,o->xx,o->yy,o->fx,o->fy,o->ang,o->ehang,o->sp,o->sc,!o->dead && o->alive_amt>=1,parts+np,n};
    for(int k=0;k<n;k++) { const body_part* p=o->pts+k; parts[np++]=(nb_part){p->xx,p->yy,p->fx,p->fy,p->dying}; }
    if(o->id==g->data.snake_id) mine=ns;
    ns++;
  }
  if(mine<0) return;
  int nf=g->data.foods ? tdarray_length(g->data.foods) : 0;
  if(nf>MAX_FOODS) nf=MAX_FOODS;
  for(int i=0;i<nf;i++) { const food* f=g->data.foods+i; foods[i]=(nb_food){f->xx,f->yy,f->sz,f->eaten}; }

  int sct=me->sct;
  int length=(int)((g->data.fpsls[sct]+me->fam/g->data.fmlts[sct]-1)*15-5);
  // The player can still hold boost while the bot steers, as in NTL.
  bool typing=igGetIO_Nil()->WantTextInput;
  bool boost=!typing && (twindow_button_down(env->wnd,GLFW_MOUSE_BUTTON_LEFT) || twindow_key_down(env->wnd,GLFW_KEY_SPACE) ||
      twindow_key_down(env->wnd,GLFW_KEY_UP) || twindow_key_down(env->wnd,GLFW_KEY_LEFT_SHIFT) || twindow_key_down(env->wnd,GLFW_KEY_RIGHT_SHIFT));
  // A wall radius is "announced" when it differs from the usual 98% of the map.
  double wall=g->data.real_flux_grd;
  bool flux=wall>0 && fabs(wall-g->data.grd*.98)>1;
  nb_scene scene={snakes,ns,mine,foods,nf,g->data.grd,flux,wall,g->data.view_xx,g->data.view_yy,length,now,boost,
                  native_prefs.bot_smart,native_prefs.bot_length};
  nb_out out;
  nb_go(&bot,&scene,&out);
  stage=out.stage;
  if(out.has_goal) { g->bot.output.xm=(float)(out.goal_x-me->xx); g->bot.output.ym=(float)(out.goal_y-me->yy); }
  else { g->bot.output.xm=cosf(me->ang)*100; g->bot.output.ym=sinf(me->ang)*100; }   // nothing decided yet: straight on
  g->bot.output.accel=out.accel;
  perf_add(PERF_BOT,started);
}
