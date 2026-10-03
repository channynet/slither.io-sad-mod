#ifndef IMAGE_LIBRARY_H
#define IMAGE_LIBRARY_H
#include <stdbool.h>
#include <stddef.h>
#define IMAGE_LIBRARY_MAX 64
typedef struct {
  int id;
  char name[96];
  float size,gap,angle;
  bool rotate;
} image_item;
typedef struct {
  image_item items[IMAGE_LIBRARY_MAX];
  int count,selected;
  bool cosmetic,enabled,read_only;
} image_library;
void image_library_seed(image_library* lib,bool cosmetic);
/* 1: loaded, 0: absent, -1: invalid/unreadable (preserve original). */
int image_library_load(image_library* lib);
bool image_library_save(const image_library* lib);
image_item* image_library_add(image_library* lib,const char* name);
void image_library_path(const image_library* lib,int index,char* out,size_t size);
#endif
