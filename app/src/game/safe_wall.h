#ifndef SAFE_WALL_H
#define SAFE_WALL_H
// Safewall, the same two scripts as the NTL mod (settings "Safewall", "Safewall script" and
// "Border assist offset"). Near the wall, while the mouse points out of the map, the snake is
// steered to a point on a safe circle inside the wall instead, so it slides along the border.
//
// Script 2 (NTL default, "based on speed, angles, width, inertia"), active within 500 of the wall:
//   safe radius = wall - margin, margin = 3 + (offset + 10) / 20 * 8 (minus 3 when the snake is
//   turning one way), fixed when the script takes over. It takes over when the camera is within
//   (outwardness * turning radius + 40 frames of travel) of the safe circle and the mouse is past
//   the circle (unless the mouse points straight out, within 9 degrees). It then aims at the
//   point of the safe circle at a look-ahead distance from the camera (4 * turning radius *
//   outwardness, at least 125, or 150 while boosting), on the side nearer the mouse.
// Script 1 ("linear, from the mouse position"), active within 50 of the wall: the point 0.78 of
//   the way to the mouse is kept inside the wall; when it is outside, the snake aims at the
//   nearest of five points on the wall circle around it.
// Chosen distance (not in NTL): both scripts keep the head centre `gap` inside the wall (1 is
//   flush: the body overlaps the wall line); NTL's closest is 3 (offset -10) or 0..3 by the turn.
// Distances are world units; angles radians. Steering is returned as a vector from the head.
#include <math.h>
#include <stdbool.h>

typedef struct {
  double cx, cy;            // map centre (grd, grd)
  double grd;               // map radius
  bool dynamic;             // the server announces the wall (it can move)
  double wall;              // the announced wall radius (used when dynamic)
  double head_x, head_y;    // snake position (xx, yy)
  double view_x, view_y;    // camera centre
  double ang, wang;         // heading, and the angle the snake is turning to
  double mouse_x, mouse_y;  // mouse, pixels from the centre of the view
  double gsc;               // pixels per world unit
  double sp, scang, spang;  // speed and the two turn factors of the snake
  double mamu, nsp3;        // server's turn rate and boost speed
  bool boosting;
  int script;               // 1 or 2
  int offset;               // -10..10, more = further from the wall (script 2)
  double gap;               // >= 0: run with the head centre this far inside the wall (both scripts),
                            // instead of NTL's margins; < 0: NTL's offset margins
} safe_wall_input;

typedef struct {
  bool latched;             // script 2 keeps its take-over distance and margin while it steers
  double p0, s0;
} safe_wall_state;

typedef struct {
  bool steer;               // false: use the mouse
  double dx, dy;            // steering vector from the head
  double tx, ty;            // the point aimed at
} safe_wall_output;

#define SAFE_WALL_PI 3.14159265358979323846
#define SAFE_WALL_TIGHT 1.0       // the default distance (flush): head centre 1 inside the wall

static inline double safe_wall_angle01(double a) {            // into [0, 2pi)
  a = fmod(a, 2 * SAFE_WALL_PI);
  return a < 0 ? a + 2 * SAFE_WALL_PI : a;
}
// The wall radius the scripts use: the announced one, else 0.98 of the map radius.
static inline double safe_wall_radius(const safe_wall_input* in) {
  return in->dynamic ? in->wall : 0.98 * in->grd;
}
// Distance from the wall at which the script starts looking (NTL: 500 for script 2, 50 for 1).
static inline bool safe_wall_near(const safe_wall_input* in) {
  double r = hypot(in->head_x - in->cx, in->head_y - in->cy);
  return r > safe_wall_radius(in) - (in->script == 1 ? 50 : 500);
}

static inline bool safe_wall_script2(const safe_wall_input* in, safe_wall_state* st, safe_wall_output* out) {
  const double pi = SAFE_WALL_PI;
  double cx = in->cx, cy = in->cy, vx = in->view_x, vy = in->view_y;
  double R = safe_wall_radius(in), cone = 0.1 * pi;
  double margin = 3 + ((in->offset + 10) / 20.0) * 8;
  double turn = in->mamu * in->scang * in->spang;               // radians per 8 ms
  if (!(turn > 0) || !(in->gsc > 0)) { st->latched = false; return false; }
  double step = in->sp / 8, radius = step / turn;               // turning radius
  double look = in->nsp3 / 8 / turn;                            // turning radius at boost speed
  if (in->boosting) { radius = look; look = 150; } else look = 125;
  double dist = hypot(vx - cx, vy - cy);
  double out_ang = atan2(vy - cy, vx - cx);                     // straight out from the centre
  if (out_ang < 0) out_ang += 2 * pi;
  // How much the heading points out (1 straight out, 0 along the wall or in).
  double a = safe_wall_angle01(in->ang), b = out_ang;
  if (b < a && a > 3 * pi / 2 && b < pi / 2) b += 2 * pi;
  else if (a < b && b > 3 * pi / 2 && a < pi / 2) a += 2 * pi;
  double outward = sin(pi / 2 - fabs(a - b));
  outward = fmin(fmax(outward, 0), 1);
  double travel = outward * step * 40;
  if (!st->latched) {
    st->p0 = outward * radius + travel;
    double side = cos(out_ang) * sin(in->wang) - sin(out_ang) * cos(in->wang);
    st->s0 = in->gap >= 0 ? in->gap : side < 0 ? margin - 3 : margin;
    st->latched = true;
  }
  double safe = R - st->s0;
  if (dist < safe - st->p0) { st->latched = false; return false; }
  double mx = vx + in->mouse_x / in->gsc, my = vy + in->mouse_y / in->gsc;
  double mouse_dist = hypot(mx - cx, my - cy);
  double mouse_ang = atan2(my - vy, mx - vx);
  if (mouse_ang < 0) mouse_ang += 2 * pi;
  double hi = out_ang + 0.5 * cone, lo = out_ang - 0.5 * cone;
  if (hi > 2 * pi) hi -= 2 * pi;
  if (lo < 0) lo += 2 * pi;
  bool straight_out = lo > hi ? (mouse_ang > lo || mouse_ang < hi) : (mouse_ang > lo && mouse_ang < hi);
  if (mouse_dist >= safe && !straight_out) {
    // Where the circle round the camera (radius: the look-ahead) crosses the safe circle.
    double r1 = fmax(4 * radius * outward, look), r2 = safe;
    double ex = cx - vx, ey = cy - vy, d = sqrt(ex * ex + ey * ey);
    if (d > r1 + r2 || d < fabs(r1 - r2) || d <= 0) { st->latched = false; return false; }
    double along = (r1 * r1 - r2 * r2 + d * d) / (2 * d);
    double px = vx + ex * along / d, py = vy + ey * along / d;
    double h = sqrt(fmax(0, r1 * r1 - along * along));
    double ox = -ey * h / d, oy = ex * h / d;
    double ax = px + ox, ay = py + oy, bx = px - ox, by = py - oy;
    bool first = hypot(mx - ax, my - ay) <= hypot(mx - bx, my - by);
    out->tx = first ? ax : bx; out->ty = first ? ay : by;
    out->dx = out->tx - in->head_x; out->dy = out->ty - in->head_y;
    out->steer = true;
    return true;
  }
  st->latched = false;
  return false;
}

static inline bool safe_wall_script1(const safe_wall_input* in, safe_wall_output* out) {
  if (!(in->gsc > 0)) return false;
  double px = in->head_x + 0.78 * in->mouse_x / in->gsc, py = in->head_y + 0.78 * in->mouse_y / in->gsc;
  double limit = in->dynamic ? in->wall : 0.9798 * in->grd;
  if (in->gap >= 0) limit = safe_wall_radius(in) - in->gap;
  if ((in->cx - px) * (in->cx - px) + (in->cy - py) * (in->cy - py) <= limit * limit) return false;
  double at = atan2(py - in->cy, px - in->cx), on = in->dynamic ? in->wall : 0.9795 * in->grd;
  if (in->gap >= 0) on = safe_wall_radius(in) - in->gap;
  double best = 1e300, off = -0.008;
  for (int k = 1; k < 6; k++, off += 0.004) {
    double qx = in->cx + on * cos(at + off), qy = in->cy + on * sin(at + off);
    double e = hypot(px - qx, py - qy);
    if (e < best) { best = e; out->tx = qx; out->ty = qy; }
  }
  out->dx = out->tx - in->head_x; out->dy = out->ty - in->head_y;
  out->steer = true;
  return true;
}

// Call each time steering is decided. Returns true when the safewall steers (out->dx, out->dy).
static inline bool safe_wall_steer(const safe_wall_input* in, safe_wall_state* st, safe_wall_output* out) {
  out->steer = false; out->dx = out->dy = out->tx = out->ty = 0;
  if (!safe_wall_near(in)) { st->latched = false; return false; }
  return in->script == 1 ? safe_wall_script1(in, out) : safe_wall_script2(in, st, out);
}
#endif
