#include "../app/src/game/name_layout.h"
#include <assert.h>
static int measurements;
static float scale=1;
static float measure(const char* text) { measurements++; return (float)strlen(text)*scale; }
int main(void) {
  name_layout c={0}; int font1,font2;
  name_layout_update(&c,"player",1000,true,&font1,12,measure);
  assert(measurements==2 && c.name_width==6 && c.total_width==11 && !strcmp(c.score_text," 1.0K"));
  for(int i=0;i<600;i++) name_layout_update(&c,"player",1000,true,&font1,12,measure);
  assert(measurements==2);
  name_layout_update(&c,"player",1001,true,&font1,12,measure); assert(measurements==2);
  name_layout_update(&c,"player",1100,true,&font1,12,measure); assert(measurements==3 && !strcmp(c.score_text," 1.1K"));
  name_layout_update(&c,"renamed",1100,true,&font1,12,measure); assert(measurements==5 && c.name_width==7);
  scale=2; name_layout_update(&c,"renamed",1100,true,&font1,24,measure); assert(measurements==7 && c.name_width==14);
  name_layout_update(&c,"renamed",1100,true,&font2,24,measure); assert(measurements==9);
  name_layout_update(&c,"hidden score",5000,false,&font2,24,measure); assert(measurements==10);
  name_layout_update(&c,"hidden score",999999,true,&font2,24,measure); assert(measurements==11 && !strcmp(c.score_text," 1000.0K"));
  puts("Name layout: 600 unchanged frames reuse two measurements; name, rounded score, font, scale and visibility invalidation passed");
}
