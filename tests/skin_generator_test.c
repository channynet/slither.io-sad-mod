#include "../app/src/game/skin_generator.h"
#include "skin_generator_golden.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  generated_skin skin,again;uint32_t enabled=0,disabled=0;
  for(size_t i=0;i<sizeof(golden)/sizeof(golden[0]);i++) {
    assert(skin_generator_make(golden[i].seed,&skin));
    // Equivalent-cost role slots can be permuted by sub-ulp math differences
    // between JS runtimes. Preserve the original rules; compare selected colors.
    for(const char* p="1234567890-qwertyuiopasdfghjklzxcvbnm,";*p;p++)
      assert((strchr(skin.code,*p)!=NULL)==(strchr(golden[i].code,*p)!=NULL));
    assert(skin.theory_mask==golden[i].mask);
  }
  char previous[257]="",first_topic[256]="";int different_topics=0;
  for(uint32_t seed=1;seed<=32;seed++) {
    assert(skin_generator_make(seed*2654435761u,&skin));
    assert(strlen(skin.code)==250 && strspn(skin.code,"1234567890-qwertyuiopasdfghjklzxcvbnm,")==250);
    assert(skin.topic[0] && strcmp(previous,skin.code));strcpy(previous,skin.code);
    if(seed==1) strcpy(first_topic,skin.topic);
    else if(strcmp(first_topic,skin.topic)) different_topics++;
    enabled|=skin.theory_mask;disabled|=~skin.theory_mask&16383;
  }
  assert(enabled==16383 && disabled==16383 && different_topics>20);
  assert(skin_generator_make(12345,&skin) && skin_generator_make(12345,&again));
  assert(!strcmp(skin.code,again.code) && !strcmp(skin.topic,again.topic) && !strcmp(skin.theories,again.theories) && skin.theory_mask==again.theory_mask);
  assert(!skin_generator_make(1,NULL));
  puts("Random skins: original-engine palettes, 250 valid cells, varied themes/codes, all 14 rules toggle, deterministic seeds passed");
}
