#ifndef DEATH_REVIEW_H
#define DEATH_REVIEW_H
#include <stdbool.h>
#include <thermite.h>
typedef struct {
  bool frozen;
  int last_frame, frame, action;
} death_review_state;
extern death_review_state death_review;
bool death_review_begin(tenv* env);
void death_review_draw(tenv* env);
void death_review_finish(tenv* env);
void death_review_reset(void);
#endif
