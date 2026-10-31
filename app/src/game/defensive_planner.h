#ifndef DEFENSIVE_PLANNER_H
#define DEFENSIVE_PLANNER_H
#include <stdbool.h>
#define DEF_OBSTACLES 512
#define DEF_CANDIDATES 17
#define DEF_STEPS 15
#define DEF_HORIZON 1.5f
typedef struct { float x,y; } def_vec;
typedef struct { def_vec a,b,velocity; float radius,priority; bool head; } def_obstacle;
typedef struct {
  def_vec position,center,food;
  float angle,speed,turn_rate,radius,arena_radius,latency,previous_angle;
  bool has_food,has_previous;
  const struct body_loop* body_path; // refreshed own-body lap; NULL when inactive
  float body_offset;
  bool has_previous_body;
  float previous_body_offset;
  def_vec orbit_center;
  float orbit_radius;
  int orbit_direction; // -1 or +1; zero disables orbit continuation
  def_obstacle obstacles[DEF_OBSTACLES];
  int count;
  float omitted_clearance;
} def_scene;
typedef struct { float angle,clearance,safe_seconds; bool safe; bool following_body; float body_offset; } def_result;
float def_wrap(float angle);
float def_point_segment(def_vec p,def_vec a,def_vec b);
void def_scene_reset(def_scene* scene);
void def_add_obstacle(def_scene* scene,def_obstacle obstacle);
def_result def_evaluate(const def_scene* scene,float angle);
float def_orbit_heading(const def_scene* scene,def_vec position);
def_result def_evaluate_orbit(const def_scene* scene);
def_result def_evaluate_body(const def_scene* scene,float offset);
float def_body_outward_offset(const def_scene* scene);
def_result def_choose(const def_scene* scene);
#endif
