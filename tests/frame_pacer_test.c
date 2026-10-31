#include "../app/src/game/frame_pacer.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static int near(double a,double b) { return fabs(a-b)<1e-9; }
int main(void) {
  const double interval=1000.0/60;
  double deadline=0;
  // First frame: a full interval from now.
  assert(near(frame_pacer_next(&deadline,1000,interval),interval) && near(deadline,1000+interval));
  // A frame that took 1 ms of the interval waits for the rest of it; the pace is kept exactly.
  double start=deadline;
  assert(near(frame_pacer_next(&deadline,start+1,interval),interval-1) && near(deadline,start+interval));
  // 60 on-time frames end exactly one second later, whatever each one cost.
  start=deadline;
  for(int i=0;i<60;i++) { double now=deadline+(i%7); double wait=frame_pacer_next(&deadline,now,interval); assert(wait>0 && near(now+wait,deadline)); }
  assert(fabs(deadline-(start+1000))<1e-6);
  // The display made this frame wait 20 ms: the next one starts at once instead of after a
  // further interval (which made such frames 37 ms long), and the pace restarts from here.
  start=deadline;
  assert(frame_pacer_next(&deadline,start+20,interval)==0 && near(deadline,start+20));
  assert(near(frame_pacer_next(&deadline,start+21,interval),interval-1));
  // Frames never come closer together than one interval to make up for a late one.
  start=deadline;
  assert(frame_pacer_next(&deadline,start+17,interval)==0 && near(deadline,start+17));
  assert(near(frame_pacer_next(&deadline,start+17.5,interval),interval-.5) && near(deadline,start+17+interval));
  // A long stall (window dragged, half a second): no burst of frames afterwards.
  start=deadline;
  assert(frame_pacer_next(&deadline,start+500,interval)==0 && near(deadline,start+500));
  assert(near(frame_pacer_next(&deadline,start+501,interval),interval-1));
  // Exactly on the deadline counts as due.
  start=deadline;
  assert(frame_pacer_next(&deadline,start+interval,interval)==0);
  puts("frame pacer test passed");
  return 0;
}
