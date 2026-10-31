#ifndef TEAM_STATUS_H
#define TEAM_STATUS_H
#include "../network/ntl_protocol.h"
#include <string.h>
typedef struct {
  char name[257],server[161];
  int fps,ping,score;
  bool self,menu,same_server,sos;
} team_status_row;
typedef struct {
  team_status_row rows[NTL_MEMBERS];
  int count,servers,playing;
} team_status_view;
// Called once per NTL response, never copies chat history or measures text per frame.
static inline void team_status_build(team_status_view* view,const ntl_reply* reply,const char* own,const char* fallback) {
  memset(view,0,sizeof(*view));
  const char* current=fallback;
  for(int i=0;i<reply->count;i++) if(!strcmp(reply->rows[i].nick,own)) current=reply->rows[i].server;
  for(int i=0;i<reply->count && i<NTL_MEMBERS;i++) {
    const ntl_member* m=&reply->rows[i]; team_status_row* row=&view->rows[view->count++];
    ntl_plain(strlen(m->nick)>=8?m->nick+8:m->nick,row->name,sizeof(row->name));
    if(!row->name[0]) strcpy(row->name,"이름 없음");
    ntl_plain(m->server,row->server,sizeof(row->server));
    row->self=!strcmp(m->nick,own); row->menu=!strcmp(m->server,"_GAME_MENU_");
    row->same_server=!row->menu && !strcmp(m->server,current); row->sos=m->sos;
    row->score=m->score; row->fps=m->fps; row->ping=row->menu?-1:m->ping;
    if(!row->menu) {
      view->playing++; bool seen=false;
      for(int j=0;j<i;j++) if(!strcmp(reply->rows[j].server,m->server)) { seen=true; break; }
      if(!seen) view->servers++;
    }
  }
}
#endif
