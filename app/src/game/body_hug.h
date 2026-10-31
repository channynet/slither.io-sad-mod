#ifndef BODY_HUG_H
#define BODY_HUG_H
// Body hug (E): the snake runs alongside the nearest enemy's body as close as it can, on the
// side it is on, in the direction it is already going, with its nose `gap` outside the enemy's
// hitbox (the enemy body radius round its centre line; only the nose counts, so the two bodies
// overlap on screen). Past the tail it curls round the end and comes back along the other side;
// past the head it runs on beside the head; a body curled into a ring is followed round.
//
// How it steers. The angle sent now only takes effect once it reaches the server, so first the
// snake's own motion is carried forward through the angles already on their way (pre_ang). From
// there every choice of the next steering period (turning harder or softer either way, or the
// plain "aim ahead along the body" steering) is played forward with the game's own turning
// limits, quantised to the 251 directions the client can send, each followed by several ways of
// carrying on. The choice that keeps the nose nearest the wanted gap wins, among those after
// which some way of carrying on still keeps the nose out of every hitbox (and the head inside
// the wall) for the whole look-ahead: on a bend that curls in tighter than the snake can turn it
// drops back in time instead of finding out too late. Distances are world units, angles radians.
#include <math.h>
#include <stdbool.h>

typedef struct { double x, y; } hug_point;

typedef struct {
  const hug_point* body; int count;  // the enemy's body, tail first, head last (its head's next moves may be appended)
  double body_radius;                // the enemy's body radius
  double head_x, head_y, ang;        // own head and heading
  double my_radius;                  // own body radius
  double gap;                        // the nose's distance outside the enemy's hitbox to keep
  double look;                       // how far ahead along the body the plain steering aims
  double turn_radius;                // own turning circle: round an end at least this wide
  double step, turn;                 // own travel and most turning per 8 ms (0: no planning)
  const double* others; int others_count;  // other snakes nearby, HUG_OTHER floats each: x, y, radius, from-step
                                           // (there from that many steps of 8 ms on: 0 now, later for where a head
                                           // is going), kind (0: there now, 1: where a head is going, 2: where a
                                           // head could swing to)
  double wall_x, wall_y, wall_r;           // the map wall (wall_r 0: none): the head centre must stay inside
  const double* pre_ang; int pre_count;    // the angles in force for the next pre_count steps of 8 ms (already sent)
  double head_speed;                       // how far the enemy's head moves per 8 ms (0: still)
  int period;                              // steps of 8 ms between the angles sent (0: HUG_PERIOD; more on a slow frame rate)
  double noise;                            // how far off (rms) the own head's motion has lately been from its model: kept spare
  bool light;                              // plan with fewer choices (a slow computer)
  // The speed (per 8 ms) changes gradually, by step_rate a step: step_goal is what the boost
  // already sent heads for (0: stays at step), step_normal the speed without boosting, and
  // step_boost (0: not held) the boosting speed when the player holds the boost: the hug then
  // checks whether boosting on is safe, and asks for it to be let go (out.slow) when not.
  double step_goal, step_normal, step_boost, step_rate;
} hug_input;

typedef struct {
  int dir;                           // +1 towards the enemy's head, -1 towards its tail, 0: not set
  int side;                          // +1 left of the body (seen from tail to head), -1 right, 0: not set
  int slow;                          // asking for the boost to be let go (beside the front of a body it would overrun)
} hug_state;

typedef struct {
  double tx, ty;                     // the point the plain steering aims at
  double wanted;                     // the plain steering's angle
  double angle;                      // the angle to send
  double distance;                   // own head centre to the enemy's body centre line now
  double edge_gap;                   // how far the nose is outside the enemy's hitbox now (negative: inside = dead)
  double margin;                     // the planned worst safety margin (negative: no safe way found)
  double ms;                         // how long the planning took (filled in by the planning thread)
  bool slow;                         // let go of the boost: faster than the hugged head and beside its front
                                     // part (boosting on only runs out ahead of the head, into its way)
  int tier;                          // 0: hugging (a way out whatever nearby heads do), 1: keeping room for it,
                                     // 2: dodging their swings, 3: making room, 4-5: no safe way
} hug_output;

#define HUG_MAX_POINTS 2048
#define HUG_LIGHT_MS 12.0            // planning slower than this: the client asks for lighter planning
#define HUG_OTHER 5                  // floats per entry of `others`
#define HUG_MAX_OTHERS 8192
#define HUG_GRID 48.0               // grid cell size
#define HUG_GRID_CELLS 4096
#define HUG_GRID_ITEMS 65536
#define HUG_FAR 130.0                // the nose's gap is not looked for further out than this
#define HUG_ROOM 60.0                // nor room from the other snakes beyond this
#define HUG_LOOK_STEPS 60            // safety look-ahead at least (0.48 s); more for a snake slow to turn
#define HUG_LOOK_MAX 240
#ifndef HUG_HEAD_STEPS
#define HUG_HEAD_STEPS 60            // where a nearby head can be is looked at this far ahead (after the delay)
#endif
#ifndef HUG_CURL_STEPS
#define HUG_CURL_STEPS 150           // and a head curling round, followed on round this far
#endif
#define HUG_CIRCLE_MAX 900
#define HUG_FAR_ERROR 40.0           // further out than this the plain steering brings the snake in
#define HUG_TRACK_STEPS 48           // how far ahead the gap is scored
#define HUG_PERIOD 4                 // the client sends an angle every 33 ms: 4 steps of 8 ms (at the least)
#define HUG_HEAD_MARGIN 28.0         // extra room beside the enemy's head (it can swing into us)
#define HUG_HEAD_ZONE 220.0          // ... over this much of the body behind the head
#define HUG_OTHERS_MARGIN 4.0        // keep at least this far from snakes other than the one hugged
#define HUG_WALL_MARGIN 6.0          // and the head centre this far inside the wall
#define HUG_NOISE_SPARE 3.0          // keep this many times the recent motion noise spare
#define HUG_DELAY_DOUBT 0.25         // the delay may be this much (a fraction) off either way
#define HUG_APPROACH 1.8             // coming in from afar, aim this many times the distance along the body
#define HUG_ROOM_SPARE 4.0           // and a way out keeps this much room further round (plus a period's travel)
#define HUG_ROOM_COST 400.0          // the cost of each unit of room short for the way out (against the gap score)
#define HUG_DODGE_COST 2000.0        // and of each unit inside where a nearby head could swing
#define HUG_SAFE_ENOUGH 4.0          // with no way out, still clear by this much over the look-ahead: keep hugging
#define HUG_TIMING 5.0               // closing in, keep the distance covered in this many steps spare (timing jitter),
                                     // and in one more period of sending
#define HUG_SLOW_ZONE 260.0           // boosting, let go within this much of the hugged head (going its way)
#define HUG_SPEED_RATE 0.18          // how fast the game eases the speed (boost on or off), per step of 8 ms (as seen)
#define HUG_PI 3.14159265358979323846
#define HUG_BEND_GAIN 4.4
// The hitbox: a snake dies when the front of its head (its "nose", HUG_NOSE head radii ahead of
// the head centre) gets inside another snake's body; its head centre must stay inside the wall.
#define HUG_NOSE 0.8
#define HUG_NEED 1.5                 // the nose never planned nearer than this to any hitbox (what the game shows is never exact)

static inline double hug_wrap(double d) {
  d = fmod(d, 2 * HUG_PI);
  if (d > HUG_PI) d -= 2 * HUG_PI;
  if (d < -HUG_PI) d += 2 * HUG_PI;
  return d;
}
// The angle the server will steer to for a wanted angle: one of 251 directions (as the client sends).
static inline double hug_quant(double a) {
  a = fmod(a, 2 * HUG_PI);
  if (a < 0) a += 2 * HUG_PI;
  double v = floor(251 * a / (2 * HUG_PI) + .5);
  if (v >= 251) v -= 251;
  return v * 2 * HUG_PI / 251;
}
static inline int hug_send_dir(double a) { return (int)(hug_quant(a) * 251 / (2 * HUG_PI) + .5) % 251; }

static inline double hug_poly_dist2(const hug_point* p, int n, double x, double y) {
  double best = 1e300;
  for (int i = 0; i + 1 < n; i++) {
    double ax = p[i].x, ay = p[i].y, dx = p[i + 1].x - ax, dy = p[i + 1].y - ay;
    double ll = dx * dx + dy * dy, t = ll > 0 ? ((x - ax) * dx + (y - ay) * dy) / ll : 0;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    double ex = ax + dx * t - x, ey = ay + dy * t - y, d2 = ex * ex + ey * ey;
    if (d2 < best) best = d2;
  }
  return best;
}

typedef struct { double x, y, a, v, goal; } hug_pose;   // v: travel per step, easing towards goal
typedef struct {
  const hug_input* in;
  int n; double total; bool ring;
  double cum[HUG_MAX_POINTS];
  int seg[HUG_MAX_POINTS], nseg;        // the body's segments near enough to matter
  int oth[HUG_MAX_OTHERS], noth;                  // the other snakes' points near enough to matter
  double reach, look, nose; int look_steps, circle_steps, period;
  double head_margin, head_zone, need;        // room kept beside the enemy's head, over this much of its body
  bool overrun;                         // faster than the enemy's head even without boosting
  double gap;                           // the gap to keep (the one asked for, or more when the motion is noisy)
  // A grid over the nearby segments (and other snakes' points) for fast nearest look-ups.
  double gx0, gy0, gcell; int gw, gh;
  int cell_start[HUG_GRID_CELLS + 1], cell_items[HUG_GRID_ITEMS];
  int ocell_start[HUG_GRID_CELLS + 1], ocell_items[HUG_GRID_ITEMS]; double omax_r;
} hug_ctx;

// Point and unit direction at arc length s along the body (clamped to it).
static inline void hug_at(const hug_ctx* c, double s, double* x, double* y, double* tx, double* ty) {
  const hug_point* p = c->in->body; int n = c->n;
  int lo = 0, hi = n - 2;
  while (lo < hi) { int mid = (lo + hi + 1) / 2; if (c->cum[mid] <= s) lo = mid; else hi = mid - 1; }
  int i = lo;
  double seg = c->cum[i + 1] - c->cum[i];
  double t = seg > 0 ? (s - c->cum[i]) / seg : 0;
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  double dx = p[i + 1].x - p[i].x, dy = p[i + 1].y - p[i].y, l = sqrt(dx * dx + dy * dy);
  *x = p[i].x + dx * t; *y = p[i].y + dy * t;
  if (l > 0) { *tx = dx / l; *ty = dy / l; } else { *tx = 1; *ty = 0; }
}
// Nearest point of the (nearby) body to (x, y): distance, arc length, tangent, which side.
static inline double hug_near(const hug_ctx* c, double x, double y, double* s, double* tx, double* ty, double* side) {
  const hug_point* p = c->in->body;
  double best = 1e300; *s = 0; *tx = 1; *ty = 0; *side = 1;
  for (int j = 0; j < c->nseg; j++) {
    int i = c->seg[j];
    double ax = p[i].x, ay = p[i].y, dx = p[i + 1].x - ax, dy = p[i + 1].y - ay;
    double ll = dx * dx + dy * dy;
    double t = ll > 0 ? ((x - ax) * dx + (y - ay) * dy) / ll : 0;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    double px = ax + dx * t, py = ay + dy * t;
    double d2 = (x - px) * (x - px) + (y - py) * (y - py);
    if (d2 < best) {
      best = d2; double l = sqrt(ll); *s = c->cum[i] + l * t;
      if (l > 0) { *tx = dx / l; *ty = dy / l; }
      *side = *tx * (y - py) - *ty * (x - px) >= 0 ? 1 : -1;
    }
  }
  return sqrt(best);
}

// Nearest nearby segment to (x, y) through the grid, up to HUG_FAR away (none: HUG_FAR + 1, *s -1).
static inline double hug_near_grid(const hug_ctx* c, double x, double y, double* s) {
  const hug_point* p = c->in->body;
  double G = c->gcell;
  int cx = (int)floor((x - c->gx0) / G), cy = (int)floor((y - c->gy0) / G);
  double best = (HUG_FAR + 1) * (HUG_FAR + 1); *s = -1;   // (-1: none that near)
  int rings = (int)(HUG_FAR / G) + 2;
  for (int r = 0; r <= rings; r++) {
    if (r >= 2 && (r - 1) * G * (r - 1) * G > best) break;
    for (int j = cy - r; j <= cy + r; j++) {
      if (j < 0 || j >= c->gh) continue;
      int edge = (j == cy - r || j == cy + r);
      for (int i = cx - r; i <= cx + r; i += (edge || r == 0) ? 1 : 2 * r) {
        if (i < 0 || i >= c->gw) continue;
        int cell = j * c->gw + i;
        for (int k = c->cell_start[cell]; k < c->cell_start[cell + 1]; k++) {
          int q = c->cell_items[k];
          double ax = p[q].x, ay = p[q].y, dx = p[q + 1].x - ax, dy = p[q + 1].y - ay;
          double ll = dx * dx + dy * dy, t = ll > 0 ? ((x - ax) * dx + (y - ay) * dy) / ll : 0;
          if (t < 0) t = 0;
          if (t > 1) t = 1;
          double ex = ax + dx * t - x, ey = ay + dy * t - y, d2 = ex * ex + ey * ey;
          if (d2 < best) { best = d2; *s = c->cum[q] + sqrt(ll) * t; }
        }
      }
    }
  }
  return sqrt(best);
}
// Least gap from (x, y) to the other snakes' points there at step t (each by its radius), only
// looked for below `upto`: gap[0] counting everything, gap[1] leaving out where heads could swing
// to, gap[2] only what is there now.
static inline void hug_others_grid(const hug_ctx* c, double x, double y, double upto, double t, double* gap) {
  const double* o = c->in->others;
  double G = c->gcell;
  int cx = (int)floor((x - c->gx0) / G), cy = (int)floor((y - c->gy0) / G);
  gap[0] = gap[1] = gap[2] = upto;
  int rings = (int)((upto + c->omax_r) / G) + 1;
  for (int r = 0; r <= rings; r++) {
    if (r >= 2 && (r - 1) * G - c->omax_r > gap[2]) break;
    for (int j = cy - r; j <= cy + r; j++) {
      if (j < 0 || j >= c->gh) continue;
      int edge = (j == cy - r || j == cy + r);
      for (int i = cx - r; i <= cx + r; i += (edge || r == 0) ? 1 : 2 * r) {
        if (i < 0 || i >= c->gw) continue;
        int cell = j * c->gw + i;
        for (int k = c->ocell_start[cell]; k < c->ocell_start[cell + 1]; k++) {
          const double* q = o + HUG_OTHER * c->ocell_items[k];
          if (q[3] > t) continue;                       // not there yet
          double g = sqrt((q[0] - x) * (q[0] - x) + (q[1] - y) * (q[1] - y)) - q[2];
          for (int w = 0; w <= 2 - (int)q[4]; w++) if (g < gap[w]) gap[w] = g;
        }
      }
    }
  }
}
static inline void hug_grid_build(hug_ctx* c) {
  const hug_input* in = c->in; const hug_point* p = in->body;
  double x0 = 1e300, y0 = 1e300, x1 = -1e300, y1 = -1e300;
  for (int j = 0; j < c->nseg; j++) { int i = c->seg[j];
    for (int e = 0; e < 2; e++) { const hug_point* q = p + i + e;
      x0 = fmin(x0, q->x); x1 = fmax(x1, q->x); y0 = fmin(y0, q->y); y1 = fmax(y1, q->y); } }
  c->omax_r = 0;
  for (int j = 0; j < c->noth; j++) { const double* q = in->others + HUG_OTHER * c->oth[j];
    x0 = fmin(x0, q[0]); x1 = fmax(x1, q[0]); y0 = fmin(y0, q[1]); y1 = fmax(y1, q[1]);
    c->omax_r = fmax(c->omax_r, q[2]); }
  double cell = HUG_GRID;
  if (!(x1 >= x0)) { x0 = x1 = c->in->head_x; y0 = y1 = c->in->head_y; }
  while (((x1 - x0) / cell + 1) * ((y1 - y0) / cell + 1) > HUG_GRID_CELLS) cell *= 1.25;   // a big window: coarser cells
  c->gcell = cell; c->gx0 = x0; c->gy0 = y0;
  c->gw = (int)((x1 - x0) / cell) + 1; c->gh = (int)((y1 - y0) / cell) + 1;
  int cells = c->gw * c->gh;
  for (int pass = 0; pass < 2; pass++) {
    int* start = pass ? c->ocell_start : c->cell_start; int* items = pass ? c->ocell_items : c->cell_items;
    for (int k = 0; k <= cells; k++) start[k] = 0;
    int m = pass ? c->noth : c->nseg;
    for (int fill = 0; fill < 2; fill++) {
      for (int j = 0; j < m; j++) {
        double ax, ay, bx, by;
        if (pass) { const double* q = in->others + HUG_OTHER * c->oth[j]; ax = bx = q[0]; ay = by = q[1]; }
        else { int i = c->seg[j]; ax = fmin(p[i].x, p[i + 1].x); bx = fmax(p[i].x, p[i + 1].x); ay = fmin(p[i].y, p[i + 1].y); by = fmax(p[i].y, p[i + 1].y); }
        int i0 = (int)floor((ax - x0) / cell), i1 = (int)floor((bx - x0) / cell), j0 = (int)floor((ay - y0) / cell), j1 = (int)floor((by - y0) / cell);
        i0 = i0 < 0 ? 0 : i0; j0 = j0 < 0 ? 0 : j0; i1 = i1 >= c->gw ? c->gw - 1 : i1; j1 = j1 >= c->gh ? c->gh - 1 : j1;
        for (int jj = j0; jj <= j1; jj++) for (int ii = i0; ii <= i1; ii++) {
          int cc = jj * c->gw + ii;
          if (!fill) start[cc + 1]++;
          else if (start[cc] < HUG_GRID_ITEMS) items[start[cc]++] = pass ? c->oth[j] : c->seg[j];
        }
      }
      if (!fill) { for (int k = 0; k < cells; k++) start[k + 1] += start[k]; if (start[cells] > HUG_GRID_ITEMS) start[cells] = HUG_GRID_ITEMS; }
      else { for (int k = cells; k > 0; k--) start[k] = start[k - 1]; start[0] = 0; }
    }
  }
}

// The plain steering: aim a little ahead along the body, `reach` out on our side (round an end
// on a circle no tighter than the snake turns). Updates the side and direction kept in st.
static inline double hug_aim(const hug_ctx* c, hug_state* st, double hx, double hy, double ang, double* ax, double* ay) {
  const hug_input* in = c->in; double total = c->total;
  double s, tx, ty, side_sign;
  double dist = hug_near(c, hx, hy, &s, &tx, &ty, &side_sign);
  double reach = c->reach, look = c->look;
  // From further out, aim further along: come in at a slant (at most about a third of a right
  // angle), not straight at the body.
  if (dist - reach > look / HUG_APPROACH) look = (dist - reach) * HUG_APPROACH;
  if (!st->side || dist > in->body_radius * .5) st->side = (int)side_sign;
  double along = cos(ang) * tx + sin(ang) * ty;
  if (!st->dir) st->dir = along >= 0 ? 1 : -1;
  else if (along * st->dir < -.25) st->dir = -st->dir;
  // Towards the tail of a moving snake: its tail draws back along the body as fast as its head
  // goes, so round it and the snake is left far behind; turn back while there is still body
  // beside to come back along (what slides by over a U-turn, and a little more).
  if (st->dir < 0 && in->head_speed > 0 && !c->ring && in->step > 0 && s < HUG_PI * in->turn_radius / in->step * in->head_speed + c->reach)
    st->dir = 1;
  double at = s + st->dir * look;
  if (c->ring) {
    if (at > total) at -= total;
    if (at < 0) at += total;
  }
  double qx, qy, ux, uy;
  if (at >= 0 && at <= total) {
    hug_at(c, at, &qx, &qy, &ux, &uy);
    double bend = atan2(tx * uy - ty * ux, tx * ux + ty * uy) * st->dir;
    double away = -bend * st->side;
    double extra = away > 0 ? look * away / 8 * HUG_BEND_GAIN : 0;
    double to_head = total - at;
    double head_room = to_head < c->head_zone ? c->head_margin * (1 - to_head / c->head_zone) : 0;
    *ax = qx - uy * st->side * (reach + extra + head_room);
    *ay = qy + ux * st->side * (reach + extra + head_room);
  } else {
    double over = at > total ? at - total : -at;
    double cap = fmax(reach, in->turn_radius * 1.15);
    double ex, ey;
    hug_at(c, at > total ? total : 0, &ex, &ey, &ux, &uy);
    double ox = st->dir * ux, oy = st->dir * uy;
    double nx = -uy * st->side, ny = ux * st->side;
    double rx = hx - ex, ry = hy - ey;
    double past = rx * ox + ry * oy, beside = rx * nx + ry * ny;
    double psi = past > 0 ? atan2(past, beside) : 0;
    if (psi < 0) psi = 0;
    double lead = fmin(look / cap, .8);
    double phi = past > 0 ? fmin(HUG_PI, psi + lead) : 0;
    double ring = past > 0 ? cap / cos(lead / 2) : cap;
    double ahead = past > 0 ? 0 : over;
    if (at > total) { phi = 0; ring = cap + c->head_margin; ahead = past > 0 ? past + look : over; }
    if (at > total && c->overrun && past > 0) {
      // Out ahead of a head it is faster than: it cannot be kept beside, so turn back along the body.
      hug_at(c, total - look > 0 ? total - look : 0, &ex, &ey, &ux, &uy);
      *ax = ex - uy * st->side * (reach + c->head_margin);
      *ay = ey + ux * st->side * (reach + c->head_margin);
      return dist;
    }
    double cx = ex + nx * (reach - cap), cy = ey + ny * (reach - cap);
    *ax = cx + ring * (cos(phi) * nx + sin(phi) * ox) + ox * ahead;
    *ay = cy + ring * (cos(phi) * ny + sin(phi) * oy) + oy * ahead;
  }
  return dist;
}

// The nose's gap to the hugged body (and the gap wanted there), and the safety clearance: the
// least of that gap, the head's room inside the wall and the nose's gap to the other snakes;
// clear[0..2] as for hug_others_grid.
static inline void hug_eval(const hug_ctx* c, const hug_pose* p, double t, double* gap, double* want, double* clear) {
  const hug_input* in = c->in;
  double nx = p->x + cos(p->a) * c->nose, ny = p->y + sin(p->a) * c->nose;
  double s;
  double g = hug_near_grid(c, nx, ny, &s) - in->body_radius;
  double to_head = s < 0 ? 1e300 : c->total - s;
  *gap = g;
  *want = c->gap + (to_head < c->head_zone ? c->head_margin * (1 - to_head / c->head_zone) : 0);
  double base = g;
  if (in->wall_r > 0) {
    double inside = in->wall_r - hypot(p->x - in->wall_x, p->y - in->wall_y) - HUG_WALL_MARGIN;
    if (inside < base) base = inside;
  }
  clear[0] = clear[1] = clear[2] = base;
  if (c->noth) {
    double og[3];
    hug_others_grid(c, nx, ny, fmin(base, HUG_ROOM) + HUG_OTHERS_MARGIN + 1, t, og);
    for (int w = 0; w < 3; w++) if (og[w] - HUG_OTHERS_MARGIN < clear[w]) clear[w] = og[w] - HUG_OTHERS_MARGIN;
  }
}
// No further round than the snake can turn in one sending period: sent further, a late next
// angle (or a delay misjudged) would keep it turning on past what was planned.
// (Half a sendable step more, so that rounding to one never stops it short of turning its most.)
static inline double hug_reachable(const hug_ctx* c, double a, double want) {
  double d = hug_wrap(want - a), most = c->in->turn * c->period + HUG_PI / 251;
  return a + (d > most ? most : d < -most ? -most : d);
}
static inline void hug_move(const hug_input* in, hug_pose* p, double cmd) {
  double d = hug_wrap(cmd - p->a);
  p->a += fabs(d) < in->turn ? d : (d > 0 ? in->turn : -in->turn);
  if (p->v != p->goal) { double e = p->goal - p->v; p->v += fabs(e) <= in->step_rate ? e : (e > 0 ? in->step_rate : -in->step_rate); }
  p->x += cos(p->a) * p->v; p->y += sin(p->a) * p->v;
}

// Plays one choice forward: `first` for the first period, then `rate` (fraction of the most
// turning, as an angle sent each period) or, rate > 1, the plain steering. Returns the gap
// score over HUG_TRACK_STEPS; worst[0..2] get the least safety margins over `steps` (counting
// everything, leaving out where heads could swing to, only what is there now), worst[3] the
// same once turned a right angle round.
static inline double hug_roll(const hug_ctx* c, hug_pose p, int t0, hug_state st, double first, double rate, double need, int steps, double* worst) {
  const hug_input* in = c->in;
  double score = 0, prev = 1e300, cmd = first; worst[0] = worst[1] = worst[2] = worst[3] = 1e300;
  int every = steps > c->look_steps ? 4 : 2;            // (a U-turn is looked at more coarsely)
  int late = (int)(HUG_PI / 2 / in->turn);            // past a right angle round
  for (int k = 0; k < steps; k++) {
    if (k >= c->period && k % c->period == 0) {
      if (rate > 1) { double ax, ay; hug_aim(c, &st, p.x, p.y, p.a, &ax, &ay); cmd = hug_quant(hug_reachable(c, p.a, atan2(ay - p.y, ax - p.x))); }
      else cmd = hug_quant(hug_reachable(c, p.a, p.a + rate * (in->turn * c->period + HUG_PI / 251)));
    }
    hug_move(in, &p, cmd);
    if (k % every != every - 1) continue;
    double g, want, clear[3];
    hug_eval(c, &p, t0 + k + 1, &g, &want, clear);
    double closing = prev < 1e299 && g < prev ? (prev - g) / every : 0;   // per step
    prev = g;
    double spare = need + (HUG_TIMING + c->period - HUG_PERIOD + HUG_DELAY_DOUBT * in->pre_count) * closing;
    for (int w = 0; w < 3; w++) if (clear[w] - spare < worst[w]) worst[w] = clear[w] - spare;
    if (k >= late && clear[0] - spare < worst[3]) worst[3] = clear[0] - spare;   // later on
    if (k < HUG_TRACK_STEPS) {
      double e = g - want, w = 10.0 / (10 + k);           // sooner counts more: no putting it off
      if (e > HUG_FAR_ERROR) e = HUG_FAR_ERROR;
      score += w * (e < 0 ? 4 * e * e : e * e);
    }
  }
  return score;
}

// Where a moving head can be over the next delay + HUG_HEAD_STEPS steps, for `others`: on its
// way (turning towards wang at its rate) and, as it can swing round at any moment, turning
// either way as hard as it can, half as hard, or going straight on (those points marked as
// swings); `delay`: the steps until an angle sent now takes effect. A head lately turning
// (`spin` a step) is also followed on round as it goes, further ahead (HUG_CURL_STEPS): a snake
// curling round is the one that closes in on what is beside it. Each point counts from the step
// the head could get there (its body stays there). A point every 4 steps.
static inline int hug_head_paths(double* others, int count, int max, double x, double y, double ang, double wang, double turn, double step, int delay, double radius, double spin) {
  static const double swings[] = {1, -1, .5, -.5, 0};   // turning this hard (of the most it can), held
  if (spin > turn) spin = turn;
  if (spin < -turn) spin = -turn;
  bool curling = fabs(spin) > .3 * turn;
  for (int way = 0; way < 7; way++) {
    if (way == 6 && !curling) break;
    double px = x, py = y, a = ang;
    int steps = delay + (way == 6 ? HUG_CURL_STEPS : HUG_HEAD_STEPS);
    for (int k = 1; k <= steps && count < max; k++) {
      if (way == 0) { double d = hug_wrap(wang - a); a += fabs(d) < turn ? d : (d > 0 ? turn : -turn); }
      else if (way == 6) a += spin;
      else a += swings[way - 1] * turn;
      px += cos(a) * step; py += sin(a) * step;
      if (k % 4 == 0) {
        double* o = others + HUG_OTHER * count++;
        o[0] = px; o[1] = py; o[2] = radius; o[3] = k; o[4] = way == 0 || way == 6 ? 1 : 2;
      }
    }
  }
  return count;
}

// The planning proper, from the poses at which the angle sent now takes effect (hp[0]; the
// others should the delay be a little off), with the speed easing towards `goal` from there.
// Returns the angle to send; margin and tier as in hug_output.
static inline double hug_plan(hug_ctx* c, const hug_state* st, const hug_pose* hp0, const int* ht, int nh, double goal, double wanted, double* margin_out, int* tier_out) {
  const hug_input* in = c->in;
  hug_pose hp[3];
  for (int h = 0; h < nh; h++) { hp[h] = hp0[h]; hp[h].goal = goal; }
  hug_pose p = hp[0];
  double need = c->need;                              // the nose stays at least this far out of a hitbox
  // (Planning lightly: the first few of each, and the delay doubt only for the way out.)
  static const double firsts[] = {0, -1, 1, -.5, .5, -.15, .15, -.75, .75, -.3, .3, -.05, .05};
  static const double rates[] = {2, 0, -1, 1, -.5, .5, -.2, .2};   // 2: the plain steering
  const int nf = in->light ? 8 : (int)(sizeof firsts / sizeof *firsts) + 1;
  const int nr = in->light ? 4 : (int)(sizeof rates / sizeof *rates);
  double g0, w0, c0[3]; hug_eval(c, &p, ht[0], &g0, &w0, c0);
  bool coming_in = g0 - w0 > HUG_FAR_ERROR;
  double tried[32];
  double best_score = 1e300, best_cmd = hug_quant(hug_reachable(c, p.a, wanted)), best_margin = -1e300; int best_tier = 5;
  // (A way out that only just clears something further round is not one to count on: the next
  // look starts a period later, from a little further in.)
  double want_room = HUG_ROOM_SPARE + fmax(p.v, goal) * c->period;
  for (int f = 0; f < nf; f++) {
    double cmd = f == 0 ? hug_quant(hug_reachable(c, p.a, wanted)) : hug_quant(hug_reachable(c, p.a, p.a + firsts[f - 1] * (in->turn * c->period + HUG_PI / 251)));
    bool seen = false;                                  // several choices can come to the same sendable direction
    for (int j = 0; j < f; j++) if (tried[j] == cmd) seen = true;
    tried[f] = cmd;
    if (seen) continue;
    // [0]: counting where nearby heads could swing to, [1]: where they are going, [2]: only what is there now
    double track[3] = {1e300, 1e300, 1e300}, safe[3] = {-1e300, -1e300, -1e300}, out_way[3] = {-1e300, -1e300, -1e300}, plain = 0;
    for (int r = 0; r < nr; r++) {
      double worst[4], sc = hug_roll(c, p, ht[0], *st, cmd, rates[r], need, c->look_steps, worst);
      for (int h = 1; h < (in->light ? 1 : nh); h++) {  // and should the delay be a little off
        double w2[4]; hug_roll(c, hp[h], ht[h], *st, cmd, rates[r], need, c->look_steps, w2);
        for (int w = 0; w < 3; w++) if (w2[w] < worst[w]) worst[w] = w2[w];
      }
      if (r == 0) plain = worst[0];                     // carrying on with the plain steering
      for (int w = 0; w < 3; w++) {
        double t = sc + (worst[w] < 0 ? 1e5 + 1e4 * -worst[w] : 0);   // this way of carrying on runs into something
        if (t < track[w]) track[w] = t;
        if (worst[w] > safe[w]) safe[w] = worst[w];
      }
    }
    // A U-turn either way: clear all the way? And so if the angle takes effect a little sooner
    // or later than reckoned (the delay is measured, not known).
    double room = 1e300;                                // how much room the way out keeps further on
    for (int h = 0; h < nh; h++) {
      double way[3] = {-1e300, -1e300, -1e300}, late = -1e300;
      for (int r = -1; r <= 1; r += 2) {
        double worst[4]; hug_roll(c, hp[h], ht[h], *st, cmd, r, need, c->circle_steps, worst);
        for (int w = 0; w < 3; w++) if (worst[w] > way[w]) way[w] = worst[w];
        if (worst[0] >= 0 && worst[3] > late) late = worst[3];
      }
      for (int w = 0; w < 3; w++) if (h == 0 || way[w] < out_way[w]) out_way[w] = way[w];
      if (late < room) room = late;
    }
    // Best: a way out (a U-turn) stays clear whatever nearby heads do: then hug as closely as
    // possible. Else, while a way out stays clear of where the heads are going, dodge their
    // swings. Else, while some way of carrying on stays clear of that over the look-ahead,
    // make room for a way out; else keep the most room, first from what is there now.
    double score, margin; int tier;
    if (out_way[0] >= 0 && room >= want_room) { score = track[0]; margin = out_way[0]; tier = 0; }
    else if (out_way[0] >= 0) { score = track[0] + HUG_ROOM_COST * (want_room - room); margin = room - want_room; tier = 1; }   // still hug, keeping what room it can
    else if (out_way[1] >= 0) { score = 1e6 + track[1] + HUG_DODGE_COST * -out_way[0]; margin = out_way[0]; tier = 2; }   // hug on, out of the swings' way
    else if (safe[1] >= HUG_SAFE_ENOUGH) { score = 2e6 + track[1] + HUG_DODGE_COST * fmax(0, -out_way[1]) * .1; margin = out_way[1]; tier = 3; }   // no way out, but clear well ahead: hug on (it looks again every period)
    else if (safe[1] >= 0) { score = 2e8 - 1e5 * fmin(safe[1], 30) - 1e3 * out_way[1] + track[1] * 1e-3; margin = out_way[1]; tier = 3; }
    else if (safe[2] >= 0) { score = 3e8 - 1e5 * safe[1]; margin = safe[1]; tier = 4; }
    else { score = 4e8 - 1e5 * safe[2]; margin = safe[2]; tier = 5; }
    if (f == 0 && out_way[0] >= 0 && plain >= HUG_FAR_ERROR / 4 && coming_in) score = -1;   // still coming in: the plain steering, while it keeps well clear
    if (score < best_score) { best_score = score; best_cmd = cmd; best_margin = margin; best_tier = tier; }
  }
  *margin_out = best_margin; *tier_out = best_tier;
  return best_cmd;
}

static inline bool body_hug_steer(const hug_input* in, hug_state* st, hug_output* out) {
  static hug_ctx ctx;
  hug_ctx* c = &ctx;
  int n = in->count;
  if (n < 2) return false;
  if (n > HUG_MAX_POINTS) n = HUG_MAX_POINTS;
  c->in = in; c->n = n;
  c->cum[0] = 0;
  for (int i = 1; i < n; i++)
    c->cum[i] = c->cum[i - 1] + hypot(in->body[i].x - in->body[i - 1].x, in->body[i].y - in->body[i - 1].y);
  c->total = c->cum[n - 1];
  if (!(c->total > 0)) return false;
  c->ring = hypot(in->body[0].x - in->body[n - 1].x, in->body[0].y - in->body[n - 1].y) < 2 * in->body_radius + 20;
  // The hard limit: HUG_NEED out of every hitbox (a gap set below 0 lets it in that far), plus
  // what the motion has lately been off by; the gap kept stays above that.
  double noise = in->noise > 0 ? fmin(in->noise, 8) : 0;
  c->need = (in->gap < 0 ? in->gap : 0) + HUG_NEED + HUG_NOISE_SPARE * noise;
  c->gap = fmax(in->gap, c->need);
  c->reach = in->body_radius + c->gap;
  if (c->reach < 1) c->reach = 1;
  c->look = in->look > 40 ? in->look : 40;
  c->nose = HUG_NOSE * in->my_radius;
  bool plan = in->step > 0 && in->turn > 0;
  c->period = in->period > HUG_PERIOD ? (in->period < 16 ? in->period : 16) : HUG_PERIOD;
  // Look far enough ahead to see the snake turn a right angle away (big snakes turn slowly).
  c->look_steps = plan ? (int)(HUG_PI / 2 / in->turn) + 12 : 0;
  if (c->look_steps < HUG_LOOK_STEPS) c->look_steps = HUG_LOOK_STEPS;
  if (c->look_steps > HUG_LOOK_MAX) c->look_steps = HUG_LOOK_MAX;
  // A way out: turning as hard as it can until it has turned right round (a U-turn).
  c->circle_steps = plan ? (int)(HUG_PI / in->turn) + 8 + 3 * c->period : 0;
  if (c->circle_steps > HUG_CIRCLE_MAX) c->circle_steps = HUG_CIRCLE_MAX;
  // The speeds: now, where the boost already sent takes it, without boosting, and boosting.
  bool eases = in->step_rate > 0;
  double v_sent = eases && in->step_goal > 0 ? in->step_goal : in->step;
  double v_normal = eases && in->step_normal > 0 ? in->step_normal : v_sent;
  double v_boost = eases && in->step_boost > 0 ? in->step_boost : 0;
  double v_most = fmax(in->step, fmax(v_sent, fmax(v_normal, v_boost)));
  // Where the head will be when the angle sent now reaches the server.
  hug_pose now = {in->head_x, in->head_y, in->ang, in->step, v_sent}, p = now;
  // ... and where, should the delay be a fifth shorter or longer (taking effect sooner, or the
  // last angle sent going on for longer).
  hug_pose hp[3]; int ht[3], nh = 1;
  int d_short = (int)lround(in->pre_count * (1 - HUG_DELAY_DOUBT)), d_long = (int)lround(in->pre_count * (1 + HUG_DELAY_DOUBT));
  if (plan) for (int k = 0; k < in->pre_count; k++) {
    if (k == d_short && d_short < in->pre_count) { hp[nh] = p; ht[nh] = k; nh++; }
    hug_move(in, &p, in->pre_ang[k]);
  }
  hp[0] = p; ht[0] = in->pre_count;
  if (plan && d_long > in->pre_count) {
    hug_pose q = p;
    for (int k = in->pre_count; k < d_long; k++) hug_move(in, &q, in->pre_ang[in->pre_count - 1]);
    hp[nh] = q; ht[nh] = d_long; nh++;
  }
  // Only what can be reached matters.
  double reach_out = (plan ? in->pre_count * v_most + fmax(c->look_steps * v_most, 2.2 * in->turn_radius * v_most / in->step) : 0) + c->look + c->reach + 2 * fmax(c->reach, 1.15 * in->turn_radius) + 120;
  double w2 = reach_out * reach_out;
  c->nseg = 0;
  for (int i = 0; i + 1 < n; i++) {
    const hug_point* a = in->body + i, *b = a + 1;
    double dx = b->x - a->x, dy = b->y - a->y, ll = dx * dx + dy * dy;
    double t = ll > 0 ? ((now.x - a->x) * dx + (now.y - a->y) * dy) / ll : 0;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    double ex = a->x + dx * t - now.x, ey = a->y + dy * t - now.y;
    if (ex * ex + ey * ey <= w2) c->seg[c->nseg++] = i;
  }
  if (!c->nseg) for (int i = 0; i + 1 < n; i++) c->seg[c->nseg++] = i;
  c->noth = 0;
  for (int i = 0; i < in->others_count && c->noth < HUG_MAX_OTHERS; i++) {
    const double* o = in->others + HUG_OTHER * i;
    double dx = o[0] - now.x, dy = o[1] - now.y, r = reach_out + o[2];
    if (dx * dx + dy * dy <= r * r) c->oth[c->noth++] = i;
  }
  hug_grid_build(c);
  // The plain steering from there: the aim point, and the side and direction to keep. (A moving
  // head's threat is in `others`: where it is going and could swing to, from when it could be there.)
  double ax, ay;
  c->head_margin = HUG_HEAD_MARGIN; c->head_zone = HUG_HEAD_ZONE;
  c->overrun = in->head_speed > 0 && v_normal > in->head_speed * 1.05;
  hug_aim(c, st, p.x, p.y, p.a, &ax, &ay);
  out->tx = ax; out->ty = ay;
  out->angle = out->wanted = atan2(ay - p.y, ax - p.x);
  out->margin = 0; out->tier = 0;
  bool front = false;
  {
    // Faster than the hugged head and close beside its front part, going its way (or out ahead
    // of the head already, going on its way): boosting on would only carry the snake out ahead of
    // the head, into its way, so the boost is let go (until well back from the head, or no longer
    // going its way).
    double s, tx, ty, sd, d = hug_near(c, p.x, p.y, &s, &tx, &ty, &sd), to_head = c->total - s;
    double hx, hy, ux, uy; hug_at(c, c->total, &hx, &hy, &ux, &uy);
    bool ahead = (p.x - hx) * ux + (p.y - hy) * uy > 0;
    bool running_ahead = ahead && (d < c->reach + 300 || cos(p.a) * ux + sin(p.a) * uy > .5);
    front = st->dir > 0 && in->head_speed > 0 && !c->ring && (d < c->reach + 150 || running_ahead)
            && to_head < HUG_SLOW_ZONE + (st->slow ? 120 : 0) && v_boost > in->head_speed * 1.1;
  }
  bool slow = false;
  if (plan) {
    double margin, b_margin = -1e300, b_cmd = 0; int tier, b_tier = 99;
    if (v_boost > 0 && !front) {
      // Boosting on: as long as that leaves a way out (having let it go, only once it leaves one
      // whatever nearby heads do); else let it go and steer for the slower speed.
      b_cmd = hug_plan(c, st, hp, ht, nh, v_boost, out->wanted, &b_margin, &b_tier);
      if (b_tier <= (st->slow ? 0 : 1)) { out->angle = b_cmd; out->margin = b_margin; out->tier = b_tier; }
      else slow = true;
    } else slow = v_boost > 0;
    if (v_boost <= 0 || slow) {
      out->angle = hug_plan(c, st, hp, ht, nh, v_normal, out->wanted, &margin, &tier);
      out->margin = margin; out->tier = tier;
      // In trouble without the boost (a head swinging round, say): boosting on may be the way out.
      if (v_boost > 0 && tier >= 2) {
        if (b_tier == 99) b_cmd = hug_plan(c, st, hp, ht, nh, v_boost, out->wanted, &b_margin, &b_tier);
        if (b_tier < tier || (b_tier == tier && b_margin > margin + 2)) { out->angle = b_cmd; out->margin = b_margin; out->tier = b_tier; slow = false; }
      }
    }
  } else slow = front;
  st->slow = slow;
  out->slow = slow;
  {
    double s, tx, ty, sd;
    out->distance = hug_near(c, now.x, now.y, &s, &tx, &ty, &sd);
    double nx = now.x + cos(now.a) * c->nose, ny = now.y + sin(now.a) * c->nose;
    out->edge_gap = sqrt(hug_poly_dist2(in->body, n, nx, ny)) - in->body_radius;
  }
  return true;
}
#endif
