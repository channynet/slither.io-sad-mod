#include "../app/src/ui/image_library.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void invalid(const char* data) {
  FILE* f=fopen("personal-tag-library.json","wb"); assert(f); fputs(data,f); fclose(f);
  image_library l; image_library_seed(&l,false);
  assert(image_library_load(&l)==-1 && l.read_only);
  assert(!image_library_save(&l)); assert(!image_library_add(&l,"new"));
  char check[512]={0}; f=fopen("personal-tag-library.json","rb"); fread(check,1,sizeof(check)-1,f); fclose(f);
  assert(!strcmp(check,data));
}
int main(void) {
  image_library tags,cos,restored;
  image_library_seed(&tags,false); image_library_seed(&cos,true);
  assert(tags.count==10 && cos.count==10);
  assert(image_library_load(&tags)==0);
  char a[256],b[256]; image_library_path(&tags,0,a,sizeof(a)); image_library_path(&cos,0,b,sizeof(b));
  assert(strcmp(a,b) && strstr(a,"tag-ruby-heart.png") && strstr(b,"cosmetic-raven-wings.png"));
  image_item* first=image_library_add(&tags,"내 이미지 \"A\" \\ 날개"); assert(first && first->id==100);
  first->size=173.5f; first->gap=42; first->angle=-73; tags.selected=10; tags.enabled=false;
  assert(image_library_save(&tags));
  image_library_seed(&restored,false); assert(image_library_load(&restored)==1);
  assert(restored.count==11 && restored.selected==10 && !restored.enabled);
  assert(!strcmp(restored.items[10].name,first->name));
  assert(restored.items[10].size==173.5f && restored.items[10].gap==42 && restored.items[10].angle==-73);
  image_item* second=image_library_add(&restored,"두 번째"); assert(second && second->id==101);
  second->size=55; restored.selected=11; assert(image_library_save(&restored));
  assert(image_library_load(&tags)==1 && tags.selected==11 && tags.items[10].size==173.5f && tags.items[11].size==55);
  tags.selected=10; assert(image_library_save(&tags)); assert(image_library_load(&restored)==1 && restored.selected==10);
  cos.items[3].gap=-80; cos.items[3].rotate=false; cos.selected=3; assert(image_library_save(&cos));
  image_library_seed(&cos,true); assert(image_library_load(&cos)==1 && cos.selected==3 && cos.items[3].gap==-80 && !cos.items[3].rotate);
  assert(image_library_load(&tags)==1 && tags.selected==10); /* separate catalog */
  while(tags.count<IMAGE_LIBRARY_MAX) assert(image_library_add(&tags,"more"));
  assert(!image_library_add(&tags,"overflow")); assert(image_library_save(&tags));
  assert(image_library_load(&restored)==1 && restored.count==IMAGE_LIBRARY_MAX);
  invalid("{broken");
  invalid("{\"version\":1,\"count\":65,\"selected\":1,\"enabled\":true,\"items\":[]}");
  invalid("{\"version\":1,\"count\":1,\"selected\":1,\"enabled\":true,\"items\":[{\"id\":1,\"name\":\"x\",\"size\":999,\"gap\":24,\"angle\":0,\"rotate\":false}]}");
  invalid("{\"version\":1,\"count\":1,\"selected\":2,\"enabled\":true,\"items\":[{\"id\":1,\"name\":\"x\",\"size\":96,\"gap\":24,\"angle\":0,\"rotate\":false}]}");
  puts("image_library: independent settings, Unicode names, selection restore, capacity, corrupt-file preservation passed");
  return 0;
}
