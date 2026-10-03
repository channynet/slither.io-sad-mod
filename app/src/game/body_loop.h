#ifndef BODY_LOOP_H
#define BODY_LOOP_H
#include "defensive_planner.h"
#define BODY_LOOP_SCAN 4096
#define BODY_LOOP_POINTS 128
typedef struct body_loop {
  def_vec points[BODY_LOOP_POINTS];
  int count;
  float length,body_length,overlap,spacing;
  int direction;
} body_loop;
/* Input goes from head toward tail. Capture only a nearby, aligned full lap. */
bool body_loop_capture(body_loop* loop,const def_vec* trail,int count,float heading,float radius,float minimum_radius);
float body_loop_heading(const body_loop* loop,def_vec position,float heading,float lookahead);
float body_loop_heading_offset(const body_loop* loop,def_vec position,float heading,float lookahead,float offset);
float body_loop_offset(const body_loop* loop,float radius,float speed,float latency);
#endif
