#ifndef TEAM_MEMBERSHIP_H
#define TEAM_MEMBERSHIP_H
#include "ntl_protocol.h"
#include <stdint.h>
#include <string.h>
typedef struct { uint8_t ids[65536/8]; } team_membership;
static inline void team_membership_build(team_membership* cache,const ntl_reply* reply,
                                         const char* identity,const char* fallback_server) {
  memset(cache,0,sizeof(*cache));
  const char* server=fallback_server;
  for(int i=0;i<reply->count;i++)
    if(!strcmp(reply->rows[i].nick,identity)) server=reply->rows[i].server;
  for(int i=0;i<reply->count;i++) {
    int sid=reply->rows[i].sid;
    if(sid>=0 && sid<=65535 && !strcmp(reply->rows[i].server,server))
      cache->ids[sid>>3]|=(uint8_t)(1u<<(sid&7));
  }
}
static inline bool team_membership_has(const team_membership* cache,int sid) {
  return sid>=0 && sid<=65535 && (cache->ids[sid>>3] & (1u<<(sid&7)))!=0;
}
#endif
