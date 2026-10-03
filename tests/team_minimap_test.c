#include "../app/src/ui/team_minimap.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  ntl_member m={.nick="ABCDEFGHmate",.server="test:444",.position=true,.x=10000,.y=10000};
  float p[2];
  assert(team_minimap_point(&m,"test:444","ABCDEFGHme",0,500,600,200,10000,4000,p));
  assert(fabsf(p[0]-600.5f)<.001f && fabsf(p[1]-700.5f)<.001f);
  m.x=14000;
  assert(team_minimap_point(&m,"test:444","ABCDEFGHme",10000,500,600,200,10000,4000,p));
  assert(fabsf(p[0]-691.4f)<.001f); // arena edge, including shader inset and softness
  m.x=10000; m.y=6000;
  assert(team_minimap_point(&m,"test:444","ABCDEFGHme",1,500,600,400,10000,4000,p));
  assert(fabsf(p[1]-619.6f)<.001f);
  m.x=10000; m.y=10000;
  assert(!team_minimap_point(&m,"test:444","ABCDEFGHme",10001,0,0,200,10000,4000,p));
  assert(!team_minimap_point(&m,"other:444","ABCDEFGHme",0,0,0,200,10000,4000,p));
  assert(!team_minimap_point(&m,"test:444",m.nick,0,0,0,200,10000,4000,p));
  assert(!team_minimap_point(&m,"test:444","me",0,0,0,200,10000,0,p));
  m.x=14001; assert(!team_minimap_point(&m,"test:444","me",0,0,0,200,10000,4000,p));
  m.x=NAN; assert(!team_minimap_point(&m,"test:444","me",0,0,0,200,10000,4000,p));
  m.x=10000;m.position=false;assert(!team_minimap_point(&m,"test:444","me",0,0,0,200,10000,4000,p));
  m.position=true;strcpy(m.server,"_GAME_MENU_");
  assert(!team_minimap_point(&m,"_GAME_MENU_","me",0,0,0,200,10000,4000,p));
  puts("Team minimap: shader-matched center/edge/resize, arena radius, same-server/self/stale/menu/invalid filters passed");
}
