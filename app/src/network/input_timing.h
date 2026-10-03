#ifndef INPUT_TIMING_H
#define INPUT_TIMING_H
#include <stdbool.h>
// NTL 9.55+ documented mouse / keyboard / boost cadence, in milliseconds.
#define STEER_INTERVAL_MS 33.0f
#define KEY_INTERVAL_MS 50.0f
#define BOOST_INTERVAL_MS 50.0f
static inline bool input_due(float now,float last,float interval) {
  return now-last>=interval;
}
#endif
