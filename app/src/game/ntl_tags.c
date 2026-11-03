#include "ntl_tags.h"
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ntl_tag_def defs[NTL_TAG_MAX];
static int public_count;
static int first_online_custom = 581;   // NTL lists bought tags fetched online below 581, down from 580
ntl_tag_settings ntl_tag_prefs = {.mode = 0, .tag = -1, .show_others = true, .look = {1, 1, 1, 1}, .look_other = {1, 1, 1, 1}, .share = true};

static bool parse_colour(const char* s, uint32_t* out) {
  if (s[0] != '#' || strlen(s) != 7) return false;
  char* end; unsigned long v = strtoul(s + 1, &end, 16);
  if (*end) return false;
  *out = (uint32_t)v; return true;
}
// kind 0: the bundled catalog (starts afresh); 1: NTL's online bought tags (580 down); 2: its
// online public tags (after the bundled ones).
static int bundled_public, revision;
static bool bought[NTL_TAG_MAX];        // online bought tags (NTL draws these over a public tag of the same id)
static int read_catalog(const char* path, int kind) {
  FILE* f = fopen(path, "rb");
  if (!f) return 0;
  if (kind == 0) { memset(defs, 0, sizeof defs); memset(bought, 0, sizeof bought); public_count = 0; first_online_custom = 581; }
  char line[256]; int count = 0, lowest = 581, highest_public = public_count;
  while (fgets(line, sizeof line, f)) {
    if (line[0] == '#') continue;
    int id, w, h, bx, by; char c1[16], c2[16];
    if (sscanf(line, "%d %d %d %15s %15s %d %d", &id, &w, &h, c1, c2, &bx, &by) != 7) continue;
    if (id < 0 || id >= NTL_TAG_MAX || w <= 42 || h <= 42 || w > 2048 || h > 2048) continue;
    // (NTL counts bought tags down from 580 and public ones up from 200; where the two meet, the
    // bought one is what it draws)
    if (kind == 1 && !(id <= 580 && id >= NTL_TAG_PUBLIC_FIRST + bundled_public)) continue;
    if (kind == 2 && (!(id >= NTL_TAG_PUBLIC_FIRST + bundled_public && id < NTL_TAG_PUBLIC_FIRST + 256) || bought[id])) continue;
    ntl_tag_def d = {w, h, bx, by, 0, 0, true};
    if (!parse_colour(c1, &d.c1) || !parse_colour(c2, &d.c2)) continue;
    defs[id] = d; count++;
    if (kind == 1) bought[id] = true;
    if (kind != 1 && id >= NTL_TAG_PUBLIC_FIRST && id < NTL_TAG_PUBLIC_FIRST + 256 && id - NTL_TAG_PUBLIC_FIRST + 1 > highest_public)
      highest_public = id - NTL_TAG_PUBLIC_FIRST + 1;
    if (kind == 1 && id < lowest) lowest = id;
  }
  fclose(f);
  public_count = highest_public;
  if (kind == 0) bundled_public = public_count;
  if (kind == 1 && lowest < first_online_custom) first_online_custom = lowest;
  revision++;
  return count;
}
int ntl_tags_load_catalog(const char* path) { return read_catalog(path, 0); }
int ntl_tags_merge_catalog(const char* path, int kind) { return kind == 1 || kind == 2 ? read_catalog(path, kind) : 0; }
int ntl_tags_bundled_public(void) { return bundled_public; }
int ntl_tags_revision(void) { return revision; }
bool ntl_tag_is_online(int id) {
  return (id >= NTL_TAG_PUBLIC_FIRST + bundled_public && id <= 580);
}
const ntl_tag_def* ntl_tag_get(int id) { return id >= 0 && id < NTL_TAG_MAX && defs[id].known ? &defs[id] : NULL; }
int ntl_tags_public_count(void) { return public_count; }
// (NTL: Rt) 60..199 and the bought tags from the first online one up to 665.
bool ntl_tag_needs_password(int id) {
  return (id >= NTL_TAG_BUILTIN && id < NTL_TAG_PUBLIC_FIRST) || (id >= first_online_custom && id <= NTL_TAG_CUSTOM_LAST);
}
bool ntl_tag_is_free(int id) {
  return (id >= 0 && id < NTL_TAG_BUILTIN) || (id >= NTL_TAG_PUBLIC_FIRST && id < NTL_TAG_PUBLIC_FIRST + public_count && !bought[id]);
}

int ntl_tag_from_skin_header(const uint8_t* h, int length) {
  if (length < 8) return -1;
  if (h[1] == 18) return h[0] == 254 && h[6] < NTL_TAG_BUILTIN ? h[6] : -1;
  if (h[1] == 38) return h[6] < public_count ? NTL_TAG_PUBLIC_FIRST + h[6] : -1;
  return -1;
}
void ntl_tag_write_skin_header(uint8_t h[8], int tag, int skin) {
  h[2] = (uint8_t)skin;
  if (tag >= 0 && tag < NTL_TAG_BUILTIN) { h[0] = 254; h[1] = 18; h[6] = (uint8_t)tag; }
  else if (tag >= NTL_TAG_PUBLIC_FIRST && tag < NTL_TAG_PUBLIC_FIRST + 256) { h[0] = 255; h[1] = 38; h[6] = (uint8_t)(tag - NTL_TAG_PUBLIC_FIRST); }
}
int ntl_tag_for_official_skin(int skin) {
  switch (skin) {
    case 24: return 14; case 25: return 13; case 37: return 1; case 39: return 5; case 42: return 4;
    case 45: return 0; case 46: return 3; case 47: return 7; case 48: return 2; case 49: return 10;
    case 59: return 6; case 62: return 8; case 65: return 39;
    default: return -1;
  }
}

// NTL's rope, as NTL draws it by default: stepped once per drawn frame, its pull, spring, damping
// and speed scaled by the frame's time (6.94 to 16.66 ms: 144 to 60 frames a second), each point
// pulled out along its line from the one before and never further than 4 (x scale x rope length).
// (Stepping it at a fixed 60 a second instead left the tie point, which moves with the head every
// frame, now one step ahead of the rest and now level with it: the first link shook.)
void ntl_tag_rope_update(ntl_tag_rope* r, float hx, float hy, float ang, float scale, double now) {
  ntl_tag_rope_update_len(r, hx, hy, ang, scale, 1, now);
}
#define ROPE_MIN_FRAME 6.94
#define ROPE_MAX_FRAME 16.66
void ntl_tag_rope_update_len(ntl_tag_rope* r, float hx, float hy, float ang, float scale, float length, double now) {
  if (!(scale > 0)) scale = 1;
  if (!(length > 0)) length = 1;
  float ax = hx - 8 * cosf(ang) * scale, ay = hy - 8 * sinf(ang) * scale;   // (the tie point: 8 behind the head)
  float gap = 4 * length * scale;
  if (!r->laid || !(now - r->time < 250) || now < r->time) {
    for (int k = 0; k < NTL_TAG_ROPE; k++) {
      r->x[k] = ax - cosf(ang) * k * gap; r->y[k] = ay - sinf(ang) * k * gap;
      r->vx[k] = r->vy[k] = 0;
    }
    r->turn = r->pturn = ang + 3.14159265f; r->time = now; r->laid = true;
    memcpy(r->px, r->x, sizeof r->x); memcpy(r->py, r->y, sizeof r->y);
    return;
  }
  r->x[0] = ax; r->y[0] = ay;
  double dt = now - r->time;
  r->time = now;
  float mb = (float)(dt > ROPE_MAX_FRAME ? ROPE_MAX_FRAME : dt < ROPE_MIN_FRAME ? ROPE_MIN_FRAME : dt);
  float pull = .2f * mb * length * scale, spring = .005f * mb, damp = .05f * mb, speed = mb / 17;
  memcpy(r->px, r->x, sizeof r->x); memcpy(r->py, r->y, sizeof r->y); r->pturn = r->turn;
  for (int k = 1; k < NTL_TAG_ROPE; k++) {
    float px = r->x[k - 1], py = r->y[k - 1];
    float a = atan2f(r->y[k] - py, r->x[k] - px);
    float tx = px + pull * cosf(a), ty = py + pull * sinf(a);
    r->vx[k] += spring * (tx - r->x[k]); r->vy[k] += spring * (ty - r->y[k]);
    r->x[k] += speed * r->vx[k]; r->y[k] += speed * r->vy[k];
    r->vx[k] *= damp; r->vy[k] *= damp;
    float dx = r->x[k] - px, dy = r->y[k] - py;
    if (dx * dx + dy * dy > gap * gap) {
      a = atan2f(dy, dx);
      r->x[k] = px + gap * cosf(a); r->y[k] = py + gap * sinf(a);
    }
  }
  // the picture turns towards the rope's end direction (NTL: 0.15 of the way each frame)
  int e = NTL_TAG_ROPE - 1;
  float want = atan2f(r->y[e] - r->y[e - 1], r->x[e] - r->x[e - 1]);
  float d = remainderf(want - r->turn, 6.28318531f);
  r->turn = remainderf(r->turn + .15f * d, 6.28318531f);
  if (!isfinite(r->x[e]) || !isfinite(r->turn)) r->laid = false;
}
void ntl_tag_rope_pose(const ntl_tag_rope* r, double now, float* x, float* y, float* turn) {
  (void)now;                                          // (stepped every frame: drawn as it is)
  memcpy(x, r->x, sizeof r->x); memcpy(y, r->y, sizeof r->y);
  *turn = r->turn;
}

// ---- MD5 (RFC 1321)
typedef struct { uint32_t a, b, c, d; } md5_state;
static uint32_t rol(uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }
static void md5_block(md5_state* s, const uint8_t* p) {
  static const uint32_t K[64] = {
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};
  static const int R[64] = {7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
                            4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
  uint32_t m[16];
  for (int i = 0; i < 16; i++) m[i] = (uint32_t)p[4 * i] | (uint32_t)p[4 * i + 1] << 8 | (uint32_t)p[4 * i + 2] << 16 | (uint32_t)p[4 * i + 3] << 24;
  uint32_t a = s->a, b = s->b, c = s->c, d = s->d;
  for (int i = 0; i < 64; i++) {
    uint32_t f; int g;
    if (i < 16) { f = (b & c) | (~b & d); g = i; }
    else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
    else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) % 16; }
    else { f = c ^ (b | ~d); g = (7 * i) % 16; }
    uint32_t t = d; d = c; c = b; b = b + rol(a + f + K[i] + m[g], R[i]); a = t;
  }
  s->a += a; s->b += b; s->c += c; s->d += d;
}
void ntl_tag_md5_hex(const char* text, char out[33]) {
  md5_state s = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
  size_t n = strlen(text), i = 0;
  for (; i + 64 <= n; i += 64) md5_block(&s, (const uint8_t*)text + i);
  uint8_t tail[128] = {0}; size_t rest = n - i;
  memcpy(tail, text + i, rest); tail[rest] = 0x80;
  size_t blocks = rest + 9 <= 64 ? 1 : 2;
  uint64_t bits = (uint64_t)n * 8;
  for (int k = 0; k < 8; k++) tail[blocks * 64 - 8 + k] = (uint8_t)(bits >> (8 * k));
  for (size_t b = 0; b < blocks; b++) md5_block(&s, tail + 64 * b);
  uint32_t w[4] = {s.a, s.b, s.c, s.d};
  for (int k = 0; k < 16; k++) snprintf(out + 2 * k, 3, "%02x", (w[k / 4] >> (8 * (k % 4))) & 255);
}

// ---- settings: {"version":1,"mode":1,"tag":5,"show_others":true,"local_only":false,"auth":[[581,"<md5>"],...]}
static bool hex32(const char* s) {
  for (int i = 0; i < 32; i++) if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return false;
  return true;
}
void ntl_tag_settings_load(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) return;
  char data[8192]; size_t n = fread(data, 1, sizeof data - 1, f); fclose(f); data[n] = 0;
  ntl_tag_settings p = {.mode = 0, .tag = -1, .show_others = true, .look = {1, 1, 1, 1}, .look_other = {1, 1, 1, 1}, .share = true};
  const char* q;
  if ((q = strstr(data, "\"mode\":"))) p.mode = atoi(q + 7) == 1;
  if ((q = strstr(data, "\"tag\":"))) { int t = atoi(q + 6); p.tag = t >= -1 && t < NTL_TAG_MAX ? t : -1; }
  if ((q = strstr(data, "\"show_others\":"))) p.show_others = strncmp(q + 14, "false", 5) != 0;
  if ((q = strstr(data, "\"local_only\":"))) p.local_only = !strncmp(q + 13, "true", 4);
  if ((q = strstr(data, "\"look\":["))) {
    ntl_tag_look l;
    if (sscanf(q + 8, "%f,%f,%f,%f", &l.size, &l.rope, &l.width, &l.alpha) == 4) p.look = ntl_tag_look_clamp(l);
  }
  if ((q = strstr(data, "\"look_other\":["))) {     // (others' own look)
    ntl_tag_look l;
    if (sscanf(q + 14, "%f,%f,%f,%f", &l.size, &l.rope, &l.width, &l.alpha) == 4) p.look_other = ntl_tag_look_clamp(l);
  } else if ((q = strstr(data, "\"look_others\":")) && strncmp(q + 14, "false", 5) != 0)
    p.look_other = p.look;                             // (0.82-0.88: one look, for others too unless turned off)
  if ((q = strstr(data, "\"share\":"))) p.share = strncmp(q + 8, "false", 5) != 0;
  if ((q = strstr(data, "\"share_url\":\""))) {
    q += 13; size_t n = 0;
    while (q[n] && q[n] != '"' && n + 1 < sizeof p.share_url) n++;
    if (q[n] == '"') { memcpy(p.share_url, q, n); p.share_url[n] = 0; }
  }
  if ((q = strstr(data, "\"auth\":["))) {
    q += 8;
    while (p.auth_count < 32 && (q = strchr(q, '['))) {
      int id; char pass[40];
      if (sscanf(q, "[%d,\"%39[0-9a-f]\"]", &id, pass) == 2 && strlen(pass) == 32 && id >= 0 && id < NTL_TAG_MAX) {
        p.auth[p.auth_count].id = id; strcpy(p.auth[p.auth_count].pass_md5, pass); p.auth_count++;
      }
      q++;
    }
  }
  ntl_tag_prefs = p;
}
bool ntl_tag_settings_save(const char* path) {
  FILE* f = fopen(path, "wb");
  if (!f) return false;
  const ntl_tag_settings* p = &ntl_tag_prefs;
  ntl_tag_look l = ntl_tag_look_clamp(p->look), o = ntl_tag_look_clamp(p->look_other);
  char url[sizeof p->share_url]; size_t n = 0;              // (kept to plain URL characters)
  for (const char* c = p->share_url; *c && n + 1 < sizeof url; c++) if ((unsigned char)*c > 32 && *c != '"' && *c != '\\') url[n++] = *c;
  url[n] = 0;
  fprintf(f, "{\"version\":1,\"mode\":%d,\"tag\":%d,\"show_others\":%s,\"local_only\":%s,\"look\":[%.3f,%.3f,%.3f,%.3f],\"look_other\":[%.3f,%.3f,%.3f,%.3f],\"share\":%s,\"share_url\":\"%s\",\"auth\":[",
          p->mode, p->tag, p->show_others ? "true" : "false", p->local_only ? "true" : "false",
          l.size, l.rope, l.width, l.alpha, o.size, o.rope, o.width, o.alpha, p->share ? "true" : "false", url);
  for (int i = 0; i < p->auth_count; i++) fprintf(f, "%s[%d,\"%s\"]", i ? "," : "", p->auth[i].id, p->auth[i].pass_md5);
  fprintf(f, "]}\n");
  return fclose(f) == 0;
}
const char* ntl_tag_auth_for(int id) {
  for (int i = 0; i < ntl_tag_prefs.auth_count; i++)
    if (ntl_tag_prefs.auth[i].id == id && hex32(ntl_tag_prefs.auth[i].pass_md5)) return ntl_tag_prefs.auth[i].pass_md5;
  return NULL;
}
void ntl_tag_auth_set(int id, const char* pass_md5) {
  ntl_tag_settings* p = &ntl_tag_prefs;
  for (int i = 0; i < p->auth_count; i++) if (p->auth[i].id == id) { p->auth[i] = p->auth[--p->auth_count]; break; }
  if (pass_md5 && strlen(pass_md5) == 32 && hex32(pass_md5) && p->auth_count < 32) {
    p->auth[p->auth_count].id = id; memcpy(p->auth[p->auth_count].pass_md5, pass_md5, 33); p->auth_count++;
  }
}
static float within(float v, float lo, float hi, float fallback) { return !isfinite(v) ? fallback : v < lo ? lo : v > hi ? hi : v; }
ntl_tag_look ntl_tag_look_clamp(ntl_tag_look l) {
  return (ntl_tag_look){within(l.size, .25f, 3, 1), within(l.rope, .2f, 3, 1), within(l.width, 0, 3, 1), within(l.alpha, .1f, 1, 1)};
}
ntl_tag_look ntl_tag_look_for(bool own) {
  return ntl_tag_look_clamp(own ? ntl_tag_prefs.look : ntl_tag_prefs.look_other);
}
int ntl_tag_own(void) {
  int t = ntl_tag_prefs.tag;
  if (ntl_tag_prefs.mode != 1 || !ntl_tag_get(t)) return -1;
  if (ntl_tag_is_free(t)) return t;
  return ntl_tag_needs_password(t) && ntl_tag_auth_for(t) ? t : -1;
}
int ntl_tag_own_local(void) {
  int t = ntl_tag_own();
  if (t >= 0) return t;
  t = ntl_tag_prefs.tag;
  return ntl_tag_prefs.mode == 1 && ntl_tag_prefs.local_only && ntl_tag_get(t) && ntl_tag_needs_password(t) ? t : -1;
}

// ---- the tag server's map, written by its thread: two buffers, the reader takes the newest
typedef struct { int count; uint16_t id[4096]; int16_t tag[4096]; } server_map;
static server_map maps[2];
static atomic_int map_current = 0, map_writing = 0;
void ntl_tag_server_map_set(const uint16_t* ids, const int16_t* tags, int count) {
  int next = 1 - atomic_load(&map_current);
  atomic_store(&map_writing, 1);
  if (count > 4096) count = 4096;
  maps[next].count = count > 0 ? count : 0;
  if (count > 0) memcpy(maps[next].id, ids, sizeof(uint16_t) * count);
  if (count > 0) memcpy(maps[next].tag, tags, sizeof(int16_t) * count);
  atomic_store(&map_current, next);
  atomic_store(&map_writing, 0);
}
void ntl_tag_server_map_clear(void) { ntl_tag_server_map_set(NULL, NULL, 0); }
int ntl_tag_server_lookup(int ntl_id) {
  const server_map* m = &maps[atomic_load(&map_current)];
  for (int i = 0; i < m->count; i++) if (m->id[i] == ntl_id) return m->tag[i] >= 0 ? m->tag[i] : -1;
  return -1;
}
