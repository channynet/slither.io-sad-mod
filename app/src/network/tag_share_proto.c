// The relay's wire format (tag_share.h): hashes and room replies. No threads, no network.
#include "tag_share.h"
#include "../external/mongoose.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void tag_share_hash(const unsigned char* bytes, size_t length, char out[17]) {
  uint64_t h = 0xcbf29ce484222325ull;
  for (size_t i = 0; i < length; i++) { h ^= bytes[i]; h *= 0x100000001b3ull; }
  snprintf(out, 17, "%016llx", (unsigned long long)h);
}
static bool hex16(const char* s) {
  if (!s || strlen(s) != 16) return false;
  for (int i = 0; i < 16; i++) if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return false;
  return true;
}
static float within(double v, float lo, float hi, float fallback) { return !isfinite(v) ? fallback : v < lo ? lo : v > hi ? hi : (float)v; }
int tag_share_parse(const char* json, size_t length, share_entry* out, int cap) {
  struct mg_str all = mg_str_n(json, length);
  int toklen = 0, at = mg_json_get(all, "$.p", &toklen);
  if (at < 0 || toklen < 2 || json[at] != '[') return 0;
  struct mg_str list = mg_str_n(json + at, (size_t)toklen), item;
  size_t offset = 0; int n = 0;
  while (n < cap && (offset = mg_json_next(list, offset, NULL, &item)) != 0) {
    double sid = -1, v;
    if (!mg_json_get_num(item, "$[0]", &sid) || sid < 0 || sid >= 65536) continue;
    char* kind = mg_json_get_str(item, "$[1]");
    share_entry e = {.sid = (int)sid};
    if (kind && !strcmp(kind, "ntl") && mg_json_get_num(item, "$[2]", &v) && v >= 0 && v < 1000) { e.kind = SHARE_NTL; e.ntl = (int)v; }
    else if (kind && !strcmp(kind, "img")) {
      char* hash = mg_json_get_str(item, "$[2]");
      if (hex16(hash)) {
        e.kind = SHARE_IMAGE; memcpy(e.hash, hash, 17);
        e.size = mg_json_get_num(item, "$[3]", &v) ? within(v, 24, 320, 128) : 128;
        e.gap = mg_json_get_num(item, "$[4]", &v) ? within(v, 0, 400, 24) : 24;
        e.angle = mg_json_get_num(item, "$[5]", &v) ? within(v, -180, 180, 0) : 0;
        e.sway = mg_json_get_num(item, "$[6]", &v) && v != 0;
      }
      free(hash);
    }
    free(kind);
    if (e.kind != SHARE_NONE) out[n++] = e;
  }
  return n;
}
