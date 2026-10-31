#ifndef DUEL_RECORD_H
#define DUEL_RECORD_H
// Kills and deaths against each opponent, kept by nickname for as long as the program runs.
// Servers do not say who killed whom, so the game works it out from where a head was when
// its snake died (see duel_board.c); this file only keeps the tally.
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#define DUEL_NAME 32
#define DUEL_MAX 256
typedef struct { char name[DUEL_NAME]; int kills, deaths; uint64_t touched; } duel_entry;
typedef struct { duel_entry entries[DUEL_MAX]; int count; int other_deaths; uint64_t clock; } duel_table;

static inline duel_entry* duel_find(duel_table* t,const char* name) {
  char key[DUEL_NAME]={0}; strncpy(key,name && name[0] ? name : "(no name)",DUEL_NAME-1);
  for(int i=0;i<t->count;i++) if(!strcmp(t->entries[i].name,key)) return t->entries+i;
  int slot=t->count;
  if(slot<DUEL_MAX) t->count++;
  else {  // full: drop the opponent met least, oldest first
    slot=0;
    for(int i=1;i<DUEL_MAX;i++) {
      duel_entry* a=t->entries+i; duel_entry* b=t->entries+slot;
      int ta=a->kills+a->deaths,tb=b->kills+b->deaths;
      if(ta<tb || (ta==tb && a->touched<b->touched)) slot=i;
    }
  }
  memset(t->entries+slot,0,sizeof(duel_entry)); memcpy(t->entries[slot].name,key,DUEL_NAME);
  return t->entries+slot;
}
// Looks an opponent up without adding it. NULL when we never met.
static inline const duel_entry* duel_peek(const duel_table* t,const char* name) {
  char key[DUEL_NAME]={0}; strncpy(key,name && name[0] ? name : "(no name)",DUEL_NAME-1);
  for(int i=0;i<t->count;i++) if(!strcmp(t->entries[i].name,key)) return t->entries+i;
  return NULL;
}
static inline void duel_add(duel_table* t,const char* name,bool kill) {
  duel_entry* e=duel_find(t,name);
  if(kill) e->kills++; else e->deaths++;
  e->touched=++t->clock;
}
// Most-met opponents first, the latest first among equals. Returns how many were written.
static inline int duel_sorted(const duel_table* t,int* order,int max) {
  int n=0;
  for(int i=0;i<t->count;i++) {
    const duel_entry* e=t->entries+i; int total=e->kills+e->deaths,at=n<max ? n : max;
    for(int k=0;k<n && k<max;k++) {
      const duel_entry* o=t->entries+order[k]; int other=o->kills+o->deaths;
      if(total>other || (total==other && e->touched>o->touched)) { at=k; break; }
    }
    if(at>=max) continue;
    int last=n<max ? n : max-1;
    for(int k=last;k>at;k--) order[k]=order[k-1];
    order[at]=i; if(n<max) n++;
  }
  return n;
}
static inline float duel_segment_distance(float x,float y,float px,float py,float qx,float qy) {
  float dx=qx-px,dy=qy-py,ll=dx*dx+dy*dy,t=ll>0 ? ((x-px)*dx+(y-py)*dy)/ll : 0;
  if(t<0) t=0; if(t>1) t=1;
  return hypotf(px+dx*t-x,py+dy*t-y);
}
#endif
