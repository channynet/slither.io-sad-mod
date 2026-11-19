#include "../app/src/ui/menu_background_image.h"
#include <external/stb/stb_image_write.h>
#include <assert.h>
#ifndef _WIN32
#include <unistd.h>
#define RemoveDirectoryA(path) (rmdir(path) == 0)
#endif

int main(void) {
  int width=1024,height=600,w,h;
  unsigned char* input=malloc((size_t)width*height*4);
  memset(input,170,(size_t)width*height*4);
  assert(stbi_write_png("source.png",width,height,4,input,width*4));
  assert(stbi_write_jpg("source.jpg",width,height,4,input,85));
  assert(stbi_write_bmp("source.bmp",width,height,4,input));
  const char* paths[]={"source.png","source.jpg","source.bmp"};
  for(int i=0;i<3;i++) {
    unsigned char* pixels=NULL; size_t length=0;
    unsigned char* bytes=read_image(fopen(paths[i],"rb"),&length,&pixels,&w,&h);
    assert(bytes && pixels && w==width && h==height);
    assert(save_image(bytes,length));
    free(bytes); stbi_image_free(pixels);
    assert(remove(paths[i])==0);
    // The original may move or disappear; the saved background remains readable.
    bytes=read_image(fopen(BACKGROUND_PATH,"rb"),&length,&pixels,&w,&h);
    assert(bytes && pixels && w==width && h==height);
    // A failed replacement must preserve the previous background.
    assert(CreateDirectoryA(BACKGROUND_PATH ".tmp",NULL));
    assert(!save_image((unsigned char*)"bad",3));
    assert(RemoveDirectoryA(BACKGROUND_PATH ".tmp"));
    free(bytes); stbi_image_free(pixels);
    bytes=read_image(fopen(BACKGROUND_PATH,"rb"),&length,&pixels,&w,&h);
    assert(bytes && pixels && w==width && h==height);
    free(bytes); stbi_image_free(pixels);
  }
  free(input);
  unsigned char* pixels=NULL; size_t length=0;
  FILE* file=fopen("bad.image","wb"); assert(file);
  fputs("not an image",file); fclose(file);
  assert(!read_image(fopen("bad.image","rb"),&length,&pixels,&w,&h) && !pixels);
  file=fopen("oversize.image","wb"); assert(file);
  assert(fseek(file,BACKGROUND_MAX_BYTES,SEEK_SET)==0); fputc(0,file); fclose(file);
  assert(!read_image(fopen("oversize.image","rb"),&length,&pixels,&w,&h));
  input=calloc(4097*4,1);
  assert(stbi_write_png("wide.png",4097,1,4,input,4097*4)); free(input);
  assert(!read_image(fopen("wide.png","rb"),&length,&pixels,&w,&h));
  assert(remove(BACKGROUND_PATH)==0);
  assert(!read_image(fopen(BACKGROUND_PATH,"rb"),&length,&pixels,&w,&h));
  puts("PASS: PNG/JPG/BMP, full resolution, persistence, failed replacement, invalid/oversize images, reset");
  return 0;
}
