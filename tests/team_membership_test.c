#include "../app/src/network/team_membership.h"
#include <assert.h>
#include <stdio.h>
static ntl_reply reply;
static bool reference(int sid,const char* identity,const char* fallback) {
  if(sid<0 || sid>65535) return false;
  const char* server=fallback;
  for(int i=0;i<reply.count;i++) if(!strcmp(reply.rows[i].nick,identity)) server=reply.rows[i].server;
  for(int i=0;i<reply.count;i++) if(reply.rows[i].sid==sid && !strcmp(reply.rows[i].server,server)) return true;
  return false;
}
int main(void) {
  team_membership cache;
  reply.count=NTL_MEMBERS;
  for(int i=0;i<reply.count;i++) {
    snprintf(reply.rows[i].nick,257,"member%d",i);
    snprintf(reply.rows[i].server,161,"server%d",i%3);
    reply.rows[i].sid=(i*521)%65536;
  }
  reply.rows[0].sid=0; reply.rows[1].sid=65535;
  reply.rows[2].sid=-1; reply.rows[3].sid=65536;
  for(int pass=0;pass<3;pass++) {
    const char* identity=pass==0 ? "missing" : "member7";
    if(pass==2) strcpy(reply.rows[100].nick,"member7"); // Preserve last matching identity semantics.
    team_membership_build(&cache,&reply,identity,"server0");
    for(int sid=-1;sid<=65536;sid++) assert(team_membership_has(&cache,sid)==reference(sid,identity,"server0"));
  }
  reply.count=0; team_membership_build(&cache,&reply,"member7","server0");
  for(int sid=0;sid<65536;sid++) assert(!team_membership_has(&cache,sid));
  puts("Team membership: full ID range, server aliases, missing identity and snapshot removal match the original lookup");
}
