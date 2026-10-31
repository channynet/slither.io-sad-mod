/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Carried over from the bot in the NTL mod, which is based on
 * "Slither.io Bot Championship Edition" (https://github.com/j-c-m/Slither.io-bot). */
#ifndef NTL_BOT_CORE_H
#define NTL_BOT_CORE_H
// The bot of the NTL mod (itself based on "Slither.io Bot Championship Edition",
// https://github.com/j-c-m/Slither.io-bot, MPL-2.0), carried over step by step so that it
// steers the same way. It only reads a plain description of the scene and returns where to
// aim and whether to boost; nothing here touches the game, the network or the screen.
//
// Smart bot on:  eat ("grow") until the chosen length, then curl up ("tocircle") and keep
//                circling its own body ("circle").
// Smart bot off: circle its own body from the start.
#include <stdbool.h>

#define NB_ARCS 16            // directions the surroundings are sorted into (22.5 degrees each)
#define NB_OWN_MAX 2048
#define NB_POINTS_MAX 16384
#define NB_POLY_MAX 2100

typedef struct { float xx, yy, fx, fy; bool dying; } nb_part;
typedef struct {
  int id;
  float xx, yy, fx, fy, ang, ehang, sp, sc;
  bool alive;                 // fully faded in and not dying
  const nb_part* pts; int count;   // body points, tail first
} nb_snake;
typedef struct { float x, y, sz; bool eaten; } nb_food;
typedef struct {
  const nb_snake* snakes; int snake_count; int me;
  const nb_food* foods; int food_count;
  double grd;                 // map centre (both coordinates) and nominal radius
  bool flux; double flux_grd; // a wall radius announced by the server, when there is one
  double view_x, view_y;      // what the screen is centred on
  int length;                 // our length as shown on screen
  double now_ms;
  bool default_accel;         // the player is holding boost
  bool smart;
  int follow_length;          // "bot length": where growing stops and circling starts
} nb_scene;

enum { NB_GROW, NB_TOCIRCLE, NB_CIRCLE };
typedef struct { double x, y; } nb_vec;
typedef struct { double x, y, len; } nb_own;
typedef struct { double xx, yy, radius, distance; int snake; bool head; } nb_point;
typedef struct { bool set; double x, y, ang, distance, radius; int snake; } nb_angle;
typedef struct { bool set; double x, y, ang, da, distance, sz, score; } nb_food_angle;
typedef struct { nb_vec pts[NB_POLY_MAX]; int count; double minx, maxx, miny, maxy; } nb_poly;
typedef struct { double x, y, radius; } nb_circle;

typedef struct {
  // kept between steps
  int stage;
  bool has_goal; nb_vec goal;
  bool has_food; double food_x, food_y, food_sz, food_da;
  bool action_set; double action_due;
  int direction;              // which way round it circles: 1 or -1
  bool accel;
  // worked out at every step
  double cos, sin, speed_mult, radius, width, mid, map_r;
  nb_circle head_circle, side_r, side_l;
  nb_vec follow_target;
  nb_own own[NB_OWN_MAX]; int own_count; double own_len;
  nb_point points[NB_POINTS_MAX]; int point_count;
  nb_angle angles[NB_ARCS];
  nb_food_angle food_angles[NB_ARCS];
  nb_poly inside, zone;
} nb_bot;

typedef struct { bool has_goal; double goal_x, goal_y; bool accel; int stage; } nb_out;

void nb_reset(nb_bot* bot);
void nb_go(nb_bot* bot, const nb_scene* scene, nb_out* out);
#endif
