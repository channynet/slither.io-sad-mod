#include "../app/src/network/target_identity.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
  uint8_t defaults[66][64]={[0]={1,0},[9]={20,7},[65]={8,4}};
  char out[NTL_TARGET];
  target_identity(out,123,9,NULL,0,defaults); assert(!strcmp(out,"9_123_27"));
  uint8_t custom[]={255,18,255,0,0,0,0,0,5,7,4,9};
  target_identity(out,123,9,custom,sizeof(custom),defaults); assert(!strcmp(out,"7_123_16"));
  custom[2]=65; target_identity(out,123,9,custom,sizeof(custom),defaults); assert(!strcmp(out,"65_123_12"));
  custom[1]=0; target_identity(out,123,9,custom,sizeof(custom),defaults); assert(!strcmp(out,"9_123_16"));
  // Full wire pattern length survives the renderer's 256-ring limit.
  custom[8]=255; custom[10]=255; target_identity(out,65535,9,custom,sizeof(custom),defaults);
  assert(!strcmp(out,"9_65535_517"));
  target_identity(out,1,66,NULL,0,defaults); assert(!strcmp(out,"66_1_17"));
  target_identity(out,1,255,NULL,0,defaults); assert(!strcmp(out,"255_1_27"));
  puts("TMS identity: stock/custom/NTL headers, full pattern, server IDs and new built-in skins passed");
}
