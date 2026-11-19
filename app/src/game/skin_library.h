#ifndef SKIN_LIBRARY_H
#define SKIN_LIBRARY_H
#include <stdbool.h>
#define SKIN_LIBRARY_MAX 128
typedef struct {
  int id;
  bool enabled,rotate,sway;
  float size,gap,angle;
} skin_image_state;
typedef struct {
  char code[257];
  bool custom,fire,gradient,smile,claws,local_tag;
  int special_skin;
  int default_skin,accessory,gradient_count,tag_mode,tag_id;
  float fire_strength,colors[4][3],look[4];
  skin_image_state images[2];
} skin_design;
typedef struct { skin_design items[SKIN_LIBRARY_MAX]; int count; bool read_only; } skin_library;
// A malformed file is preserved and makes this library read-only.
bool skin_library_load(skin_library* library,const char* path);
// Returns the existing/new index, or -1 on failure. Does not evict older skins.
int skin_library_remember(skin_library* library,const char* path,const skin_design* design);
#endif
