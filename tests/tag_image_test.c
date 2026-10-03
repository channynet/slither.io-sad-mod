#include "../app/src/ui/tag_image.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void u32(unsigned char* p,uint32_t v) { for(int i=0;i<4;i++) p[i]=(unsigned char)(v>>(8*i)); }
static unsigned char* bmp(int w,int h,size_t* length) {
  *length=54+(size_t)w*h*4; unsigned char* p=calloc(1,*length); assert(p);
  p[0]='B'; p[1]='M'; u32(p+2,(uint32_t)*length); u32(p+10,54); u32(p+14,40);
  u32(p+18,w); u32(p+22,h); p[26]=1; p[28]=32;
  for(size_t i=54;i<*length;i+=4) { p[i]=30; p[i+1]=180; p[i+2]=240; p[i+3]=127; }
  return p;
}
int main(int argc,char** argv) {
  tag_image image={0},loaded={0}; size_t length;
  unsigned char* p=bmp(1024,256,&length);
  assert(tag_image_decode(p,length,&image)); assert(image.width==512 && image.height==128);
  assert(image.rgba[0]==240 && image.rgba[1]==180 && image.rgba[2]==30 && image.rgba[3]==127);
  assert(tag_image_save("test-tag.rgba",&image)); assert(tag_image_load("test-tag.rgba",&loaded));
  assert(loaded.width==image.width && loaded.height==image.height);
  assert(!memcmp(image.rgba,loaded.rgba,(size_t)image.width*image.height*4)); tag_image_free(&loaded);
  FILE* f=fopen("test-tag.rgba","ab"); assert(f); fputc(1,f); fclose(f);
  assert(!tag_image_load("test-tag.rgba",&loaded)); assert(!loaded.rgba);
  f=fopen("test-tag.rgba","wb"); uint32_t bad[]={0x31474154,0xffffffff,2}; fwrite(bad,1,sizeof(bad),f); fclose(f);
  assert(!tag_image_load("test-tag.rgba",&loaded));
  u32(p+18,4097); assert(!tag_image_decode(p,length,&loaded));
  assert(!tag_image_decode(p,8,&loaded)); assert(!tag_image_decode(p,17*1024*1024,&loaded));
  assert(!tag_image_decode((unsigned char*)"not an image",12,&loaded));
  free(p); tag_image_free(&image); assert(!image.rgba && !image.width);
  p=bmp(1,17,&length); assert(tag_image_decode(p,length,&image)); assert(image.width==1 && image.height==17);
  tag_image_free(&image); free(p);
  if(argc>1) {
    f=fopen(argv[1],"rb"); assert(f); fseek(f,0,SEEK_END); long n=ftell(f); rewind(f);
    p=malloc(n); assert(p && fread(p,1,n,f)==(size_t)n); fclose(f);
    assert(tag_image_decode(p,n,&image)); free(p);
    assert(image.width<=512 && image.height<=512);
    size_t clear=0,solid=0;
    for(int i=0;i<image.width*image.height;i++) { clear+=image.rgba[4*i+3]==0; solid+=image.rgba[4*i+3]>240; }
    assert(clear>0 && solid>0);
    printf("Bundled wings decoded: %dx%d with transparent and opaque pixels\n",image.width,image.height);
    tag_image_free(&image);
  }
  puts("Tag images: decode, alpha, aspect ratio, resizing, cache round trip and invalid data passed");
}
