#include "../app/src/game/skin_library.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  skin_library l,loaded;
  assert(skin_library_load(&l,"skins.json") && !l.count);
  skin_design d={.custom=true,.accessory=255,.fire=true,.fire_strength=.8f,.gradient_count=2,.tag_id=-1,
    .images={{.id=1,.enabled=true,.size=96,.gap=24,.sway=true},{.id=2,.enabled=true,.size=160,.gap=-18,.rotate=true}}};
  strcpy(d.code,"9,abc");d.colors[0][0]=1;d.colors[1][2]=1;
  for(int i=0;i<4;i++) d.look[i]=1;
  assert(skin_library_remember(&l,"skins.json",&d)==0);
  assert(skin_library_remember(&l,"skins.json",&d)==0 && l.count==1);
  d.images[1].angle=45;d.fire=false;d.gradient=true;
  assert(skin_library_remember(&l,"skins.json",&d)==1);
  assert(skin_library_load(&loaded,"skins.json") && loaded.count==2);
  assert(!strcmp(loaded.items[0].code,"9,abc") && loaded.items[0].fire);
  assert(loaded.items[1].gradient && loaded.items[1].images[1].angle==45 && loaded.items[1].images[1].id==2);
  assert(skin_library_remember(&loaded,"skins.json",&d)==1);
  FILE* f=fopen("broken.json","wb");fputs("not json",f);fclose(f);
  assert(!skin_library_load(&loaded,"broken.json") && loaded.read_only);
  assert(skin_library_remember(&loaded,"broken.json",&d)==-1);
  char bytes[20]={0};f=fopen("broken.json","rb");fread(bytes,1,19,f);fclose(f);assert(!strcmp(bytes,"not json"));
  assert(skin_library_remember(&l,"missing/fail.json",&d)==1); // existing item needs no write
  d.images[1].angle=90;
  assert(skin_library_remember(&l,"missing/fail.json",&d)==-1 && l.count==2);
  for(int kind=2;kind<=6;kind++) {
    d.special_skin=kind;d.fire=false;d.fire_strength=.6f;
    int slot=skin_library_remember(&l,"skins.json",&d);
    assert(slot==kind);
    assert(skin_library_load(&loaded,"skins.json"));
    assert(loaded.items[slot].special_skin==kind && loaded.items[slot].fire_strength==.6f);
    assert(skin_library_remember(&loaded,"skins.json",&d)==slot);
  }
  d.special_skin=7;
  assert(skin_library_remember(&l,"skins.json",&d)==-1);
  puts("Skin library: round trip, full appearance, deduplication, corrupt-file preservation and write failure passed");
}
