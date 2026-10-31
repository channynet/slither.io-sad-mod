#include "../app/src/ui/team_status.h"
#include <assert.h>
#include <stdio.h>
static ntl_reply reply;
static team_status_view view;
int main(void) {
  const char* data="[{\"nick\":\"SELF0001me\",\"srv\":\"alias\",\"dt\":\"FPS: 120 @ 42 ms\"},{\"nick\":\"TEAM0001<b>same</b>\",\"srv\":\"alias\",\"dt\":\"FPS: 60 @ 80(100) ms\",\"sos\":true},{\"nick\":\"TEAM0002other\",\"srv\":\"other\"},{\"nick\":\"TEAM0003menu\",\"srv\":\"_GAME_MENU_\",\"dt\":\"FPS: 60 @ 500 ms\"}]";
  assert(ntl_parse(data,"SELF0001me",&reply)==0);
  team_status_build(&view,&reply,"SELF0001me","address");
  assert(view.count==4 && view.playing==3 && view.servers==2);
  assert(view.rows[0].self && view.rows[0].same_server && view.rows[0].fps==120 && view.rows[0].ping==42);
  assert(!strcmp(view.rows[1].name,"same") && view.rows[1].same_server && view.rows[1].sos);
  assert(!view.rows[2].same_server && view.rows[2].fps==-1 && view.rows[2].ping==-1);
  assert(view.rows[3].menu && !view.rows[3].same_server && view.rows[3].ping==-1);
  reply.count=0; team_status_build(&view,&reply,"SELF0001me","address");
  assert(!view.count&&!view.servers&&!view.playing);
  puts("Team status: server grouping/count, alias/self, name sanitization, SOS, missing telemetry, menu ping and empty snapshot passed");
}
