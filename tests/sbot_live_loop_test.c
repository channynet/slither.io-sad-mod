#include "../app/src/user.h"
#include "../app/src/game/perf_trace.h"
#include "../app/src/game/body_loop.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <windows.h>
#define PI 3.14159265358979323846f
static tuser_data usr;
static def_vec trail[1024];
static int count,decisions;
double perf_now(void) { decisions++; return 0; }
void perf_add(int stage,double start) { (void)start; assert(stage==PERF_BOT); }
snake* get_snake(game_data* g,int id) { return g->data.snakes && g->data.snakes[0].id==id?g->data.snakes:NULL; }
static float distance(def_vec a,def_vec b) { return hypotf(a.x-b.x,a.y-b.y); }
static void advance(def_vec head,float body_length) {
  assert(count<1024);
  memmove(trail+1,trail,count*sizeof(*trail)); trail[0]=head; count++;
  float walked=0;
  for(int i=1;i<count;i++) {
    float length=distance(trail[i],trail[i-1]);
    if(walked+length>=body_length) {
      float t=(body_length-walked)/fmaxf(length,.001f);
      trail[i]=(def_vec){trail[i-1].x+(trail[i].x-trail[i-1].x)*t,
                        trail[i-1].y+(trail[i].y-trail[i-1].y)*t};
      count=i+1; return;
    }
    walked+=length;
  }
}
static void sync_body(snake* own,int stride) {
  tdarray_clear(own->pts);
  for(int i=count-1;i>=1;i-=stride) {
    body_part p={.xx=trail[i].x,.yy=trail[i].y}; tdarray_push(&own->pts,&p);
  }
}
int main(void) {
  tenv env={.usr=&usr}; game_data* g=&usr.gdata;
  g->data.snakes=tdarray_create(snake); g->data.foods=tdarray_create(food);
  snake me={.id=42,.generation=1,.sp=5.76f,.ssp=5.76f,.sc=1,.spang=1,.scang=1};
  me.pts=tdarray_create(body_part); tdarray_push(&g->data.snakes,&me);
  snake* own=g->data.snakes;
  g->data.snake_id=42; g->data.follow_view=true; g->data.mamu=3.0f/125;
  g->data.flux_grd=g->data.real_flux_grd=10000; g->data.ping=80;
  for(int stride=1;stride<=4;stride*=4) for(int sign=-1;sign<=1;sign+=2) for(int oval=0;oval<2;oval++) {
    sbot_init(&env); own->generation++;
    float rx=oval?220:170,ry=oval?140:170;
    count=300;
    for(int i=0;i<count;i++) {
      float a=PI/2-sign*i*2*PI/240;
      trail[i]=(def_vec){rx*cosf(a),ry*sinf(a)};
    }
    float initial_body=0;
    for(int i=1;i<count;i++) initial_body+=distance(trail[i-1],trail[i]);
    own->xx=trail[0].x; own->yy=trail[0].y; own->ang=sign>0?PI:0;
    float queue[16]; for(int i=0;i<16;i++) queue[i]=own->ang;
    float total_turn=0,min_overlap=100000,last_overlap=0,start_length=0,end_length=0;
    int closed=0,checked=0;
    usr.usrs.hotkeys[HOTKEY_BOT].active=true;
    // Live body follows every movement, with actual delayed steering. Grow by
    // 15% over 10 seconds; later remove 5% to exercise overlap recovery.
    for(int step=0;step<2000;step++) {
      sync_body(own,stride);
      float growth=1+.15f*fminf(1,fmaxf(0,(step-500)/250.0f));
      if(step>=1400) growth-=.05f;
      g->data.ctm=1000+step*40; g->data.ping=step%400<200?80:160;
      // A transient missing neck/body snapshot must preserve the last route.
      if(step>=1000 && step<1008) tdarray_clear(own->pts);
      sbot_go(&env);
      assert(!g->bot.output.accel);
      queue[step%16]=atan2f(g->bot.output.ym,g->bot.output.xm);
      int delay=step%400<200?2:4;
      float target=queue[(step+16-delay)%16];
      float delta=fmaxf(-.12f,fminf(.12f,def_wrap(target-own->ang)));
      own->xx+=cosf(own->ang+delta/2)*7.2f;
      own->yy+=sinf(own->ang+delta/2)*7.2f;
      own->ang+=delta; total_turn+=delta;
      assert(isfinite(own->xx) && isfinite(own->yy));
      advance((def_vec){own->xx,own->yy},initial_body*growth);
      if(step%3==0) {
        body_loop actual; checked++;
        if(body_loop_capture(&actual,trail,count,own->ang,14.5f,60)) {
          closed++; min_overlap=fminf(min_overlap,actual.overlap);
          if(!start_length) start_length=actual.length;
          if(step>1750) { last_overlap=actual.overlap; end_length=actual.length; }
          assert(actual.direction==sign);
        }
      }
    }
    printf("Live %s sign %d stride %d: closed %d/%d, min overlap %.1f, final %.1f, lap %.1f -> %.1f, turns %.1f\n",
           oval?"oval":"circle",sign,stride,closed,checked,min_overlap,last_overlap,start_length,end_length,total_turn/(2*PI));
    fflush(stdout);
    assert(closed>checked*.95f && sign*total_turn>50);
    assert(min_overlap>-25 && last_overlap>20 && last_overlap<240);
    assert(end_length>start_length*1.10f);
    int before=decisions;
    usr.usrs.hotkeys[HOTKEY_BOT].active=false;
    for(int i=0;i<500;i++) { g->data.ctm+=40; sbot_go(&env); }
    assert(decisions==before);
  }
  puts("Live enclosure: both directions, circles/ovals, dense/sparse body updates, growth, shrinking length, 80/160ms steering delay, missing body snapshot and zero OFF planning passed");
}
