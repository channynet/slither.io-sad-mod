#ifndef TEAM_MINIMAP_H
#define TEAM_MINIMAP_H
#include "../network/ntl_protocol.h"
#include <math.h>
#include <stdint.h>
#include <string.h>
static inline bool team_minimap_point(const ntl_member* member,const char* server,const char* own,
    uint64_t age_ms,float origin_x,float origin_y,float diameter,float center,float radius,float out[2]) {
  if(age_ms>10000 || !member->position || !server[0] || !strcmp(server,"_GAME_MENU_") ||
     strcmp(member->server,server) || !strcmp(member->nick,own) ||
     !isfinite(center) || !isfinite(radius) || radius<=0 || !isfinite(diameter) || diameter<=0 ||
     !isfinite(member->x) || !isfinite(member->y)) return false;
  double nx=(member->x-center)/radius,ny=(member->y-center)/radius;
  if(nx*nx+ny*ny>1.0) return false;
  // Match mm.slang: EDGE_SOFTNESS=2, SHADOW=.9, map is centered on grd,
  // but scaled by flux_grd (the current arena boundary), not by grd.
  out[0]=origin_x+diameter*.5f+.5f+(float)nx*(diameter+2)*.45f;
  out[1]=origin_y+diameter*.5f+.5f+(float)ny*(diameter+2)*.45f;
  return true;
}
#endif
