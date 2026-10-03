#ifndef TAG_IMAGE_H
#define TAG_IMAGE_H
#include <stdbool.h>
#include <stddef.h>
typedef struct { unsigned char* rgba; int width,height; } tag_image;
bool tag_image_decode(const unsigned char* bytes,size_t length,tag_image* out);
bool tag_image_load(const char* path,tag_image* out);
bool tag_image_save(const char* path,const tag_image* image);
void tag_image_free(tag_image* image);
#endif
