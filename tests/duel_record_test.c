#include "../app/src/network/duel_record.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  static duel_table t; int order[4];
  assert(duel_sorted(&t,order,4)==0);
  duel_add(&t,"ann",true); duel_add(&t,"bob",false); duel_add(&t,"ann",false); duel_add(&t,"",true);
  assert(t.count==3);
  assert(duel_peek(&t,"ann") && duel_peek(&t,"ann")->kills==1 && !duel_peek(&t,"zed") && t.count==3);
  assert(duel_peek(&t,"")==duel_peek(&t,NULL) && duel_peek(&t,"")->kills==1);
  duel_entry* ann=duel_find(&t,"ann"); assert(ann->kills==1 && ann->deaths==1);
  assert(duel_find(&t,"bob")->deaths==1 && duel_find(&t,"bob")->kills==0);
  assert(duel_find(&t,NULL)==duel_find(&t,"") && duel_find(&t,"")->kills==1 && !strcmp(duel_find(&t,"")->name,"(no name)"));
  // most met first; among equals the latest
  assert(duel_sorted(&t,order,4)==3 && !strcmp(t.entries[order[0]].name,"ann") && !strcmp(t.entries[order[1]].name,"(no name)") && !strcmp(t.entries[order[2]].name,"bob"));
  assert(duel_sorted(&t,order,2)==2 && !strcmp(t.entries[order[0]].name,"ann") && !strcmp(t.entries[order[1]].name,"(no name)"));
  // long names are cut, not overrun
  duel_add(&t,"0123456789012345678901234567890123456789",true);
  assert(strlen(duel_find(&t,"0123456789012345678901234567890123456789")->name)==DUEL_NAME-1 && t.count==4);
  // a full table drops the least met, oldest entry and keeps the others
  for(int i=0;i<DUEL_MAX+40;i++) { char name[16]; snprintf(name,sizeof(name),"p%d",i); duel_add(&t,name,true); }
  assert(t.count==DUEL_MAX);
  int found=0; for(int i=0;i<t.count;i++) if(!strcmp(t.entries[i].name,"ann")) found=t.entries[i].kills+t.entries[i].deaths;
  assert(found==2);
  // geometry: beside the middle, past either end, and a zero-length segment
  assert(fabsf(duel_segment_distance(5,3,0,0,10,0)-3)<1e-4f);
  assert(fabsf(duel_segment_distance(-4,3,0,0,10,0)-5)<1e-4f);
  assert(fabsf(duel_segment_distance(13,4,0,0,10,0)-5)<1e-4f);
  assert(fabsf(duel_segment_distance(3,4,0,0,0,0)-5)<1e-4f);
  puts("duel record tests passed");
  return 0;
}
