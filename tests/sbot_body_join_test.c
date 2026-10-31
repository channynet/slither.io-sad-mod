#include "../app/src/user.h"
#include "../app/src/game/body_loop.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#define PI 3.14159265358979323846f
static tuser_data usr;
static def_vec trail[4096];
static int count;
double perf_now(void) { return 0; }
void perf_add(int stage,double start) { (void)stage; (void)start; }
snake* get_snake(game_data* g,int id) { return g->data.snakes[0].id==id?g->data.snakes:NULL; }
static float distance(def_vec a,def_vec b) { return hypotf(a.x-b.x,a.y-b.y); }
static void advance(def_vec head,float length) {
  assert(count<4096);
  memmove(trail+1,trail,count*sizeof(*trail)); trail[0]=head; count++;
  float walked=0;
  for(int i=1;i<count;i++) {
    float d=distance(trail[i-1],trail[i]);
    if(walked+d>=length) {
      float t=(length-walked)/fmaxf(.001f,d);
      trail[i]=(def_vec){trail[i-1].x+(trail[i].x-trail[i-1].x)*t,
                        trail[i-1].y+(trail[i].y-trail[i-1].y)*t};
      count=i+1; return;
    }
    walked+=d;
  }
}
int main(void) {
  tenv env={.usr=&usr}; game_data* g=&usr.gdata;
  g->data.snakes=tdarray_create(snake); g->data.foods=tdarray_create(food);
  snake me={.id=42,.generation=1,.spang=1}; me.pts=tdarray_create(body_part);
  tdarray_push(&g->data.snakes,&me); snake* own=g->data.snakes;
  g->data.snake_id=42; g->data.follow_view=true; g->data.mamu=.033f;
  g->data.grd=20000; g->data.flux_grd=g->data.real_flux_grd=18000;
  for(int scale=1;scale<=6;scale+=(scale==1?2:3)) for(int sign=-1;sign<=1;sign+=2)
  for(int delay=2;delay<=4;delay+=2) for(int initial=205;initial<=229;initial+=24) for(int oval=0;oval<2;oval++) {
    sbot_init(&env); own->generation++; own->sc=scale;
    own->sp=own->ssp=4.25f+.5f*scale;
    own->scang=.13f+.87f*powf((7-scale)/6.0f,2);
    float speed=own->sp*31.25f,rate=g->data.mamu*own->scang*125;
    float radius=14.5f*scale,rx=170*scale;
    count=initial;
    for(int i=0;i<count;i++) {
      float a=-sign*i*2*PI/240;
      trail[i]=(def_vec){20000+rx*cosf(a),20000+rx*(oval?.8f:1)*sinf(a)};
    }
    own->xx=trail[0].x; own->yy=trail[0].y; own->ang=sign*PI/2;
    body_loop path;
    if(!oval || initial==205) assert(!body_loop_capture(&path,trail,count,own->ang,radius,speed/rate));
    assert(body_loop_capture_follow(&path,trail,count,own->ang,radius,speed/rate));
    assert(path.open && path.direction==sign);
    assert(body_loop_offset(&path,100000,radius,speed,.16f)==0);
    assert(!body_loop_capture_follow(&path,trail,count,own->ang+PI,radius,speed/rate));
    def_vec saved=trail[40]; trail[40].x+=10000;
    assert(!body_loop_capture_follow(&path,trail,count,own->ang,radius,speed/rate));
    trail[40]=saved;
    float length=0; for(int i=1;i<count;i++) length+=distance(trail[i-1],trail[i]);
    float commands[16]; for(int i=0;i<16;i++) commands[i]=own->ang;
    float turns=0; int closed=0,checks=0;
    int steps=(int)(4.5f*2*PI*rx/speed/.04f);
    usr.usrs.hotkeys[HOTKEY_BOT].active=true;
    for(int step=0;step<steps;step++) {
      tdarray_clear(own->pts);
      // Coarse body updates, as in the network's tail-to-neck representation.
      for(int i=count-1;i>=1;i-=3) {
        body_part p={.xx=trail[i].x,.yy=trail[i].y}; tdarray_push(&own->pts,&p);
      }
      g->data.ctm=1000+step*40; g->data.ping=delay*40;
      sbot_go(&env); assert(!g->bot.output.accel);
      commands[step%16]=atan2f(g->bot.output.ym,g->bot.output.xm);
      float d=def_wrap(commands[(step+16-delay)%16]-own->ang);
      d=fmaxf(-rate*.04f,fminf(rate*.04f,d));
      own->xx+=cosf(own->ang+d/2)*speed*.04f;
      own->yy+=sinf(own->ang+d/2)*speed*.04f;
      own->ang+=d; turns+=d;
      advance((def_vec){own->xx,own->yy},length);
      if(step>steps/2 && step%3==0) {
        checks++;
        if(body_loop_capture(&path,trail,count,own->ang,radius,speed/rate)) closed++;
      }
    }
    printf("Join oval %d initial %d scale %d direction %d delay %dms: %.2f turns, closed %d/%d\n",oval,initial,scale,sign,delay*40,sign*turns/(2*PI),closed,checks);
    fflush(stdout);
    assert(sign*turns>4*2*PI && closed>checks*.9f);
  }
  // Never invent a closing segment or turn back at the open path's endpoint.
  body_loop open={.points={{0,0},{100,0},{200,0}},.count=3,.length=200,.direction=1,.open=true};
  assert(fabsf(body_loop_heading(&open,(def_vec){199,0},0,50))<.001f);
  open.points[1]=(def_vec){0,200}; open.points[2]=(def_vec){200,200};
  assert(fabsf(body_loop_distance(&open,(def_vec){100,100})-100)<.001f);
  for(int i=0;i<100;i++) trail[i]=(def_vec){-i*5,0};
  assert(!body_loop_capture_follow(&open,trail,100,0,14.5f,60));
  puts("Body joining: partial laps, realistic sizes/turn rates, both directions, sparse body and steering delay passed");
}
