#include "../app/src/user.h"
#include "../app/src/game/perf_trace.h"
#include "../app/src/game/body_loop.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#define PI 3.14159265358979323846f
static tuser_data usr;
static def_vec trail[1024];
static int count;
double perf_now(void) { return 0; }
void perf_add(int stage,double start) { (void)start; assert(stage==PERF_BOT); }
snake* get_snake(game_data* g,int id) { return g->data.snakes[0].id==id?g->data.snakes:NULL; }
static float distance(def_vec a,def_vec b) { return hypotf(a.x-b.x,a.y-b.y); }
static float area(const body_loop* loop) {
  float sum=0;
  for(int i=0;i<loop->count;i++) {
    def_vec a=loop->points[i],b=loop->points[(i+1)%loop->count];
    a.x-=loop->points[0].x; a.y-=loop->points[0].y;
    b.x-=loop->points[0].x; b.y-=loop->points[0].y;
    sum+=a.x*b.y-a.y*b.x;
  }
  return fabsf(sum)*.5f;
}
static void advance(def_vec head,float body_length) {
  assert(count<1024); memmove(trail+1,trail,count*sizeof(*trail)); trail[0]=head; count++;
  float walked=0;
  for(int i=1;i<count;i++) {
    float length=distance(trail[i],trail[i-1]);
    if(walked+length>=body_length) {
      float t=(body_length-walked)/fmaxf(length,.001f);
      trail[i]=(def_vec){trail[i-1].x+(trail[i].x-trail[i-1].x)*t,trail[i-1].y+(trail[i].y-trail[i-1].y)*t};
      count=i+1; return;
    }
    walked+=length;
  }
}
int main(void) {
  tenv env={.usr=&usr}; game_data* g=&usr.gdata;
  g->data.snakes=tdarray_create(snake); g->data.foods=tdarray_create(food);
  snake me={.id=42,.generation=1,.sp=5.76f,.ssp=5.76f,.sc=1,.spang=1,.scang=1};
  me.pts=tdarray_create(body_part); tdarray_push(&g->data.snakes,&me); snake* own=g->data.snakes;
  g->data.snake_id=42; g->data.follow_view=true; g->data.mamu=3.0f/125;
  g->data.flux_grd=g->data.real_flux_grd=10000;
  usr.usrs.hotkeys[HOTKEY_BOT].active=true;
  for(int size=1;size<=6;size+=(size==1?2:3)) for(int delay=2;delay<=4;delay+=2) for(int sign=-1;sign<=1;sign+=2) for(int oval=0;oval<2;oval++) {
    sbot_init(&env); own->generation++; count=300;
    own->sc=size; own->sp=own->ssp=4.25f+.5f*size;
    own->scang=.13f+.87f*powf((7-size)/6.0f,2); g->data.mamu=.033f;
    float speed=own->sp*31.25f,rate=g->data.mamu*own->scang*125;
    float radius=14.5f*size, minimum=speed/rate;
    float rx=(oval?220:170)*size,ry=(oval?140:170)*size,body_length=0;
    for(int i=0;i<count;i++) {
      float a=PI/2-sign*i*2*PI/240;
      trail[i]=(def_vec){rx*cosf(a),ry*sinf(a)};
      if(i) body_length+=distance(trail[i-1],trail[i]);
    }
    own->xx=trail[0].x; own->yy=trail[0].y; own->ang=sign>0?PI:0;
    g->data.ping=delay*40;
    float queue[16]; for(int i=0;i<16;i++) queue[i]=own->ang;
    body_loop initial; assert(body_loop_capture(&initial,trail,count,own->ang,radius,minimum));
    float min_lap=initial.length,max_lap=initial.length,final_lap=0,total_turn=0;
    float initial_area=area(&initial),min_area=initial_area;
    int closed=0;
    for(int step=0;step<4500;step++) { // Three minutes, fixed body length, no enemies.
      tdarray_clear(own->pts);
      for(int i=count-1;i>=1;i--) {
        body_part p={.xx=trail[i].x,.yy=trail[i].y}; tdarray_push(&own->pts,&p);
      }
      g->data.ctm=1000+step*40; sbot_go(&env); assert(!g->bot.output.accel);
      queue[step%16]=atan2f(g->bot.output.ym,g->bot.output.xm);
      float target=queue[(step+16-delay)%16];
      float delta=fmaxf(-rate*.04f,fminf(rate*.04f,def_wrap(target-own->ang)));
      own->xx+=cosf(own->ang+delta/2)*speed*.04f; own->yy+=sinf(own->ang+delta/2)*speed*.04f;
      own->ang+=delta; total_turn+=delta;
      advance((def_vec){own->xx,own->yy},body_length);
      if(step%10==0) {
        body_loop actual;
        if(body_loop_capture(&actual,trail,count,own->ang,radius,minimum)) {
          closed++; min_lap=fminf(min_lap,actual.length); max_lap=fmaxf(max_lap,actual.length); final_lap=actual.length;
          min_area=fminf(min_area,area(&actual));
          assert(actual.direction==sign && actual.overlap>20);
        }
      }
    }
    printf("Size hold scale %d %s sign %d delay %dms: closed %d/450, lap ratio min %.4f max %.4f final %.4f, min area %.4f, turns %.1f\n",
      size,oval?"oval":"circle",sign,delay*40,closed,min_lap/initial.length,max_lap/initial.length,final_lap/initial.length,min_area/initial_area,total_turn/(2*PI)); fflush(stdout);
    assert(closed>=445 && sign*total_turn>20);
    assert(min_lap>=initial.length*.97f && max_lap<=initial.length*1.06f);
    assert(min_area>=initial_area*.95f);
  }
  puts("Quiet enclosure: 24 three-minute live-body scenarios, sizes 1/3/6, both directions and 80/160ms command delay passed");
}
