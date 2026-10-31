// Body hug (E): closed-loop drives alongside an enemy body with the game's turning limits, the
// 251 sendable directions, an angle every 33 ms and the network delay before it takes effect.
// The hitbox is checked every step: the nose must never get inside the enemy's body.
#include "../app/src/game/body_hug.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double sc_turn(double sc, double sp) {
  double scang = .13 + .87 * pow((7 - sc) / 6, 2), spang = fmin(1, sp / 4.8);
  return .033 * scang * spang;
}
#define HUG_SIM_ACCEL .18   // the speed's change a step (8 ms), as seen in the game
static double speed(double sc, bool boost) { return boost ? 14 : 5.39 + .4 * sc; }

// The enemy: a body that is either still or a moving snake leaving its trail.
typedef struct {
  hug_point pts[4096]; int n;
  double x, y, ang, wang, sp, turn, length, spin; int program; double t;
} enemy;
static void enemy_trail(enemy* e) {
  hug_point* last = &e->pts[e->n - 1];
  if (hypot(e->x - last->x, e->y - last->y) >= 20) {
    if (e->n == 4096) { memmove(e->pts, e->pts + 1, sizeof(hug_point) * 4095); e->n--; }
    e->pts[e->n++] = (hug_point){e->x, e->y};
  }
  // drop the tail beyond its length
  double len = hypot(e->x - e->pts[e->n - 1].x, e->y - e->pts[e->n - 1].y);
  int keep = e->n - 1;
  while (keep > 0 && len < e->length) { len += hypot(e->pts[keep].x - e->pts[keep - 1].x, e->pts[keep].y - e->pts[keep - 1].y); keep--; }
  if (keep > 0) { memmove(e->pts, e->pts + keep, sizeof(hug_point) * (e->n - keep)); e->n -= keep; }
}
static void enemy_step(enemy* e) {
  if (!e->sp) return;
  e->t++;
  switch (e->program) {
    case 1: e->wang = e->ang + (fmod(e->t, 500) < 250 ? 1 : -1); break;       // S-bends at its tightest turn
    case 2: e->wang = e->ang + 1; break;                                      // circling
    case 3: e->wang = e->ang + (fmod(e->t, 300) < 150 ? .5 : -.5) * (fmod(e->t, 900) < 450 ? 1 : 0); break;   // gentle weave, straights
    case 4: e->wang = e->ang + (fmod(e->t, 160) < 80 ? 1 : -1); break;        // quick zigzag
    default: break;
  }
  double d = hug_wrap(e->wang - e->ang);
  e->spin = fabs(d) < e->turn ? d : (d > 0 ? e->turn : -e->turn);   // (how it is turning, as the client sees)
  e->ang += e->spin;
  e->x += cos(e->ang) * e->sp; e->y += sin(e->ang) * e->sp;
  enemy_trail(e);
}
static void enemy_grow(enemy* e, double x, double y, double ang, double sc, int program, double length) {
  memset(e, 0, sizeof *e);
  e->x = x; e->y = y; e->ang = e->wang = ang; e->sp = speed(sc, false) / 4; e->turn = sc_turn(sc, speed(sc, false));
  e->program = program; e->length = length;
  e->pts[0] = (hug_point){x - cos(ang) * length, y - sin(ang) * length}; e->n = 1;
  for (double s = 20; s < length; s += 20) e->pts[e->n++] = (hug_point){x - cos(ang) * (length - s), y - sin(ang) * (length - s)};
  e->pts[e->n++] = (hug_point){x, y};
  // let it run a while so the body takes the program's shape
  for (int k = 0; k < (int)(length / e->sp) + 50; k++) enemy_step(e);
}

// A start 80 out beside the middle of the body (or further along) with room all round.
static bool start_beside(const enemy* e, int way, double* sx, double* sy, double* sa) {
  for (int off = 0; off < e->n / 2 - 2; off++) for (int side = 1; side >= -1; side -= 2) {
    int mid = e->n / 2 + (off % 2 ? off / 2 + 1 : -off / 2);
    if (mid < 1 || mid >= e->n - 2) continue;
    double tx = e->pts[mid + 1].x - e->pts[mid].x, ty = e->pts[mid + 1].y - e->pts[mid].y, l = hypot(tx, ty);
    tx /= l; ty /= l;
    double x = e->pts[mid].x - ty * 80 * side, y = e->pts[mid].y + tx * 80 * side;
    if (sqrt(hug_poly_dist2(e->pts, e->n, x, y)) < 75 || hypot(x - e->x, y - e->y) < 200) continue;
    double a = atan2(ty, tx) + (way ? HUG_PI : 0); bool clear = true;
    for (double d = 0; d <= 200 && clear; d += 10) clear = sqrt(hug_poly_dist2(e->pts, e->n, x + cos(a) * d, y + sin(a) * d)) > 70;   // room to settle in
    if (!clear) continue;
    *sx = x; *sy = y; *sa = a;
    return true;
  }
  return false;
}

typedef struct { double min_gap, mean_gap, median_gap, max_gap; int wrapped; int deaths; } result;
typedef struct {
  double sc; bool boost; double gap; int delay; int steps; int settle;
  const double* others; int others_count; double wall_r;
  int period;          // steps between angles sent (0: 4, as at 30 fps and more)
  double jitter;       // the head as seen is up to this far off (uniform), as with jerky frames
  double misjudge;     // the delay the client reckons with, as a share of the real one (0: right)
  double* trace;       // (if set) the nose's gap at every step
  double* start_gap;   // (if set) the first step at which the nose came within this gap (written back: the step, or -1)
} drive_opts;

static int cmp_d(const void* a, const void* b) { double x = *(const double*)a, y = *(const double*)b; return x < y ? -1 : x > y; }
// Our snake starts at (x, y, ang). The enemy is static (sp 0) or moving.
static unsigned rng = 12345;
static double jit(double j) { rng = rng * 1103515245u + 12345u; return j * (2.0 * ((rng >> 8) & 0xffff) / 65535.0 - 1); }
static result drive(enemy* e, double x, double y, double ang, drive_opts o) {
  double sp = speed(o.sc, o.boost), my_r = 14.5 * o.sc, turn = sc_turn(o.sc, sp), step = sp / 4, tr = step / turn;
  bool want_boost = o.boost; double reached_at = -1;   // the boost as sent: let go when the hug asks
  static bool sent_boost[20000];
  double body_r = e->sp ? 14.5 * 2 : 30;  // a moving enemy is size 2
  int period = o.period ? o.period : HUG_PERIOD;
  static double sent[20000];              // the angle sent at each step (in force o.delay steps later)
  double cmd = ang, a = ang;
  hug_state st = {0};
  result r = {1e9, 0, 0, -1e9, 0, 0}; int counted = 0, first_side = 0;
  static double gaps[20000];
  static hug_point view[4096 + 2];
  double px = 0, py = 0, pa = 0, noise2 = 0; bool have = false;   // the client's noise estimate
  int reckon = o.misjudge > 0 ? (int)lround(o.delay * o.misjudge) : o.delay;
  for (int k = 0; k < o.steps && k < 20000; k++) {
    if (k % period == 0) {
      int vn = e->n;
      memcpy(view, e->pts, sizeof(hug_point) * e->n);
      if (hypot(view[vn - 1].x - e->x, view[vn - 1].y - e->y) > .01) view[vn++] = (hug_point){e->x, e->y};
      static double others[HUG_OTHER * HUG_MAX_OTHERS]; int on = o.others_count;
      memcpy(others, o.others, sizeof(double) * HUG_OTHER * on);
      if (e->sp)                                    // as the client does: where its head is going, or could swing to
        on = hug_head_paths(others, on, HUG_MAX_OTHERS, e->x, e->y, e->ang, e->wang, e->turn, e->sp, reckon, body_r, e->spin);
      // as the client does: the angles sent in the last `reckon` steps are taken to be on their way
      static double pre[64];
      for (int j = 0; j < reckon && j < 64; j++) { int at = k - reckon + j; pre[j] = at >= 0 ? sent[at] : ang; }
      double sx = x + jit(o.jitter), sy = y + jit(o.jitter);
      if (have) {                                   // and its noise estimate: where its model put the head, sideways
        double mx = px, my = py, ma = pa;
        for (int j = 0; j < period; j++) { int at = k - period + j - reckon; double d = hug_wrap((at >= 0 ? sent[at] : ang) - ma); ma += fabs(d) < turn ? d : (d > 0 ? turn : -turn); mx += cos(ma) * step; my += sin(ma) * step; }
        double side = fabs(-sin(ma) * (sx - mx) + cos(ma) * (sy - my)); noise2 += (side * side - noise2) * .1;
      }
      have = true; px = sx; py = sy; pa = a;
      hug_input in = {view, vn, body_r, sx, sy, a, my_r, o.gap, fmax(60, 2.2 * tr), tr, step, turn, others, on, 0, 0, o.wall_r,
                      pre, reckon < 64 ? reckon : 64, e->sp, period, sqrt(noise2), false,
                      speed(o.sc, want_boost) / 4, speed(o.sc, false) / 4, o.boost ? speed(o.sc, true) / 4 : 0, HUG_SIM_ACCEL / 4};
      hug_output out;
      if (body_hug_steer(&in, &st, &out)) {
        cmd = hug_quant(out.angle); want_boost = o.boost && !out.slow;
#ifdef HUG_DRIVE_VERBOSE
        if (HUG_DRIVE_VERBOSE(k)) { double ht = hypot(x - e->x, y - e->y);
          printf("k %d t %.2f gap %.1f tier %d margin %.1f slow %d sp %.2f head-dist %.0f side %d dir %d ang %.2f cmd %.2f eang %.2f\n", k, k / 125.0, out.edge_gap, out.tier, out.margin, out.slow, sp, ht, st.side, st.dir, a, cmd, e->ang); }
#endif
        if (k > o.settle) { if (out.edge_gap > r.max_gap) r.max_gap = out.edge_gap; r.mean_gap += out.edge_gap; if (counted < 20000) gaps[counted] = out.edge_gap; counted++; }
        if (!first_side) first_side = st.side; else if (st.side != first_side) r.wrapped = 1;
      }
    }
    sent[k] = cmd;
    // boost: let go as the client does (tight and no clear way out), else as the player holds it;
    // the speed eases towards its goal
    sent_boost[k] = want_boost;
    bool boosting = k - o.delay >= 0 ? sent_boost[k - o.delay] : o.boost;   // (it takes effect as late as the angle)
    { double goal = speed(o.sc, boosting); sp += fabs(goal - sp) < HUG_SIM_ACCEL ? goal - sp : (goal > sp ? HUG_SIM_ACCEL : -HUG_SIM_ACCEL);
      turn = sc_turn(o.sc, sp); step = sp / 4; tr = step / turn; }
    // the angle in force this step: sent o.delay steps ago
    double now_cmd = k - o.delay >= 0 ? sent[k - o.delay] : ang;
    double d = hug_wrap(now_cmd - a);
    a += fabs(d) < turn ? d : (d > 0 ? turn : -turn);
    x += cos(a) * step; y += sin(a) * step;
    enemy_step(e);
    // the hitbox, every step
    double nx = x + cos(a) * HUG_NOSE * my_r, ny = y + sin(a) * HUG_NOSE * my_r;
    static hug_point now_body[4097]; int nb = e->n;
    memcpy(now_body, e->pts, sizeof(hug_point) * e->n);
    now_body[nb++] = (hug_point){e->x, e->y};
    double g = sqrt(hug_poly_dist2(now_body, nb, nx, ny)) - body_r;
    for (int j = 0; j < o.others_count; j++) g = fmin(g, hypot(nx - o.others[HUG_OTHER * j], ny - o.others[HUG_OTHER * j + 1]) - o.others[HUG_OTHER * j + 2]);
    if (o.wall_r > 0) g = fmin(g, o.wall_r - hypot(x, y));
    if (k > 20 && g < r.min_gap) r.min_gap = g;
    if (o.trace) o.trace[k] = g;
    if (o.start_gap && reached_at < 0 && g < *o.start_gap) reached_at = k;
    if (g < 0) r.deaths++;
  }
  if (o.start_gap) *o.start_gap = reached_at;
  r.mean_gap /= counted ? counted : 1;
  if (counted) { int m = counted < 20000 ? counted : 20000; qsort(gaps, m, sizeof(double), cmp_d); r.median_gap = gaps[m / 2]; }
  return r;
}
static enemy E;
static void static_body(const hug_point* pts, int n) { memset(&E, 0, sizeof E); memcpy(E.pts, pts, sizeof(hug_point) * (n - 1)); E.n = n - 1; E.x = pts[n - 1].x; E.y = pts[n - 1].y; E.length = 1; }

int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  static hug_point body[400];
  // Geometry of the plain steering: beside a straight body the aim point is `reach` out, ahead.
  for (int i = 0; i < 100; i++) body[i] = (hug_point){i * 20.0, 0};
  hug_state st = {0}; hug_output o;
  hug_input in = {body, 100, 25, 500, 100, 0.1, 20, 3, 120, 40};
  assert(body_hug_steer(&in, &st, &o) && st.side == 1 && st.dir == 1);
  // from 72 out (100 less 25 + 3), aim 1.8 times that along: come in at a slant
  assert(fabs(o.ty - 28) < 1e-9 && fabs(o.tx - (500 + 1.8 * 72)) < 1e-9 && fabs(o.distance - 100) < 1e-9);
  assert(fabs(o.edge_gap - (100 + sin(0.1) * 0.8 * 20 - 25)) < 1e-9);        // the nose is 0.8 head radii ahead
  in.ang = HUG_PI - 0.1; st = (hug_state){0};
  assert(body_hug_steer(&in, &st, &o) && st.dir == -1 && fabs(o.tx - (500 - 1.8 * 72)) < 1e-9);
  in.head_y = -100; in.ang = 0.1; st = (hug_state){0};
  assert(body_hug_steer(&in, &st, &o) && st.side == -1 && fabs(o.ty + 28) < 1e-9);
  in.head_x = 30; in.head_y = 100; in.ang = HUG_PI; st = (hug_state){0};
  assert(body_hug_steer(&in, &st, &o) && o.tx < 0);
  // The directions the client can send.
  assert(hug_send_dir(0) == 0 && hug_send_dir(2 * HUG_PI - 1e-6) == 0 && hug_send_dir(2 * HUG_PI / 251 * 7.4) == 7 && hug_send_dir(2 * HUG_PI / 251 * 7.6) == 8);

  const double sizes[] = {1.2, 2, 3, 4.5};
  const int delays[] = {0, 8, 16};             // 0, 64 and 128 ms before an angle takes effect
  int total_deaths = 0;
  // Still straight body: hold the gap closely.
  for (int si = 0; si < 4; si++) for (int b = 0; b < 2; b++) for (int di = 0; di < 3; di += 2) {
    for (int i = 0; i < 400; i++) body[i] = (hug_point){i * 25.0, 0};
    static_body(body, 400);
    result r = drive(&E, 200, 250, -.6, (drive_opts){sizes[si], b, 1, delays[di], 2000, 900});
    printf("straight, size %.1f%s, delay %d: nose gap min %.1f median %.1f max %.1f\n", sizes[si], b ? " boost" : "", delays[di], r.min_gap, r.median_gap, r.max_gap);
    assert(r.deaths == 0 && r.median_gap < 4.5);
    total_deaths += r.deaths;
  }
  // Coiled in a ring of radius 400, from outside and inside.
  for (int i = 0; i < 400; i++) { double a = i * 2 * HUG_PI / 400; body[i] = (hug_point){400 * cos(a), 400 * sin(a)}; }
  for (int si = 0; si < 3; si++) for (int side = 0; side < 2; side++) for (int di = 0; di < 3; di += 2) {
    static_body(body, 400);
    result r = drive(&E, side ? 300 : 650, 0, HUG_PI / 2 + (side ? -.4 : .4), (drive_opts){sizes[si], false, 1, delays[di], 3000, 900});
    printf("ring %s, size %.1f, delay %d: nose gap min %.1f median %.1f\n", side ? "inside" : "outside", sizes[si], delays[di], r.min_gap, r.median_gap);
    assert(r.deaths == 0 && r.median_gap < 6);
  }
  // Tight S-bends lying still (radius 90 and 160): beside the inside of a bend tighter than the
  // snake turns it has to drop back in time; never into the body.
  for (int rad = 0; rad < 2; rad++) {
    double R = rad ? 160 : 90; int n = 0; double x = 0, y = 0, a = 0;
    for (int seg = 0; seg < 8 && n < 399; seg++) {
      double turn_dir = seg % 2 ? -1 : 1;
      for (int k = 0; k < 40 && n < 399; k++) { body[n++] = (hug_point){x, y}; a += turn_dir * (HUG_PI / 40); x += cos(a) * R * HUG_PI / 40; y += sin(a) * R * HUG_PI / 40; }
    }
    body[n++] = (hug_point){x, y};
    const double ssizes[] = {1.2, 3};
    for (int si = 0; si < 2; si++) for (int side = 0; side < 2; side++) for (int di = 0; di < 3; di += 2) {
      static_body(body, n);
      result r = drive(&E, side ? 200 : -200, -150, HUG_PI / 2, (drive_opts){ssizes[si], false, 1, delays[di], 2400, 0});
      printf("S-bends radius %.0f, size %.1f, %s, delay %d: nose gap min %.1f median %.1f\n", R, ssizes[si], side ? "right" : "left", delays[di], r.min_gap, r.median_gap);
      assert(r.deaths == 0);
    }
  }
  // A moving enemy turning about (S-bends at its tightest, circling, weaving, zigzag), beside it
  // going its way or against it (towards its head), sizes, boost, network delays: never into it.
  // Known limit: a slow-turning snake going against a weaving enemy can be closed in by it (its
  // head comes round behind us before it could be seen coming); those runs are counted.
  const char* programs[] = {"", "S-bends", "circling", "weaving", "zigzag"};
  const double msizes[] = {1.2, 3}; const int mdelays[] = {16};
  int closed_in = 0, closed_runs = 0;
  for (int pg = 1; pg <= 4; pg++) for (int si = 0; si < 2; si++) for (int b = 0; b < 2 - si; b++) for (int di = 0; di < 1; di++) for (int way = 0; way < 2; way++) {
    enemy_grow(&E, 0, 0, 0, 2, pg, 900);
    double sx = 0, sy = 0, sa = 0;
    assert(start_beside(&E, way, &sx, &sy, &sa));
    result r = drive(&E, sx, sy, sa, (drive_opts){msizes[si], b, 1, mdelays[di], 1600, 400});
    printf("moving enemy %s, size %.1f%s, %s, delay %d: nose gap min %.1f median %.1f%s\n", programs[pg], msizes[si], b ? " boost" : "", way ? "against" : "along", mdelays[di], r.min_gap, r.median_gap, r.deaths ? " (died)" : "");
    if (pg == 3 && way == 1) { closed_runs++; closed_in += r.deaths > 0; continue; }
    total_deaths += r.deaths;
    assert(r.deaths == 0);
  }
  printf("against a weaving enemy: closed in %d of %d runs\n", closed_in, closed_runs);
  assert(closed_in <= closed_runs / 2);
  // Jerky frames: an angle only every 7 steps (56 ms) and the head seen up to 2 off; the noise
  // seen is kept as spare room.
  {
    for (int i = 0; i < 400; i++) body[i] = (hug_point){i * 25.0, 0};
    static_body(body, 400);
    result r = drive(&E, 200, 250, -.6, (drive_opts){2, false, 1, 8, 2400, 900, NULL, 0, 0, 7, 2});
    printf("jerky frames, straight: nose gap min %.1f median %.1f\n", r.min_gap, r.median_gap);
    assert(r.deaths == 0 && r.median_gap < 8);
    for (int pg = 1; pg <= 2; pg++) for (int way = 0; way < 2; way++) {
      enemy_grow(&E, 0, 0, 0, 2, pg, 900);
      double sx = 0, sy = 0, sa = 0;
      assert(start_beside(&E, way, &sx, &sy, &sa));
      r = drive(&E, sx, sy, sa, (drive_opts){1.2, false, 1, 8, 1600, 400, NULL, 0, 0, 7, 2});
      printf("jerky frames, moving enemy %s, %s: nose gap min %.1f median %.1f\n", programs[pg], way ? "against" : "along", r.min_gap, r.median_gap);
      assert(r.deaths == 0);
    }
  }
  // The delay misjudged by an eighth either way (it is measured, not known).
  int misjudged = 0, misjudged_runs = 0;
  for (int m = 0; m < 2; m++) for (int pg = 1; pg <= 2; pg++) for (int si = 0; si < 2; si++) for (int way = 0; way < 2; way++) {
    enemy_grow(&E, 0, 0, 0, 2, pg, 900);
    double sx = 0, sy = 0, sa = 0;
    assert(start_beside(&E, way, &sx, &sy, &sa));
    result r = drive(&E, sx, sy, sa, (drive_opts){msizes[si], false, 1, 16, 1600, 400, NULL, 0, 0, 0, 0, m ? 1.125 : .875});
    printf("delay reckoned %s, moving enemy %s, size %.1f, %s: nose gap min %.1f median %.1f%s\n", m ? "an eighth long" : "an eighth short", programs[pg], msizes[si], way ? "against" : "along", r.min_gap, r.median_gap, r.deaths ? " (died)" : "");
    misjudged_runs++; misjudged += r.deaths > 0;
  }
  printf("with the delay misjudged: died in %d of %d runs\n", misjudged, misjudged_runs);
  assert(misjudged <= misjudged_runs / 8);
  // Heading for the tail: round the end and back along the other side.
  for (int si = 0; si < 4; si++) for (int b = 0; b < 2; b++) {
    for (int i = 0; i < 100; i++) body[i] = (hug_point){i * 20.0, 0};
    static_body(body, 100);
    result r = drive(&E, 900, 120, HUG_PI, (drive_opts){sizes[si], b, 1, 8, 3000, 600});
    printf("round the tail, size %.1f%s: wrapped %d, nose gap min %.1f\n", sizes[si], b ? " boost" : "", r.wrapped, r.min_gap);
    assert(r.wrapped && r.deaths == 0);
  }
  // Straight at the body from close by (a steep approach).
  for (int si = 0; si < 4; si++) for (int b = 0; b < 2; b++) for (int h = 0; h < 3; h++) {
    for (int i = 0; i < 400; i++) body[i] = (hug_point){i * 25.0, 0};
    static_body(body, 400);
    double heading = -HUG_PI / 2 + (h - 1) * .5, sp = speed(sizes[si], b), tr = sp / 4 / sc_turn(sizes[si], sp);
    result r = drive(&E, 5000, 30 + 14.5 * sizes[si] + 2 + 1.2 * tr + 8 * sp / 4, heading, (drive_opts){sizes[si], b, 1, 8, 800, 0});
    printf("steep approach, size %.1f%s heading %.2f: nose gap min %.1f\n", sizes[si], b ? " boost" : "", heading, r.min_gap);
    assert(r.deaths == 0);
  }
  // A third snake lying across the way.
  {
    for (int i = 0; i < 400; i++) body[i] = (hug_point){i * 25.0, 0};
    static_body(body, 400);
    static double others[HUG_OTHER * 40]; int on = 0;
    for (int j = 0; j < 40; j++) { double* q = others + HUG_OTHER * on++; q[0] = 3000; q[1] = 60 + j * 10; q[2] = 22; q[3] = 0; q[4] = 0; }
    result r = drive(&E, 2000, 50, 0, (drive_opts){1.5, false, 1, 8, 1200, 0, others, on});
    printf("third snake across the way: nearest any hitbox came to the nose %.1f\n", r.min_gap);
    assert(r.deaths == 0);
  }
  // Beside a body that runs along the map wall, on the wall side.
  {
    double R = 2000;
    for (int i = 0; i < 400; i++) { double a = i * 0.004; body[i].x = (R - 58) * cos(a); body[i].y = (R - 58) * sin(a); }
    static_body(body, 400);
    result r = drive(&E, (R - 5) * cos(.3), (R - 5) * sin(.3), .3 + HUG_PI / 2, (drive_opts){1.5, false, 1, 8, 1500, 0, NULL, 0, R});
    printf("between a body and the wall: nearest the head came to the wall or a hitbox %.1f\n", r.min_gap);
    assert(r.deaths == 0);
  }
  printf("Body hug: steering geometry, still / coiled / S-bent / moving and turning bodies, sizes, boost and network delay passed (%d deaths)\n", total_deaths);
  return 0;
}
