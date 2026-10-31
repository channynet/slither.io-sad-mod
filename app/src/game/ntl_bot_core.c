/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Carried over from the bot in the NTL mod, which is based on
 * "Slither.io Bot Championship Edition" (https://github.com/j-c-m/Slither.io-bot). */
#include "ntl_bot_core.h"
#include <math.h>
#include <string.h>

// The values the NTL mod runs with.
#define TARGET_FPS 25
#define ARC_SIZE (M_PI / 8)
#define RADIUS_MULT 12
#define FOOD_FRAMES 5
#define COLLISION_DELAY 25
#define SPEED_BASE 5.78
#define FRONT_ANGLE (M_PI / 2)
#define ENCIRCLE_THRESHOLD 0.5
#define ENCIRCLE_ALL_THRESHOLD 0.5
#define ENCIRCLE_DISTANCE_MULT 30
#define ENEMY_BODY_OFFSET 0.38
#define WALL_OFFSET 0.22
#define COURSE_FLOOR -0.8
#define COURSE_BACKOFF 0.16
#define TAIL_SHARE 0.85
#define TURN_DIVISOR 25
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double jsround(double v) { return floor(v + 0.5); }
static double dist2(double x1, double y1, double x2, double y2) { return (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2); }
static double fast_atan2(double y, double x) {
  double quarter = M_PI / 4, three = 3 * M_PI / 4, r, angle, abs_y = fabs(y) + 1e-10;
  if (x < 0) { r = (x + abs_y) / (abs_y - x); angle = three; }
  else { r = (x - abs_y) / (x + abs_y); angle = quarter; }
  angle += (0.1963 * r * r - 0.9817) * r;
  return y < 0 ? -angle : angle;
}
static nb_circle circle(double x, double y, double r) { return (nb_circle){jsround(x), jsround(y), jsround(r)}; }
static nb_vec unit(nb_vec v) { double l = sqrt(v.x * v.x + v.y * v.y); return l > 0 ? (nb_vec){v.x / l, v.y / l} : (nb_vec){0, 0}; }
static double snake_width(double sc) { return jsround(sc * 29.2); }
static double angle_between(double a1, double a2) {
  double r1 = fmod(a1 - a2, M_PI), r2 = fmod(a2 - a1, M_PI);
  return r1 < r2 ? -r1 : r2;
}
static int angle_index(double angle) {
  if (angle < 0) angle += 2 * M_PI;
  int index = (int)jsround(angle / ARC_SIZE);
  return index == NB_ARCS ? 0 : index;
}
// Where two circles overlap, and the direction of that spot from our head.
static bool circles_meet(const nb_snake* me, nb_circle a, nb_circle b, nb_vec* at, double* ang) {
  double r = a.radius + b.radius;
  if (!(a.x + r > b.x && a.y + r > b.y && a.x < b.x + r && a.y < b.y + r && dist2(a.x, a.y, b.x, b.y) < r * r)) return false;
  nb_vec p = {(a.x * b.radius + b.x * a.radius) / r, (a.y * b.radius + b.y * a.radius) / r};
  if (at) *at = p;
  if (ang) *ang = fast_atan2(p.y - me->yy, p.x - me->xx);
  return true;
}
static void poly_box(nb_poly* poly) {
  poly->minx = poly->maxx = poly->pts[0].x; poly->miny = poly->maxy = poly->pts[0].y;
  for (int i = 1; i < poly->count; i++) {
    if (poly->pts[i].x < poly->minx) poly->minx = poly->pts[i].x;
    if (poly->pts[i].x > poly->maxx) poly->maxx = poly->pts[i].x;
    if (poly->pts[i].y < poly->miny) poly->miny = poly->pts[i].y;
    if (poly->pts[i].y > poly->maxy) poly->maxy = poly->pts[i].y;
  }
}
static bool in_poly(nb_vec p, const nb_poly* poly) {
  if (p.x < poly->minx || p.x > poly->maxx || p.y < poly->miny || p.y > poly->maxy) return false;
  bool inside = false;
  for (int i = 0, j = poly->count - 1; i < poly->count; j = i++) {
    const nb_vec* a = poly->pts + i; const nb_vec* b = poly->pts + j;
    if ((a->y > p.y) != (b->y > p.y) && p.x < (b->x - a->x) * (p.y - a->y) / (b->y - a->y) + a->x) inside = !inside;
  }
  return inside;
}
static double cross(nb_vec o, nb_vec a, nb_vec b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); }
// Convex hull of a few points (monotone chain), into poly.
static void hull(nb_vec* pts, int count, nb_poly* poly) {
  for (int i = 1; i < count; i++) {          // insertion sort by x, then y; stable like the original
    nb_vec v = pts[i]; int k = i - 1;
    while (k >= 0 && (pts[k].x > v.x || (pts[k].x == v.x && pts[k].y > v.y))) { pts[k + 1] = pts[k]; k--; }
    pts[k + 1] = v;
  }
  nb_vec lower[32], upper[32]; int nl = 0, nu = 0;
  for (int i = 0; i < count; i++) {
    while (nl >= 2 && cross(lower[nl - 2], lower[nl - 1], pts[i]) <= 0) nl--;
    lower[nl++] = pts[i];
  }
  for (int i = count - 1; i >= 0; i--) {
    while (nu >= 2 && cross(upper[nu - 2], upper[nu - 1], pts[i]) <= 0) nu--;
    upper[nu++] = pts[i];
  }
  nu--; nl--;
  poly->count = 0;
  for (int i = 0; i < nl; i++) poly->pts[poly->count++] = lower[i];
  for (int i = 0; i < nu; i++) poly->pts[poly->count++] = upper[i];
  if (poly->count) poly_box(poly); else { poly->minx = poly->miny = 1; poly->maxx = poly->maxy = 0; }
}

static void set_goal(nb_bot* b, double x, double y) { b->goal = (nb_vec){x, y}; b->has_goal = true; }
static void heading_abs(nb_bot* b, const nb_snake* me, double angle) {
  set_goal(b, jsround(me->xx + b->head_circle.radius * cos(angle)), jsround(me->yy + b->head_circle.radius * sin(angle)));
}
static void heading_rel(nb_bot* b, const nb_snake* me, double angle) {
  double hx = me->xx + 500 * b->cos, hy = me->yy + 500 * b->sin, c = cos(-angle), s = sin(-angle);
  set_goal(b, jsround(c * (hx - me->xx) - s * (hy - me->yy) + me->xx), jsround(s * (hx - me->xx) + c * (hy - me->yy) + me->yy));
}

static bool near_wall(const nb_scene* sc, double within, bool flux_aware) {
  double vu = sqrt(dist2(sc->view_x, sc->view_y, sc->grd, sc->grd));
  return within > fabs((flux_aware && sc->flux ? sc->flux_grd : sc->grd) - vu);
}
static nb_vec wall_point(const nb_bot* b, const nb_scene* sc, double along) {
  double eu = atan2(sc->view_y - sc->grd, sc->view_x - sc->grd), r = sc->flux ? sc->flux_grd : b->map_r;
  return (nb_vec){b->mid + cos(eu + along / b->mid) * r, b->mid + sin(eu + along / b->mid) * r};
}

// Remember, for the direction a point lies in, the nearest thing found there.
static void add_angle(nb_bot* b, const nb_snake* me, const nb_point* p) {
  double ang = fast_atan2(jsround(p->yy - me->yy), jsround(p->xx - me->xx));
  int index = angle_index(ang);
  double actual = jsround(pow(sqrt(p->distance) - p->radius, 2));
  nb_angle* a = b->angles + index;
  if (!a->set || a->distance > p->distance)
    *a = (nb_angle){true, jsround(p->xx), jsround(p->yy), ang, actual, p->radius, p->snake};
}
static void push_point(nb_bot* b, const nb_point* p) { if (b->point_count < NB_POINTS_MAX) b->points[b->point_count++] = *p; }
static void sort_points(nb_point* p, int n) {       // by distance, keeping equal ones in order
  static nb_point tmp[NB_POINTS_MAX];
  for (int width = 1; width < n; width *= 2) {
    for (int lo = 0; lo < n; lo += 2 * width) {
      int mid = lo + width < n ? lo + width : n, hi = lo + 2 * width < n ? lo + 2 * width : n, i = lo, j = mid, k = lo;
      while (i < mid && j < hi) tmp[k++] = p[j].distance < p[i].distance ? p[j++] : p[i++];
      while (i < mid) tmp[k++] = p[i++];
      while (j < hi) tmp[k++] = p[j++];
    }
    memcpy(p, tmp, sizeof(nb_point) * (size_t)n);
  }
}
// Everything we could run into: heads (pushed ahead by their speed), bodies, and the wall.
static void collision_points(nb_bot* b, const nb_scene* sc) {
  const nb_snake* me = sc->snakes + sc->me;
  b->point_count = 0; memset(b->angles, 0, sizeof(b->angles));
  double box_x = jsround(me->xx - 1200), box_y = jsround(me->yy - 1200);
  for (int i = 0; i < sc->snake_count; i++) {
    const nb_snake* s = sc->snakes + i;
    if (s->id == me->id || !s->alive) continue;
    double r = snake_width(s->sc) / 2, sp = fmin(1, s->sp / 5.78 - 1);
    nb_point head = {s->xx + cos(s->ehang) * r * sp * RADIUS_MULT / 2, s->yy + sin(s->ehang) * r * sp * RADIUS_MULT / 2,
                     b->head_circle.radius, 0, i, true};
    head.distance = dist2(me->xx, me->yy, head.xx, head.yy);
    add_angle(b, me, &head); push_point(b, &head);
    for (int k = 0; k < s->count; k++) {
      const nb_part* part = s->pts + k;
      if (part->dying) continue;
      if (!(box_x <= part->xx && box_y <= part->yy && box_x + 2400 >= part->xx && box_y + 2400 >= part->yy)) continue;
      nb_point p = {part->xx, part->yy, r, 0, i, false};
      p.distance = dist2(me->xx, me->yy, p.xx, p.yy);
      add_angle(b, me, &p);
      if (p.distance <= pow(b->head_circle.radius + p.radius, 2)) push_point(b, &p);
    }
  }
  if (near_wall(sc, 3500, true))
    for (int along = -1000; along <= 1000; along += 8) {
      nb_vec w = wall_point(b, sc, along);
      nb_point p = {w.x, w.y, 8, 0, 0, false};   // counted with snake 0, as the original does
      p.distance = dist2(me->xx, me->yy, p.xx, p.yy);
      add_angle(b, me, &p); push_point(b, &p);
    }
  sort_points(b->points, b->point_count);
}
static bool in_front(const nb_snake* me, nb_vec p) {
  double ang = fast_atan2(jsround(p.y - me->yy), jsround(p.x - me->xx));
  return fabs(angle_between(ang, me->ehang)) < FRONT_ANGLE;
}
// Something inside the look-ahead circle: turn straight away from it.
static bool check_collision(nb_bot* b, const nb_scene* sc) {
  const nb_snake* me = sc->snakes + sc->me;
  collision_points(b, sc);
  for (int i = 0; i < b->point_count; i++) {
    const nb_point* p = b->points + i;
    nb_vec at; double ang;
    if (circles_meet(me, b->head_circle, circle(p->xx, p->yy, p->radius), &at, &ang) && in_front(me, at)) {
      b->accel = (p->head && sc->snakes[p->snake].sp > 10) ? true : sc->default_accel;
      // directly away; the side of our course the spot is on only decides which way the half turn is written
      bool left = (2000 * b->cos) * (at.y - me->yy) - (2000 * b->sin) * (at.x - me->xx) > 0;
      heading_abs(b, me, left ? ang - M_PI : ang + M_PI);
      return true;
    }
  }
  b->accel = sc->default_accel;
  return false;
}
// Steer for the middle of the widest gap, or failing that for whatever is furthest away.
static void heading_best(nb_bot* b, const nb_snake* me) {
  struct { int start, end, sz; } open[NB_ARCS + 1]; int open_count = 0;
  int open_start = -1; bool best_set = false; double best_distance = 0; int best_index = 0;   // -1: no gap being followed double best_distance = 0; int best_index = 0;
  for (int i = 0; i < NB_ARCS; i++) {
    double distance;
    if (!b->angles[i].set) { distance = 0; if (open_start < 0) open_start = i; }
    else {
      distance = b->angles[i].distance;
      // (a gap that starts in direction 0 is never closed or counted: the original tests the index as a flag)
      if (open_start > 0) { open[open_count].start = open_start; open[open_count].end = i - 1; open[open_count].sz = i - 1 - open_start; open_count++; open_start = -1; }
    }
    if (!best_set || (best_distance < distance && best_distance != 0)) { best_set = true; best_distance = distance; best_index = i; }
  }
  if (open_start > 0 && open_count) {
    open[0].start = open_start; open[0].sz = open[0].end - open_start;
    if (open[0].sz < 0) open[0].sz += NB_ARCS;
  } else if (open_start > 0) { open[0].start = open[0].end = open_start; open[0].sz = 0; open_count = 1; }
  if (open_count) {
    int pick = 0;
    for (int i = 1; i < open_count; i++) if (open[i].sz > open[pick].sz) pick = i;
    heading_abs(b, me, (open[pick].end - open[pick].sz / 2.0) * ARC_SIZE);
  } else heading_abs(b, me, best_index * ARC_SIZE);
}
// Being surrounded, by one snake or by everything together.
static bool check_encircle(nb_bot* b, const nb_scene* sc) {
  const nb_snake* me = sc->snakes + sc->me;
  int high = 0, high_snake = -1, all = 0;
  for (int i = 0; i < NB_ARCS; i++) {
    if (!b->angles[i].set) continue;
    int count = 0;
    for (int k = 0; k <= i; k++) if (b->angles[k].set && b->angles[k].snake == b->angles[i].snake) count++;
    if (count > high) { high = count; high_snake = b->angles[i].snake; }
    if (b->angles[i].distance < pow(b->radius * ENCIRCLE_DISTANCE_MULT, 2)) all++;
  }
  if (high > NB_ARCS * ENCIRCLE_THRESHOLD) {
    heading_best(b, me);
    b->accel = (high != NB_ARCS && sc->snakes[high_snake].sp > 10) ? true : sc->default_accel;
    return true;
  }
  if (all > NB_ARCS * ENCIRCLE_ALL_THRESHOLD) { heading_best(b, me); b->accel = sc->default_accel; return true; }
  b->accel = sc->default_accel;
  return false;
}
// The direction with the most food for the distance, skipping what is hard to turn into.
static void food_goal(nb_bot* b, const nb_scene* sc) {
  const nb_snake* me = sc->snakes + sc->me;
  memset(b->food_angles, 0, sizeof(b->food_angles));
  for (int i = sc->food_count - 1; i >= 0; i--) {
    const nb_food* f = sc->foods + i;
    if (f->eaten) continue;
    nb_circle fc = circle(f->x, f->y, 2);
    if (circles_meet(me, fc, b->side_l, NULL, NULL) || circles_meet(me, fc, b->side_r, NULL, NULL)) continue;
    double ang = fast_atan2(jsround(f->y - me->yy), jsround(f->x - me->xx));
    int index = angle_index(ang);
    double distance = dist2(me->xx, me->yy, f->x, f->y);
    if (b->angles[index].set && !(sqrt(b->angles[index].distance) > sqrt(distance) + b->radius * RADIUS_MULT * b->speed_mult / 2)) continue;
    nb_food_angle* a = b->food_angles + index;
    if (!a->set) *a = (nb_food_angle){true, jsround(f->x), jsround(f->y), ang, fabs(angle_between(ang, me->ehang)), distance, f->sz, f->sz * f->sz / distance};
    else {
      a->sz += jsround(f->sz); a->score += f->sz * f->sz / distance;
      if (a->distance > distance) { a->x = jsround(f->x); a->y = jsround(f->y); a->distance = distance; }
    }
  }
  const nb_food_angle* best = NULL;
  for (int i = 0; i < NB_ARCS; i++) if (b->food_angles[i].set && (!best || b->food_angles[i].score > best->score)) best = b->food_angles + i;
  b->has_food = true;
  if (best && best->sz > 0) { b->food_x = best->x; b->food_y = best->y; b->food_sz = best->sz; b->food_da = best->da; }
  else { b->food_x = b->mid; b->food_y = b->mid; b->food_sz = 0; b->food_da = 0; }
}
// Long enough: turn steadily one way until the head meets the body, then circle.
static void to_circle(nb_bot* b, const nb_scene* sc) {
  const nb_snake* me = sc->snakes + sc->me;
  int tail = 0;
  while (tail < me->count && me->pts[tail].dying) tail++;
  if (tail >= me->count) return;
  nb_circle tail_circle = circle(me->pts[tail].xx, me->pts[tail].yy, trunc(b->head_circle.radius / 2));
  double divisor = b->width > 100 ? 100 : b->width;
  divisor = trunc(divisor / 100 * TURN_DIVISOR);
  if (divisor < 8) divisor = 8;
  nb_circle here = circle(me->xx, me->yy, trunc(b->width / 2));
  b->accel = sc->default_accel;
  heading_rel(b, me, b->direction * M_PI / divisor);
  if (circles_meet(me, b->head_circle, tail_circle, NULL, NULL)) b->stage = NB_CIRCLE;
  for (int i = 0; i < me->count * TAIL_SHARE; i++) {
    if (me->pts[i].dying) continue;
    if (circles_meet(me, here, circle(me->pts[i].xx, me->pts[i].yy, trunc(b->width / 2)), NULL, NULL)) { b->stage = NB_CIRCLE; break; }
  }
}

// Our own body as a line from the head back to the tail, with the distance along it.
static void own_body(nb_bot* b, const nb_snake* me) {
  double x = me->xx + me->fx, y = me->yy + me->fy, len = 0;
  b->own_count = 0; b->own[b->own_count++] = (nb_own){x, y, 0};
  for (int i = me->count - 1; i >= 0 && b->own_count < NB_OWN_MAX; i--) {
    if (me->pts[i].dying) continue;
    double px = me->pts[i].xx + me->pts[i].fx, py = me->pts[i].yy + me->pts[i].fy;
    len += sqrt(dist2(x, y, px, py));
    b->own[b->own_count++] = (nb_own){px, py, len};
    x = px; y = py;
  }
  b->own_len = len;
}
static nb_vec own_at(const nb_bot* b, double t) {
  const nb_own* s = b->own; int n = b->own_count;
  if (t >= b->own_len) return (nb_vec){s[n - 1].x, s[n - 1].y};
  if (t <= 0) return (nb_vec){s[0].x, s[0].y};
  int lo = 0, hi = n - 1;
  while (hi - lo > 1) {
    int mid = (int)jsround((lo + hi) / 2.0);
    if (t > s[mid].len) lo = mid; else hi = mid;
  }
  double wp = s[hi].len - t, wq = t - s[lo].len, w = wp + wq;
  return (nb_vec){(wp * s[lo].x + wq * s[hi].x) / w, (wp * s[lo].y + wq * s[hi].y) / w};
}
// How far along our body the part nearest to the head is (skipping the neck).
static double closest_body(const nb_bot* b, const nb_snake* me) {
  double hx = me->xx + me->fx, hy = me->yy + me->fy; const nb_own* s = b->own; int n = b->own_count;
  if (n < 2) return b->own_len;
  int start = 0; double start_d2 = 0;
  for (;;) {
    double prev = start_d2;
    start++;
    start_d2 = dist2(hx, hy, s[start].x, s[start].y);
    if (start_d2 < prev || start == n - 1) break;
  }
  if (start >= n || start <= 1) return b->own_len;
  int min_n = start; double min_d2 = start_d2;
  for (int i = min_n + 1; i < n; i++) {
    double d2 = dist2(hx, hy, s[i].x, s[i].y);
    if (d2 < min_d2) { min_n = i; min_d2 = d2; }
  }
  int next_n; double next_d2;
  if (min_n == n - 1) { next_n = min_n - 1; next_d2 = dist2(hx, hy, s[next_n].x, s[next_n].y); }
  else {
    double before = dist2(hx, hy, s[min_n - 1].x, s[min_n - 1].y), after = dist2(hx, hy, s[min_n + 1].x, s[min_n + 1].y);
    if (before < after) { next_n = min_n - 1; next_d2 = before; } else { next_n = min_n + 1; next_d2 = after; }
  }
  double t2 = s[min_n].len - s[next_n].len;
  t2 *= t2;
  if (t2 == 0) return s[min_n].len;
  return (s[min_n].len * (t2 - (min_d2 - next_d2)) + s[next_n].len * (t2 + (min_d2 - next_d2))) / (2 * t2);
}
// The area we are about to sweep through, widened by `offset`: nothing else may be inside.
static void danger_zone(nb_bot* b, const nb_snake* me, double offset, nb_vec target, nb_vec normal, double close_dist, nb_vec past, nb_vec close_point) {
  double hx = me->xx + me->fx, hy = me->yy + me->fy, o = b->direction, w = b->width, c = b->cos, s = b->sin;
  nb_vec pts[10] = {
    {hx - o * offset * s, hy + o * offset * c},
    {hx + w * c + offset * (c - o * s), hy + w * s + offset * (s + o * c)},
    {hx + 1.75 * w * c + 0.3 * o * w * s + offset * (c - o * s), hy + 1.75 * w * s - 0.3 * o * w * c + offset * (s + o * c)},
    {hx + 2.5 * w * c + 0.7 * o * w * s + offset * (c - o * s), hy + 2.5 * w * s - 0.7 * o * w * c + offset * (s + o * c)},
    {hx + 3 * w * c + 1.2 * o * w * s + offset * c, hy + 3 * w * s - 1.2 * o * w * c + offset * s},
    {target.x + normal.x * (offset + 0.5 * fmax(close_dist, 0)), target.y + normal.y * (offset + 0.5 * fmax(close_dist, 0))},
    {past.x + normal.x * offset, past.y + normal.y * offset},
    past, target, close_point};
  hull(pts, 10, &b->zone);
}
// Circle along our own body: stay just outside the previous lap, tighten when there is room,
// and bend the course away from other snakes and the wall before they are in the way.
static void follow_circle(nb_bot* b, const nb_scene* sc) {
  const nb_snake* me = sc->snakes + sc->me;
  own_body(b, me);
  {  // which way round: with the body's middle on the inside (smart), otherwise always the same way
    double cx = 0, cy = 0;
    for (int i = 0; i < b->own_count; i++) { cx += b->own[i].x; cy += b->own[i].y; }
    double vx = me->xx + me->fx - cx / b->own_count, vy = me->yy + me->fy - cy / b->own_count;
    b->direction = sc->smart ? (0 < -vy * b->cos + vx * b->sin ? -1 : 1) : 1;
  }
  double o = b->direction, w = b->width;
  if (b->own_len < 9 * w) return;
  double hx = me->xx + me->fx, hy = me->yy + me->fy;
  double close_t = closest_body(b, me);
  nb_vec close_point = own_at(b, close_t), close_next = own_at(b, close_t - w);
  nb_vec tangent = unit((nb_vec){close_next.x - close_point.x, close_next.y - close_point.y});
  nb_vec normal = {-o * tangent.y, o * tangent.x};
  double course = asin(fmax(-1, fmin(1, b->cos * normal.x + b->sin * normal.y)));
  double close_dist = (hx - close_point.x) * normal.x + (hy - close_point.y) * normal.y;
  // the part of our loop that lies ahead
  double from = 5 * w, to = close_t + 5 * w;
  b->inside.count = 0;
  b->inside.pts[b->inside.count++] = own_at(b, to);
  b->inside.pts[b->inside.count++] = own_at(b, from);
  for (double t = from; t < to && b->inside.count < NB_POLY_MAX; t += w) b->inside.pts[b->inside.count++] = own_at(b, t);
  poly_box(&b->inside);
  // where on the body we will be alongside once we have closed in
  double target_t = close_t, far = 0, step = w / 64, h = close_dist, a = course;
  for (int guard = 0; h >= 0.125 * w && guard < 2000000; guard++) {
    target_t -= step; far += step * cos(a); h += step * sin(a);
    a = fmax(-M_PI / 4, a - step / w);
  }
  nb_vec target = own_at(b, target_t), past = own_at(b, target_t - 3 * w);
  bool enemy_inside = false; double head_d2 = 4096 * w * w, body_offset = 0.15 * w;
  for (int i = 0; i < sc->snake_count; i++) {
    const nb_snake* s = sc->snakes + i;
    if (s->id == me->id || !s->alive) continue;
    nb_vec head = {s->xx + s->fx, s->yy + s->fy};
    double ax = head.x + cos(s->ang) * w, ay = head.y + sin(s->ang) * w;
    if (in_poly(head, &b->inside)) enemy_inside = true;
    else head_d2 = fmin(head_d2, fmin(dist2(head.x, head.y, target.x, target.y), dist2(ax, ay, target.x, target.y)));
    bool made = false;
    for (int k = 0; k < s->count; k++) {
      if (s->pts[k].dying) continue;
      nb_vec p = {s->pts[k].xx + s->pts[k].fx, s->pts[k].yy + s->pts[k].fy};
      if (in_poly(p, &b->inside)) continue;
      while (!made || (body_offset >= -w && in_poly(p, &b->zone))) {
        if (made) body_offset -= 0.0625 * w; else made = true;
        danger_zone(b, me, ENEMY_BODY_OFFSET * (w + snake_width(s->sc)) + body_offset, target, normal, close_dist, past, close_point);
      }
    }
  }
  double head_dist = sqrt(head_d2), wall_offset = 0.25 * w;
  bool wall = near_wall(sc, 3500, false);
  if (wall) {
    bool made = false;
    for (int along = -1000; along <= 1000; along += 8) {
      nb_vec p = wall_point(b, sc, along);
      while (!made || (wall_offset >= -w && in_poly(p, &b->zone))) {
        if (made) wall_offset -= 0.0625 * w; else made = true;
        danger_zone(b, me, WALL_OFFSET * (w + wall_offset), target, normal, close_dist, past, close_point);
      }
    }
  }
  double goal_course = course;
  if (near_wall(sc, 1650, false)) goal_course += 0.25;
  double head_prox = -1 - (2 * far - head_dist) / w;
  goal_course = fmin(goal_course, head_prox > 0 ? 0.125 * head_prox * head_prox : -0.5 * head_prox * head_prox);
  bool backed = false; double backed_course = 0;
  if (goal_course > goal_course + (body_offset - 0.0625 * w) / w) { backed = true; goal_course += (body_offset - 0.0625 * w) / w; backed_course = goal_course; }
  if (wall) goal_course = fmin(goal_course, goal_course + (wall_offset - 0.0625 * w) / w);
  double tail_behind = b->own_len - close_t;
  nb_vec toward = unit((nb_vec){b->follow_target.x - hx, b->follow_target.y - hy});
  goal_course = fmin(goal_course, (tail_behind - w * (2 - 0.5 * (toward.x * normal.x + toward.y * normal.y)) + (w - close_dist)) / w);
  goal_course = fmin(goal_course, -0.5 * (close_dist - (enemy_inside ? 1 : 4) * w) / w);
  goal_course = fmax(goal_course, COURSE_FLOOR * close_dist / w);
  goal_course = fmin(goal_course, 1);
  if (backed && goal_course > backed_course) goal_course -= COURSE_BACKOFF;
  double gx = hx + 4 * (tangent.x * cos(goal_course) - o * tangent.y * sin(goal_course)) * w;
  double gy = hy + 4 * (tangent.y * cos(goal_course) + o * tangent.x * sin(goal_course)) * w;
  if (b->has_goal && fabs(gx - b->goal.x) < 1000 && fabs(gy - b->goal.y) < 1000)
    set_goal(b, jsround(0.25 * gx + 0.75 * b->goal.x), jsround(0.25 * gy + 0.75 * b->goal.y));
  else set_goal(b, jsround(gx), jsround(gy));
}

static void every(nb_bot* b, const nb_scene* sc) {
  const nb_snake* me = sc->snakes + sc->me;
  b->mid = sc->grd; b->map_r = 0.98 * sc->grd;
  if (near_wall(sc, 3500, false)) {
    double eu = atan2(sc->view_y - sc->grd, sc->view_x - sc->grd);
    b->follow_target = (nb_vec){b->mid + cos(eu + 64 * b->direction / b->mid) * b->map_r, b->mid + sin(eu + 64 * b->direction / b->mid) * b->map_r};
  } else b->follow_target = (nb_vec){b->mid, b->mid};
  b->cos = cos(me->ang); b->sin = sin(me->ang);
  b->speed_mult = me->sp / SPEED_BASE;
  b->width = snake_width(me->sc); b->radius = b->width / 2;
  b->head_circle = circle(me->xx + b->cos * fmin(1, b->speed_mult - 1) * RADIUS_MULT / 2 * b->radius,
                          me->yy + b->sin * fmin(1, b->speed_mult - 1) * RADIUS_MULT / 2 * b->radius, RADIUS_MULT / 2.0 * b->radius);
  double lx = me->xx, ly = me->yy;
  if (me->count) { lx = me->pts[me->count - 1].xx; ly = me->pts[me->count - 1].yy; }
  b->side_r = circle(lx - b->sin * b->width, ly + b->cos * b->width, b->width * b->speed_mult);
  b->side_l = circle(lx + b->sin * b->width, ly - b->cos * b->width, b->width * b->speed_mult);
}
// What the original does a moment later on a timer: pick food, or keep curling up.
static void action(nb_bot* b, const nb_scene* sc) {
  const nb_snake* me = sc->snakes + sc->me;
  if (me->alive) {
    if (b->stage == NB_GROW) { food_goal(b, sc); set_goal(b, b->food_x, b->food_y); }
    else if (b->stage == NB_TOCIRCLE) to_circle(b, sc);
  }
  b->action_set = false;
}

void nb_reset(nb_bot* bot) {
  bot->stage = NB_GROW; bot->has_goal = bot->has_food = bot->action_set = bot->accel = false; bot->direction = 1;
  bot->point_count = 0; memset(bot->angles, 0, sizeof(bot->angles));
}
void nb_go(nb_bot* b, const nb_scene* sc, nb_out* out) {
  if (b->direction != 1 && b->direction != -1) b->direction = 1;
  every(b, sc);
  if (b->action_set && sc->now_ms >= b->action_due) action(b, sc);
  if (sc->smart) {
    if (sc->length < sc->follow_length) b->stage = NB_GROW;
    else if (b->stage != NB_CIRCLE) to_circle(b, sc);
  } else b->stage = NB_CIRCLE;
  if (b->has_food && b->stage != NB_GROW) b->has_food = false;
  if (b->stage == NB_CIRCLE) { b->accel = sc->default_accel; follow_circle(b, sc); }
  else if (check_collision(b, sc) || check_encircle(b, sc)) {
    if (b->action_set) b->action_due = sc->now_ms + 1000.0 / TARGET_FPS * COLLISION_DELAY;
  } else {
    if (sc->length > sc->follow_length) b->stage = NB_TOCIRCLE;
    if (!b->action_set) { b->action_set = true; b->action_due = sc->now_ms + 1000.0 / TARGET_FPS * FOOD_FRAMES; }
  }
  out->has_goal = b->has_goal; out->goal_x = b->goal.x; out->goal_y = b->goal.y; out->accel = b->accel; out->stage = b->stage;
}
