#include "tag_image.h"
#include <external/stb/stb_image.h>
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
void tag_image_free(tag_image* image) { free(image->rgba); *image=(tag_image){0}; }
bool tag_image_decode(const unsigned char* bytes,size_t length,tag_image* out) {
  *out=(tag_image){0};
  if(length<8 || length>16*1024*1024) return false;
  bool png=!memcmp(bytes,"\x89PNG\r\n\x1a\n",8),jpg=bytes[0]==255 && bytes[1]==216,bmp=bytes[0]=='B'&&bytes[1]=='M';
  if(!png && !jpg && !bmp) return false;
  int w,h,c;
  if(!stbi_info_from_memory(bytes,(int)length,&w,&h,&c) || w<1 || h<1 || w>4096 || h>4096) return false;
  unsigned char* original=stbi_load_from_memory(bytes,(int)length,&w,&h,&c,4);
  if(!original) return false;
  float scale=fminf(1,512.0f/fmaxf(w,h));
  out->width=max(1,(int)roundf(w*scale)); out->height=max(1,(int)roundf(h*scale));
  out->rgba=malloc((size_t)out->width*out->height*4);
  if(!out->rgba) { stbi_image_free(original); *out=(tag_image){0}; return false; }
  // Premultiplied interpolation keeps transparent PNG edges free of dark halos.
  for(int y=0;y<out->height;y++) for(int x=0;x<out->width;x++) {
    float px=fmaxf(0,(x+0.5f)*w/out->width-0.5f),py=fmaxf(0,(y+0.5f)*h/out->height-0.5f);
    int x0=(int)px,y0=(int)py,x1=min(w-1,x0+1),y1=min(h-1,y0+1);
    float fx=px-x0,fy=py-y0,weights[]={ (1-fx)*(1-fy),fx*(1-fy),(1-fx)*fy,fx*fy };
    const unsigned char* p[]={original+4*(y0*w+x0),original+4*(y0*w+x1),original+4*(y1*w+x0),original+4*(y1*w+x1)};
    float a=0,rgb[3]={0};
    for(int i=0;i<4;i++) { float wa=weights[i]*p[i][3]; a+=wa; for(int c=0;c<3;c++) rgb[c]+=wa*p[i][c]; }
    unsigned char* dst=out->rgba+4*(y*out->width+x);
    for(int c=0;c<3;c++) dst[c]=a>0 ? (unsigned char)fminf(255,roundf(rgb[c]/a)) : 0;
    dst[3]=(unsigned char)fminf(255,roundf(a));
  }
  stbi_image_free(original); return true;
}
bool tag_image_save(const char* path,const tag_image* image) {
  if(!image->rgba || image->width<1 || image->height<1 || image->width>512 || image->height>512) return false;
  char temp[MAX_PATH]; if(snprintf(temp,sizeof(temp),"%s.tmp",path)>=(int)sizeof(temp)) return false;
  FILE* f=fopen(temp,"wb"); if(!f) return false;
  uint32_t header[]={0x31474154,(uint32_t)image->width,(uint32_t)image->height};
  size_t size=(size_t)image->width*image->height*4;
  bool ok=fwrite(header,1,sizeof(header),f)==sizeof(header) && fwrite(image->rgba,1,size,f)==size;
  if(fclose(f)) ok=false;
  return ok && MoveFileExA(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
bool tag_image_load(const char* path,tag_image* out) {
  *out=(tag_image){0}; FILE* f=fopen(path,"rb"); if(!f) return false;
  uint32_t header[3]; bool ok=fread(header,1,sizeof(header),f)==sizeof(header) && header[0]==0x31474154 && header[1]>0 && header[1]<=512 && header[2]>0 && header[2]<=512;
  if(ok) {
    out->width=header[1]; out->height=header[2]; size_t n=(size_t)out->width*out->height*4;
    out->rgba=malloc(n); ok=out->rgba && fread(out->rgba,1,n,f)==n && fgetc(f)==EOF && !ferror(f);
  }
  fclose(f); if(!ok) tag_image_free(out); return ok;
}
