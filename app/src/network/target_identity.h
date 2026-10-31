#ifndef TARGET_IDENTITY_H
#define TARGET_IDENTITY_H
#include "ntl_protocol.h"
#include <stdio.h>
#include <stdint.h>
// Wire identity, independent of our renderer's skin remapping/truncation.
static inline void target_identity(char out[NTL_TARGET],int id,unsigned skin,
    const uint8_t* custom,int bytes,const uint8_t defaults[66][64]) {
  unsigned first=0,length=0;
  if(custom && bytes>=10) {
    for(int i=8;i+1<bytes;i+=2) {
      unsigned color=custom[i+1];
      // NTL excludes the three unused color slots.
      if(color>41 || color==36 || color==38 || color==40 || !custom[i]) continue;
      if(!length) first=color;
      length+=custom[i];
    }
    if(custom[1]==18 || custom[1]==38) {
      skin=custom[2]==255?custom[9]:custom[2];
      if(custom[2]!=255) length=0;
    }
  }
  if(!length && skin<66) { length=defaults[skin][0]; first=defaults[skin][1]; }
  if(!length && skin>=66 && skin<248) {
    unsigned slot=skin-66,row=slot/13,column=slot%13;
    if(column>=row) column++;
    first=column%2?16:25; length=1;
  }
  if(!length && skin>=248 && skin<=255) { first=(skin==253 || skin==254)?16:26; length=1; }
  if(length) snprintf(out,NTL_TARGET,"%u_%d_%u",skin,id,first+length);
  else snprintf(out,NTL_TARGET,"%u_%d_",skin,id);
}
#endif
