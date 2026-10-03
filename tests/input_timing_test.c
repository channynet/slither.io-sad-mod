#include "../app/src/network/input_timing.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  assert(!input_due(132,100,STEER_INTERVAL_MS));
  assert(input_due(133,100,STEER_INTERVAL_MS));
  assert(!input_due(149,100,BOOST_INTERVAL_MS));
  assert(input_due(150,100,BOOST_INTERVAL_MS));
  assert(input_due(150,100,KEY_INTERVAL_MS));
  assert(!input_due(0,100,STEER_INTERVAL_MS));
  // A 4ms service cadence coalesces the latest direction without packet bursts.
  int sent=0; float last=0;
  for(int now=0;now<=1000;now+=4) if(input_due(now,last,STEER_INTERVAL_MS)) { sent++; last=now; }
  assert(sent==27);
  assert(input_due(2000,last,STEER_INTERVAL_MS));
  last=2000; assert(!input_due(2000,last,STEER_INTERVAL_MS));
  puts("Input cadence: 33/50/50ms boundaries, clock reset and no catch-up bursts passed");
}
