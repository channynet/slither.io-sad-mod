#ifndef NAME_LAYOUT_H
#define NAME_LAYOUT_H
#include "../constants.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  bool valid,score_valid;
  const void* font;
  float font_size,name_width,total_width;
  int score;
  char name[MAX_NICKNAME_LEN+1],score_text[9];
} name_layout;
// measure uses the current font; callers must include its effective size in the key.
static inline void name_layout_update(name_layout* c,const char* name,int score,
                                     bool show_score,const void* font,float size,
                                     float (*measure)(const char*)) {
  bool changed=!c->valid || c->font!=font || c->font_size!=size || strcmp(c->name,name);
  if(changed) {
    snprintf(c->name,sizeof(c->name),"%s",name);
    c->font=font; c->font_size=size; c->valid=true;
    c->name_width=measure(name); c->score_valid=false;
  }
  if(show_score && (!c->score_valid || c->score!=score)) {
    char text[9]; snprintf(text,sizeof(text)," %.1fK",score/1000.0);
    if(!c->score_valid || strcmp(text,c->score_text)) {
      char label[MAX_NICKNAME_LEN+1+9];
      snprintf(label,sizeof(label),"%s%s",name,text);
      c->total_width=measure(label);
      memcpy(c->score_text,text,strlen(text)+1);
    }
    c->score=score; c->score_valid=true;
  }
}
#endif
