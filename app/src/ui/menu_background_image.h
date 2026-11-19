#ifndef MENU_BACKGROUND_IMAGE_H
#define MENU_BACKGROUND_IMAGE_H
#include <stdbool.h>
#include "../platform/compat.h"
#include <external/stb/stb_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BACKGROUND_PATH "menu-background.image"
#define BACKGROUND_MAX_BYTES (16 * 1024 * 1024)

// Validate and save the same bounded byte buffer, even if the source changes.
static unsigned char* read_image(FILE* file, size_t* length,
                                 unsigned char** pixels, int* width, int* height) {
  *pixels=NULL;
  if(!file) return NULL;
  bool seek=fseek(file,0,SEEK_END)==0;
  long size=seek ? ftell(file) : -1;
  rewind(file);
  unsigned char* bytes=size>=8 && size<=BACKGROUND_MAX_BYTES ? malloc((size_t)size) : NULL;
  bool ok=bytes && fread(bytes,1,(size_t)size,file)==(size_t)size && !ferror(file);
  fclose(file);
  if(ok) {
    bool format=!memcmp(bytes,"\x89PNG\r\n\x1a\n",8) ||
                (bytes[0]==255 && bytes[1]==216) || (bytes[0]=='B' && bytes[1]=='M');
    int channels;
    ok=format && stbi_info_from_memory(bytes,(int)size,width,height,&channels) &&
       *width>0 && *height>0 && *width<=4096 && *height<=4096;
    if(ok) *pixels=stbi_load_from_memory(bytes,(int)size,width,height,&channels,4);
    ok=ok && *pixels;
  }
  if(!ok) { free(bytes); return NULL; }
  *length=(size_t)size;
  return bytes;
}

static bool save_image(const unsigned char* bytes,size_t length) {
  const char* temp=BACKGROUND_PATH ".tmp";
  FILE* file=fopen(temp,"wb");
  if(!file) return false;
  bool ok=fwrite(bytes,1,length,file)==length;
  if(fclose(file)) ok=false;
  if(ok) ok=MoveFileExA(temp,BACKGROUND_PATH,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
  if(!ok) remove(temp);
  return ok;
}

#endif
