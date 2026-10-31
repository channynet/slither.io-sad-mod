#ifndef FRAME_PACER_H
#define FRAME_PACER_H
// Frame-rate cap. `deadline` is when the frame that has just been drawn was due to end.
// Returns how many milliseconds to wait before the next frame may start, and moves the
// deadline on to the frame after it.
//
// A frame that ran past its deadline (the GPU or the display made it wait) is followed by
// the next one at once. It used to be followed by a whole further interval of sleep, which
// turned a 20 ms frame into a 37 ms one. Frames are never brought closer together than one
// interval to make up for a late one, so no extra frames queue up in front of the display.
static inline double frame_pacer_next(double* deadline,double now,double interval) {
  if(*deadline<=0) *deadline=now;            // first frame
  *deadline+=interval;
  if(now>=*deadline) { *deadline=now; return 0; }
  return *deadline-now;
}
#endif
