// Synthetic cost of the complete production BOT path, including world scanning.
// Static scenes measure CPU cost only; they do not establish survival rates.
#include "../app/src/user.h"
#include "../app/src/game/perf_trace.h"
#include "../app/src/game/defensive_planner.h"
#include <windows.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static tuser_data usr;
static LARGE_INTEGER frequency;
static int decisions;
static double planner_total;
double perf_now(void) { LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart*1000.0/frequency.QuadPart; }
def_result __real_def_choose(const def_scene* scene);
def_result __wrap_def_choose(const def_scene* scene) {
  double start=perf_now(); def_result r=__real_def_choose(scene);
  planner_total+=perf_now()-start; return r;
}
void perf_add(int stage,double start) { (void)start; assert(stage==PERF_BOT); decisions++; }
snake* get_snake(game_data* g,int id) {
  for(int i=0;i<tdarray_length(g->data.snakes);i++) if(g->data.snakes[i].id==id) return g->data.snakes+i;
  return NULL;
}
static int compare(const void* a,const void* b) { double x=*(const double*)a,y=*(const double*)b; return (x>y)-(x<y); }
int main(void) {
  QueryPerformanceFrequency(&frequency);
  int enemies[]={8,32,64},parts[]={256,1024,4096};
  puts("[");
  for(int c=0;c<3;c++) for(int closed=0;closed<2;closed++) {
    memset(&usr,0,sizeof(usr)); tenv env={.usr=&usr}; game_data* g=&usr.gdata;
    g->data.snakes=tdarray_create(snake); g->data.foods=tdarray_create(food);
    snake own={.id=1,.generation=1,.xx=120,.ang=1.570796327f,.sp=5.76f,.ssp=5.76f,.sc=1,.spang=1,.scang=1};
    own.pts=tdarray_create(body_part);
    if(closed) for(int j=299;j>=1;j--) {
      float a=-j*6.283185307f/240;
      body_part p={.xx=120*cosf(a),.yy=120*sinf(a)}; tdarray_push(&own.pts,&p);
    }
    tdarray_push(&g->data.snakes,&own);
    for(int i=0;i<enemies[c];i++) {
      float radius=400+i*20;
      snake other={.id=i+2,.generation=2,.xx=radius,.sc=1}; other.pts=tdarray_create(body_part);
      for(int j=parts[c]-1;j>=0;j--) {
        float a=-j*6.283185307f/parts[c];
        body_part p={.xx=radius*cosf(a),.yy=radius*sinf(a)}; tdarray_push(&other.pts,&p);
      }
      tdarray_push(&g->data.snakes,&other);
    }
    g->data.snake_id=1; g->data.follow_view=true; g->data.mamu=3.0f/125;
    g->data.flux_grd=g->data.real_flux_grd=10000; g->data.ping=50;
    sbot_init(&env); decisions=0;
    double start=perf_now();
    for(int i=0;i<10000;i++) { g->data.ctm+=16; sbot_go(&env); }
    double off=(perf_now()-start)/10000; assert(decisions==0);
    usr.usrs.hotkeys[HOTKEY_BOT].active=true;
    for(int i=0;i<30;i++) { g->data.ctm+=40; sbot_go(&env); }
    decisions=0; planner_total=0; double times[200],sum=0;
    for(int i=0;i<200;i++) {
      g->data.ctm+=40; start=perf_now(); sbot_go(&env); times[i]=perf_now()-start; sum+=times[i];
      assert(!g->bot.output.accel);
    }
    assert(decisions==200); qsort(times,200,sizeof(double),compare);
    printf("%s{\"enemy_snakes\":%d,\"parts_each\":%d,\"own_closed_body\":%s,\"off_mean_ms\":%.6f,\"on_mean_ms\":%.6f,\"planner_mean_ms\":%.6f,\"non_planner_mean_ms\":%.6f,\"on_p95_ms\":%.6f,\"on_p99_ms\":%.6f,\"on_max_ms\":%.6f}",c||closed?",\n":"",enemies[c],parts[c],closed?"true":"false",off,sum/200,planner_total/200,(sum-planner_total)/200,times[189],times[197],times[199]);
    for(int i=0;i<tdarray_length(g->data.snakes);i++) tdarray_destroy(g->data.snakes[i].pts);
    tdarray_destroy(g->data.snakes); tdarray_destroy(g->data.foods); sbot_destroy(&env);
  }
  puts("]"); return 0;
}
