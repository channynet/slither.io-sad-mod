// NTL tags: catalog, skin header bytes (as NTL writes and reads them), MD5 of passwords, saved
// settings, and the rope staying sane while the head turns.
#include "../app/src/game/ntl_tags.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
  const char* catalog = argc > 1 ? argv[1] : "app/res/ntl-tags/tags.txt";
  int n = ntl_tags_load_catalog(catalog);
  assert(n > 300);
  // NTL's own first tag and a public one, with their rope colours and offsets
  const ntl_tag_def* d = ntl_tag_get(5);
  assert(d && d->w == 190 && d->h == 188 && d->c1 == 0x1d3245 && d->c2 == 0x44d4ff && d->bx == -32 && d->by == -70);
  assert(ntl_tag_get(200) && ntl_tags_public_count() >= 104 && !ntl_tag_get(-1) && !ntl_tag_get(NTL_TAG_MAX));
  // free and bought
  assert(ntl_tag_is_free(0) && ntl_tag_is_free(59) && ntl_tag_is_free(200) && !ntl_tag_is_free(60) && !ntl_tag_is_free(581));
  assert(ntl_tag_needs_password(60) && ntl_tag_needs_password(199) && ntl_tag_needs_password(581) && ntl_tag_needs_password(665));
  assert(!ntl_tag_needs_password(0) && !ntl_tag_needs_password(200) && !ntl_tag_needs_password(666));
  // skin header: written as NTL writes it, read back
  uint8_t h[8] = {255, 255, 255, 0, 0, 0, 7, 9};
  ntl_tag_write_skin_header(h, 14, 255);
  assert(h[0] == 254 && h[1] == 18 && h[2] == 255 && h[6] == 14 && ntl_tag_from_skin_header(h, 8) == 14);
  ntl_tag_write_skin_header(h, 203, 9);
  assert(h[0] == 255 && h[1] == 38 && h[2] == 9 && h[6] == 3 && ntl_tag_from_skin_header(h, 8) == 203);
  uint8_t plain[8] = {255, 255, 255, 0, 0, 0, 14, 0}, bad[8] = {200, 18, 255, 0, 0, 0, 14, 0};
  assert(ntl_tag_from_skin_header(plain, 8) == -1 && ntl_tag_from_skin_header(bad, 8) == -1 && ntl_tag_from_skin_header(h, 7) == -1);
  assert(ntl_tag_for_official_skin(24) == 14 && ntl_tag_for_official_skin(65) == 39 && ntl_tag_for_official_skin(0) == -1);
  // MD5 (RFC 1321 vectors, and one longer than a block)
  char m[33];
  ntl_tag_md5_hex("", m); assert(!strcmp(m, "d41d8cd98f00b204e9800998ecf8427e"));
  ntl_tag_md5_hex("abc", m); assert(!strcmp(m, "900150983cd24fb0d6963f7d28e17f72"));
  ntl_tag_md5_hex("12345678901234567890123456789012345678901234567890123456789012345678901234567890", m);
  assert(!strcmp(m, "57edf4a22be3c955ac49da2e2107b67a"));
  // settings: saved and read back; a bought tag counts only once its password was accepted
  ntl_tag_prefs = (ntl_tag_settings){.mode = 1, .tag = 581, .show_others = false};
  assert(ntl_tag_own() == -1);
  ntl_tag_md5_hex("secret", m);
  ntl_tag_auth_set(581, m);
  assert(ntl_tag_own() == 581 && ntl_tag_auth_for(581));
  const char* path = "ntl-tags-test.json";
  assert(ntl_tag_settings_save(path));
  ntl_tag_prefs = (ntl_tag_settings){0};
  ntl_tag_settings_load(path);
  assert(ntl_tag_prefs.mode == 1 && ntl_tag_prefs.tag == 581 && !ntl_tag_prefs.show_others && ntl_tag_prefs.auth_count == 1);
  assert(!strcmp(ntl_tag_auth_for(581), m));
  ntl_tag_auth_set(581, NULL);
  assert(ntl_tag_own() == -1);
  ntl_tag_prefs.tag = 3; assert(ntl_tag_own() == 3);
  ntl_tag_prefs.mode = 0; assert(ntl_tag_own() == -1);
  // a bought tag without its password, after the warning: drawn on my own screen only, never sent
  ntl_tag_prefs = (ntl_tag_settings){.mode = 1, .tag = 590, .show_others = true};
  assert(ntl_tag_own() == -1 && ntl_tag_own_local() == -1);
  ntl_tag_prefs.local_only = true;
  assert(ntl_tag_own() == -1 && ntl_tag_own_local() == 590);
  assert(ntl_tag_settings_save(path));
  ntl_tag_prefs = (ntl_tag_settings){0};
  ntl_tag_settings_load(path);
  assert(ntl_tag_prefs.local_only && ntl_tag_prefs.tag == 590 && ntl_tag_own() == -1 && ntl_tag_own_local() == 590);
  {                                                    // (a file from before the look: tags as they come)
    FILE* old = fopen("ntl-tags-old.json", "wb");
    fprintf(old, "{\"version\":1,\"mode\":1,\"tag\":3,\"show_others\":true,\"local_only\":false,\"auth\":[]}\n"); fclose(old);
    ntl_tag_settings keep = ntl_tag_prefs;
    ntl_tag_settings_load("ntl-tags-old.json"); remove("ntl-tags-old.json");
    assert(ntl_tag_prefs.tag == 3 && ntl_tag_prefs.look.size == 1 && ntl_tag_prefs.look.rope == 1 && ntl_tag_prefs.look.width == 1 && ntl_tag_prefs.look_other.size == 1);
    ntl_tag_prefs = keep;
  }
  {                                                    // (0.82-0.88: one look, for others too unless turned off)
    FILE* old = fopen("ntl-tags-old.json", "wb");
    fprintf(old, "{\"version\":1,\"mode\":1,\"tag\":3,\"look\":[1.500,1.000,1.000,1.000],\"look_others\":true,\"auth\":[]}\n"); fclose(old);
    ntl_tag_settings keep = ntl_tag_prefs;
    ntl_tag_settings_load("ntl-tags-old.json");
    assert(fabsf(ntl_tag_prefs.look.size - 1.5f) < 1e-3f && fabsf(ntl_tag_prefs.look_other.size - 1.5f) < 1e-3f);
    old = fopen("ntl-tags-old.json", "wb");
    fprintf(old, "{\"version\":1,\"mode\":1,\"tag\":3,\"look\":[1.500,1.000,1.000,1.000],\"look_others\":false,\"auth\":[]}\n"); fclose(old);
    ntl_tag_settings_load("ntl-tags-old.json"); remove("ntl-tags-old.json");
    assert(fabsf(ntl_tag_prefs.look.size - 1.5f) < 1e-3f && ntl_tag_prefs.look_other.size == 1);
    ntl_tag_prefs = keep;
  }
  // my look and other players' look: each saved, read back, kept in range
  ntl_tag_prefs.look = (ntl_tag_look){1.5f, .5f, 0, .8f}; ntl_tag_prefs.look_other = (ntl_tag_look){.6f, 2, 1.2f, 1};
  assert(ntl_tag_settings_save(path));
  ntl_tag_prefs = (ntl_tag_settings){0};
  ntl_tag_settings_load(path);
  ntl_tag_look own = ntl_tag_look_for(true), other = ntl_tag_look_for(false);
  assert(fabsf(own.size - 1.5f) < 1e-3f && fabsf(own.rope - .5f) < 1e-3f && own.width == 0 && fabsf(own.alpha - .8f) < 1e-3f);
  assert(fabsf(other.size - .6f) < 1e-3f && fabsf(other.rope - 2) < 1e-3f && fabsf(other.width - 1.2f) < 1e-3f && other.alpha == 1);
  ntl_tag_look wild = ntl_tag_look_clamp((ntl_tag_look){99, -1, NAN, 0});
  assert(wild.size == 3 && fabsf(wild.rope - .2f) < 1e-6f && wild.width == 1 && fabsf(wild.alpha - .1f) < 1e-6f);
  ntl_tag_prefs.tag = 7; assert(ntl_tag_own() == 7 && ntl_tag_own_local() == 7);       // (a free tag: everyone sees it anyway)
  ntl_tag_prefs.tag = 590; ntl_tag_prefs.mode = 0; assert(ntl_tag_own_local() == -1);  // my own image tag instead
  remove(path);
  // server map: newest list wins, unlisted ids have none
  uint16_t ids[2] = {1025, 7}; int16_t tags[2] = {590, -1};
  ntl_tag_server_map_set(ids, tags, 2);
  assert(ntl_tag_server_lookup(1025) == 590 && ntl_tag_server_lookup(7) == -1 && ntl_tag_server_lookup(8) == -1);
  ntl_tag_server_map_clear();
  assert(ntl_tag_server_lookup(1025) == -1);
  // the rope: trails behind the head, links no longer than 4 (x scale), finite while turning hard
  ntl_tag_rope r = {0};
  float x = 1000, y = 1000, a = 0, sc = 2;
  double t = 0;
  for (int i = 0; i < 2000; i++) {
    t += i % 7 == 0 ? 33 : 8;                       // uneven frames
    a += .03f; x += cosf(a) * 3; y += sinf(a) * 3;
    ntl_tag_rope_update(&r, x, y, a, sc, t);
    for (int k = 1; k < NTL_TAG_ROPE; k++) {
      float dx = r.x[k] - r.x[k - 1], dy = r.y[k] - r.y[k - 1];
      // (the tie point moves with the head between 60 Hz steps: the first link may stretch a little)
      assert(isfinite(dx) && sqrtf(dx * dx + dy * dy) <= 4 * sc + (k == 1 ? 12 : 1e-3f));
    }
  }
  float tie = hypotf(r.x[0] - (x - 8 * cosf(a) * sc), r.y[0] - (y - 8 * sinf(a) * sc));
  assert(tie < 1e-3f && isfinite(r.turn));
  // going straight, the rope lies behind the head
  for (int i = 0; i < 300; i++) { t += 16; x += 3; ntl_tag_rope_update(&r, x, y, 0, 1, t); }
  assert(r.x[NTL_TAG_ROPE - 1] < r.x[0] - 20 && fabsf(r.y[NTL_TAG_ROPE - 1] - y) < 2);
  // drawn between steps: at 144 frames a second the rope's end glides (no step-sized jumps)
  {
    ntl_tag_rope g = {0}; float gx = 0, ga = 0, last_x = 0, last_y = 0, worst = 0; double gt = 0;
    for (int i = 0; i < 2000; i++) {
      gt += 1000.0 / 144; ga += .004f; gx += .9f * cosf(ga);
      float gy = 400 + 0;
      ntl_tag_rope_update(&g, gx, gy + sinf(ga) * 50, ga, 1, gt);
      float px[NTL_TAG_ROPE], py[NTL_TAG_ROPE], turn;
      ntl_tag_rope_pose(&g, gt, px, py, &turn);
      assert(isfinite(turn) && px[0] == g.x[0]);
      float ex = px[NTL_TAG_ROPE - 1], ey = py[NTL_TAG_ROPE - 1];
      if (i > 200) { float j = hypotf(ex - last_x, ey - last_y); if (j > worst) worst = j; }
      last_x = ex; last_y = ey;
    }
    assert(worst < 2.5f);                          // (the head moves about 0.9-1.3 a frame)
  }
  // uneven 60 Hz frames (16-17.4 ms): the first link keeps its length (stepped every frame, the
  // tie point and the rest move together; stepped at a fixed 60 a second it shook by about 1)
  {
    ntl_tag_rope g = {0}; float gx = 0, ga = 0; double gt = 0, sum = 0, sum2 = 0; int n = 0;
    for (int i = 0; i < 1500; i++) {
      double dt = 16.0 + 1.4 * ((i * 7919) % 100) / 100.0; gt += dt;
      ga += .0006f * (float)dt; gx += .185f * (float)dt * cosf(ga);
      float gy = .185f * (float)dt * sinf(ga);
      static float yy = 0; yy += gy;
      ntl_tag_rope_update(&g, gx, yy, ga, 2, gt);
      if (i > 300) { float L = hypotf(g.x[1] - g.x[0], g.y[1] - g.y[0]); sum += L; sum2 += L * L; n++; }
    }
    double mean = sum / n, sd = sqrt(fmax(0, sum2 / n - mean * mean));
    assert(sd < .05);
  }
  // a longer rope (the shared look): its end lies twice as far back, the tie point stays put
  {
    ntl_tag_rope a = {0}, b = {0}; float hx = 0; double tt = 0;
    for (int i = 0; i < 400; i++) { tt += 16; hx += 3; ntl_tag_rope_update(&a, hx, 0, 0, 1, tt); ntl_tag_rope_update_len(&b, hx, 0, 0, 1, 2, tt); }
    float la = a.x[0] - a.x[NTL_TAG_ROPE - 1], lb = b.x[0] - b.x[NTL_TAG_ROPE - 1];
    assert(a.x[0] == b.x[0] && la > 20 && fabsf(lb / la - 2) < .1f);
  }
  // a long pause: laid out afresh behind the head
  t += 5000; ntl_tag_rope_update(&r, x, y, 0, 1, t);
  assert(fabsf(r.x[1] - (r.x[0] - 4)) < 1e-3f);
  // NTL's online lists: public tags up from the bundled ones, bought ones down from 580; where
  // they meet, the bought one wins (as NTL draws it)
  {
    int first = NTL_TAG_PUBLIC_FIRST + ntl_tags_bundled_public();
    FILE* fs = fopen("ntl-fs-test.txt", "wb"); FILE* cs = fopen("ntl-cs-test.txt", "wb");
    fprintf(fs, "# fs\n%d 140 107 #ffffff #ff0000 -20 -53\n%d 200 200 #000000 #ffffff -20 -100\n", first, first + 50);
    fprintf(cs, "# cs\n580 280 200 #00ffff #add8e6 -25 -95\n%d 217 147 #008750 #fefefe -26 -74\n", first + 50);
    fclose(fs); fclose(cs);
    assert(ntl_tags_merge_catalog("ntl-fs-test.txt", 2) == 2 && ntl_tags_merge_catalog("ntl-cs-test.txt", 1) == 2);
    assert(ntl_tag_get(first + 50)->w == 217 && !ntl_tag_is_free(first + 50) && ntl_tag_needs_password(first + 50));
    assert(ntl_tag_is_free(first) && !ntl_tag_needs_password(first) && ntl_tag_is_online(first) && ntl_tag_get(580)->w == 280);
    assert(ntl_tags_merge_catalog("ntl-fs-test.txt", 2) == 1 && ntl_tag_get(first + 50)->w == 217);   // (merged again: still bought)
    remove("ntl-fs-test.txt"); remove("ntl-cs-test.txt");
  }
  printf("NTL tags: catalog (%d), skin header, MD5, settings, server map and rope passed\n", n);
  return 0;
}
