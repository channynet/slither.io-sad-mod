#include "../app/src/game/user_settings.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  // A settings file as this version writes it.
  user_settings now; memset(&now,0,sizeof(now));
  strcpy(now.version,"1.3"); strcpy(now.nickname,"player"); strcpy(now.ipv4,"some-words-here.trycloudflare.com");
  strcpy(now.skin_code,"abc"); now.accessory=7; now.custom_skin=true; now.default_skin=3; now.score=12345; now.play_time=6789.5; now.kills=42;
  now.cursor_size=31; now.vsync=true; now.zoom_step=.125f; now.modes[1].food_scale=2.5f; now.modes[1].food_color[2]=.75f;
  now.hotkeys[NUM_HOTKEYS-1].key=77; now.hotkeys[NUM_HOTKEYS-1].active=true; strcpy(now.hotkeys[NUM_HOTKEYS-1].description,"last");
  user_settings read; memset(&read,0xEE,sizeof(read));
  assert(user_settings_from_file(&read,(const unsigned char*)&now,sizeof(now)) && !memcmp(&read,&now,sizeof(now)));

  // The same settings as 0.50 and earlier wrote them: the address field 48 bytes shorter.
  const size_t address=offsetof(user_settings,ipv4),grown=MAX_IPV4_LEN-OLD_IPV4_LEN;
  strcpy(now.ipv4,"141.164.36.225:444"); memset(now.ipv4+19,0,sizeof(now.ipv4)-19);
  unsigned char old[sizeof(user_settings)]; size_t old_size=sizeof(user_settings)-grown;
  memcpy(old,&now,address+OLD_IPV4_LEN+1);
  memcpy(old+address+OLD_IPV4_LEN+1,(const unsigned char*)&now+address+MAX_IPV4_LEN+1,sizeof(now)-address-MAX_IPV4_LEN-1);
  assert(old_size==1168);                       // the size of the files in use
  memset(&read,0xEE,sizeof(read));
  assert(user_settings_from_file(&read,old,old_size));
  assert(!memcmp(&read,&now,sizeof(now)));      // every field where it belongs, nothing lost
  assert(!strcmp(read.ipv4,"141.164.36.225:444") && read.kills==42 && read.play_time==6789.5 && read.hotkeys[NUM_HOTKEYS-1].key==77);

  // Anything else is not a settings file.
  assert(!user_settings_from_file(&read,old,old_size-1) && !user_settings_from_file(&read,old,0) && !user_settings_from_file(&read,old,sizeof(now)+1));
  puts("user settings file test passed");
  return 0;
}
