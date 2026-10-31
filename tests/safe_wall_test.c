// Safewall: NTL's scripts 1 and 2 (geometry checks), then closed-loop drives along the wall.
#include "../app/src/game/safe_wall.h"
#include <assert.h>
#include <stdio.h>

static safe_wall_input base(double r, double heading, double mouse_ang, double mouse_len) {
  const double c = 21600;
  safe_wall_input in = {.cx = c, .cy = c, .grd = c, .dynamic = false, .wall = 0,
                        .head_x = c + r, .head_y = c, .view_x = c + r, .view_y = c,
                        .ang = heading, .wang = heading,
                        .mouse_x = mouse_len * cos(mouse_ang), .mouse_y = mouse_len * sin(mouse_ang),
                        .gsc = 0.9, .sp = 5.78, .scang = 1, .spang = 1, .mamu = 0.033, .nsp3 = 12,
                        .boosting = false, .script = 2, .offset = 0, .gap = -1};
  return in;
}

// One drive: the snake runs at the wall from the inside; the mouse stays pointed out of the map
// at `mouse_off` from straight out, the way a player holds it. Commands reach the server
// `ping`/2 after the client decides, from what the server sent `ping`/2 earlier.
typedef struct { double x, y, ang, wang; } state;
static bool tight_drive;
static double drive(double sc, bool boost, double ping, double start_heading, double mouse_off, int script, int offset,
                    bool dynamic, double* closest) {
  const double c = 21600, R = dynamic ? 15000 : 0.98 * c;
  double scang = 0.13 + 0.87 * pow((7 - sc) / 6, 2), sp = boost ? 12 : 5.78, spang = fmin(1, sp / 4.8);
  state hist[4096]; int n = 0;
  state s = {c + R - 700, c, start_heading, start_heading};
  safe_wall_state st = {0};
  double pending_wang[512], pending_at[512]; int pend = 0;
  double max_r = 0; *closest = 1e9;
  int delay_steps = (int)(ping / 2 / 8 + 0.5);
  for (int step = 0; step < 1250; step++) {               // 10 s in 8 ms steps
    double t = step * 8.0;
    for (int k = 0; k < pend;)                               // commands arriving at the server now
      if (pending_at[k] <= t) { s.wang = pending_wang[k]; pending_wang[k] = pending_wang[pend - 1]; pending_at[k] = pending_at[pend - 1]; pend--; }
      else k++;
    double turn = 0.033 * scang * spang, d = fmod(s.wang - s.ang, 2 * SAFE_WALL_PI);
    if (d > SAFE_WALL_PI) d -= 2 * SAFE_WALL_PI;
    if (d < -SAFE_WALL_PI) d += 2 * SAFE_WALL_PI;
    s.ang += fabs(d) < turn ? d : (d > 0 ? turn : -turn);
    s.x += cos(s.ang) * sp / 4; s.y += sin(s.ang) * sp / 4;
    hist[n++ % 4096] = s;
    double r = hypot(s.x - c, s.y - c);
    if (r > max_r) max_r = r;
    if (R - r < *closest) *closest = R - r;
    if (step % 4 == 0 && pend < 512) {                      // the client steers every ~33 ms
      state seen = hist[(n - 1 - (delay_steps < n ? delay_steps : n - 1)) % 4096];
      double out = atan2(seen.y - c, seen.x - c);
      double ma = out + mouse_off;
      safe_wall_input in = {.cx = c, .cy = c, .grd = c, .dynamic = dynamic, .wall = R,
                            .head_x = seen.x, .head_y = seen.y, .view_x = seen.x, .view_y = seen.y,
                            .ang = seen.ang, .wang = seen.wang, .mouse_x = 400 * cos(ma), .mouse_y = 400 * sin(ma),
                            .gsc = 0.9, .sp = sp, .scang = scang, .spang = spang, .mamu = 0.033, .nsp3 = 12,
                            .boosting = boost, .script = script, .offset = offset, .gap = tight_drive ? SAFE_WALL_TIGHT : -1};
      safe_wall_output o;
      double want = safe_wall_steer(&in, &st, &o) ? atan2(o.dy, o.dx) : ma;
      pending_wang[pend] = want; pending_at[pend] = t + ping / 2; pend++;
    }
  }
  return max_r - R;                                         // > 0: the head crossed the wall
}

int main(void) {
  const double c = 21600, R = 0.98 * c;
  safe_wall_state st = {0}; safe_wall_output o;
  // Far from the wall nothing happens; the latch is cleared.
  safe_wall_input in = base(R - 800, 0, 0.6, 400);
  st.latched = true; assert(!safe_wall_steer(&in, &st, &o) && !st.latched);
  // Script 2: heading out, mouse out of the map 35 degrees off straight out.
  for (int off = -10; off <= 10; off += 10) {
    in = base(R - 30, 0.2, 0.6, 400); in.offset = off; st.latched = false;
    assert(safe_wall_steer(&in, &st, &o) && o.steer && st.latched);
    double margin = 3 + (off + 10) / 20.0 * 8;
    double side = cos(0) * sin(in.wang) - sin(0) * cos(in.wang);
    double safe = R - (side < 0 ? margin - 3 : margin);
    assert(fabs(hypot(o.tx - c, o.ty - c) - safe) < 1e-6);          // on the safe circle
    assert(fabs(st.s0 - (side < 0 ? margin - 3 : margin)) < 1e-9);
    double look = hypot(o.tx - in.view_x, o.ty - in.view_y);
    assert(look >= 125 - 1e-6);                                      // at the look-ahead distance
    assert(o.ty > in.head_y);                                        // on the mouse's side (+y)
    assert(fabs(atan2(o.dy, o.dx) - atan2(o.ty - in.head_y, o.tx - in.head_x)) < 1e-12);
  }
  // The mouse's side decides the side of the wall it slides to.
  in = base(R - 30, -0.2, -0.6, 400); st.latched = false;
  assert(safe_wall_steer(&in, &st, &o) && o.ty < in.head_y);
  // Mouse straight out (within 9 degrees) or inside the safe circle: the mouse is used.
  in = base(R - 30, 0.2, 0.1, 400); st.latched = false; assert(!safe_wall_steer(&in, &st, &o));
  in = base(R - 30, 0.2, 2.5, 400); st.latched = false; assert(!safe_wall_steer(&in, &st, &o));
  // The take-over distance grows with how much the snake points out; along the wall it waits.
  in = base(R - 50, 0.0, 0.6, 400); st.latched = false; bool out_take = safe_wall_steer(&in, &st, &o);
  in = base(R - 50, SAFE_WALL_PI / 2, 0.6, 400); st.latched = false; bool along_take = safe_wall_steer(&in, &st, &o);
  assert(out_take && !along_take);
  // Look-ahead: 4 x turning radius x outwardness, at least 125 (150 boosting, with the boost radius).
  in = base(R - 30, 0.2, 0.6, 400); st.latched = false;
  assert(safe_wall_steer(&in, &st, &o) && fabs(hypot(o.tx - in.view_x, o.ty - in.view_y) - 125) < 1e-6);
  in = base(R - 30, 0.2, 0.6, 400); in.boosting = true; in.sp = 12; st.latched = false;
  double boost_radius = 12 / 8.0 / 0.033, expect = fmax(4 * boost_radius * cos(0.2), 150);
  assert(safe_wall_steer(&in, &st, &o) && fabs(hypot(o.tx - in.view_x, o.ty - in.view_y) - expect) < 1e-6);
  in = base(R - 30, 1.2, 0.6, 400); in.boosting = true; in.sp = 12; st.latched = false;   // mostly along: 150
  assert(safe_wall_steer(&in, &st, &o) && fabs(hypot(o.tx - in.view_x, o.ty - in.view_y) - 150) < 1e-6);
  // Script 1: the point 0.78 of the way to the mouse is kept on the wall circle.
  in = base(R - 20, 0.2, 0.6, 400); in.script = 1;
  assert(safe_wall_steer(&in, &st, &o));
  double px = in.head_x + 0.78 * in.mouse_x / in.gsc, py = in.head_y + 0.78 * in.mouse_y / in.gsc;
  assert(fabs(hypot(o.tx - c, o.ty - c) - 0.9795 * c) < 1e-6);
  assert(fabs(atan2(o.ty - c, o.tx - c) - atan2(py - c, px - c)) <= 0.0081);
  in = base(R - 20, 0.2, 2.5, 400); in.script = 1; assert(!safe_wall_steer(&in, &st, &o));
  in = base(R - 60, 0.2, 0.6, 400); in.script = 1; assert(!safe_wall_steer(&in, &st, &o));   // only within 50
  // A server that announces the wall: its radius is used.
  in = base(15000 - 30 - c + c, 0.2, 0.6, 400); in.head_x = in.view_x = c + 15000 - 30; in.dynamic = true; in.wall = 15000;
  st.latched = false; assert(safe_wall_steer(&in, &st, &o) && fabs(hypot(o.tx - c, o.ty - c) - (15000 - st.s0)) < 1e-6);

  // Closed loop: sizes, boost, heading into the wall, mouse held out of the map. These are NTL's
  // scripts as they are, so the drives are a record of how they behave (printed), with one hard
  // requirement: script 2 keeps small and medium snakes (size < 6) inside without lag.
  const double sizes[] = {1, 2.5, 4, 6}, headings[] = {0.0, 0.5, 1.0}, offs[] = {0.35, 0.8, 1.3};
  for (int script = 2; script >= 1; script--) {
    int runs = 0, crossed[2] = {0, 0}, crossed_small = 0; double worst[2] = {-1e9, -1e9}, near_sum = 0;
    for (int dyn = 0; dyn < 2; dyn++)
    for (int si = 0; si < 4; si++) for (int b = 0; b < 2; b++) for (int h = 0; h < 3; h++) for (int m = 0; m < 3; m++)
    for (int side = -1; side <= 1; side += 2) for (int lag = 0; lag < 2; lag++) {
      double closest, over = drive(sizes[si], b, lag ? 100 : 0, side * headings[h], side * offs[m], script, 0, dyn, &closest);
      runs++;
      crossed[lag] += over > 0; if (over > worst[lag]) worst[lag] = over;
      if (!lag) { near_sum += closest; if (over > 0 && sizes[si] < 6) crossed_small++; }
    }
    printf("Safewall script %d: %d drives; no lag %d crossed (worst %.1f), 100 ms %d crossed (worst %.1f), closest to the wall %.1f on average\n",
           script, runs, crossed[0], worst[0], crossed[1], worst[1], near_sum / (runs / 2));
    if (script == 2) assert(crossed_small == 0);
  }
  // Chosen distance: the safe circle is `gap` inside the wall, for both scripts.
  for (double gap = 0; gap <= 20; gap += 2.5) {                                // any chosen distance
    in = base(R - 30, 0.2, 0.6, 400); in.gap = gap; st.latched = false;
    assert(safe_wall_steer(&in, &st, &o) && fabs(hypot(o.tx - c, o.ty - c) - (R - gap)) < 1e-6 && st.s0 == gap);
    in = base(R - 30, -0.2, -0.6, 400); in.gap = gap; st.latched = false;    // the other turn side too
    assert(safe_wall_steer(&in, &st, &o) && st.s0 == gap);
    in = base(R - 20, 0.2, 0.6, 400); in.script = 1; in.gap = gap;
    if (safe_wall_steer(&in, &st, &o)) assert(fabs(hypot(o.tx - c, o.ty - c) - (R - gap)) < 1e-6);
  }
  tight_drive = true;
  for (int script = 2; script >= 1; script--) {
    int runs = 0, crossed[2] = {0, 0}, flush_small = 0, steady_n = 0; double worst[2] = {-1e9, -1e9}, near_sum = 0, near_max = 0, steady_sum = 0;
    for (int dyn = 0; dyn < 2; dyn++)
    for (int si = 0; si < 4; si++) for (int b = 0; b < 2; b++) for (int h = 0; h < 3; h++) for (int m = 0; m < 3; m++)
    for (int side = -1; side <= 1; side += 2) for (int lag = 0; lag < 2; lag++) {
      double closest, over = drive(sizes[si], b, lag ? 100 : 0, side * headings[h], side * offs[m], script, 0, dyn, &closest);
      runs++; crossed[lag] += over > 0; if (over > worst[lag]) worst[lag] = over;
      if (!lag) { near_sum += closest; if (closest > near_max) near_max = closest; if (over > 0 && sizes[si] < 6) flush_small++; }
      if (!lag && sizes[si] < 6 && !b) { steady_sum += closest; steady_n++; }
    }
    printf("  flush script %d, sizes below 6 without boost: closest to the wall %.1f on average; size<6 crossings without lag %d\n", script, steady_sum / steady_n, flush_small);
    if (script == 2) assert(flush_small == 0);
    printf("Safewall flush, script %d: %d drives; no lag %d crossed (worst %.1f), 100 ms %d crossed (worst %.1f), closest %.1f on average\n",
           script, runs, crossed[0], worst[0], crossed[1], worst[1], near_sum / (runs / 2));
  }
  tight_drive = false;
  puts("Safewall (NTL scripts 1 and 2): safe circle, offset margins, sides, straight-out and inside mouse, take-over distance, boost look-ahead, wall circle and announced wall passed");
  return 0;
}
