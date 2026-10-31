#include "ntl_tags_online.h"
#include "../game/ntl_tags.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void merge_one(const char* kind, int k) {
  char path[96]; snprintf(path, sizeof path, NTL_TAGS_ONLINE_DIR "/%s.txt", kind);
  FILE* f = fopen(path, "rb");
  if (f) fclose(f); else snprintf(path, sizeof path, NTL_TAGS_ONLINE_BUNDLED "/%s.txt", kind);
  ntl_tags_merge_catalog(path, k);
}
static void merge_kept(void) {
  merge_one("fstags", 2);   // (public first: a bought tag with the same id wins, as in NTL)
  merge_one("cstags", 1);
}
void ntl_tags_online_merge(void) { merge_kept(); }

#ifdef _WIN32
#define COBJMACROS
#include "../external/mongoose.h"      // (brings winsock2.h before windows.h)
#include <windows.h>
#include <winhttp.h>
#include <wincodec.h>
#include <external/stb/stb_image_write.h>

#ifndef NTL_TAGS_ONLINE_HOST                     // (a test can point it elsewhere)
#define NTL_TAGS_ONLINE_HOST L"ntl-slither.com"
#define NTL_TAGS_ONLINE_PORT 443
#define NTL_TAGS_ONLINE_FLAGS WINHTTP_FLAG_SECURE
#endif
static volatile LONG fresh, started, failed_pictures;
static char status[256] = "아직 받지 않음";

// The colour names NTL's lists use (CSS), as #rrggbb.
static const struct { const char* name; const char* hex; } css[] = {
  {"black","#000000"},{"white","#ffffff"},{"red","#ff0000"},{"lime","#00ff00"},{"green","#008000"},{"blue","#0000ff"},
  {"yellow","#ffff00"},{"cyan","#00ffff"},{"aqua","#00ffff"},{"magenta","#ff00ff"},{"fuchsia","#ff00ff"},{"silver","#c0c0c0"},
  {"gray","#808080"},{"grey","#808080"},{"maroon","#800000"},{"olive","#808000"},{"purple","#800080"},{"teal","#008080"},
  {"navy","#000080"},{"orange","#ffa500"},{"gold","#ffd700"},{"pink","#ffc0cb"},{"hotpink","#ff69b4"},{"deeppink","#ff1493"},
  {"brown","#a52a2a"},{"chocolate","#d2691e"},{"tan","#d2b48c"},{"beige","#f5f5dc"},{"violet","#ee82ee"},{"indigo","#4b0082"},
  {"orchid","#da70d6"},{"plum","#dda0dd"},{"crimson","#dc143c"},{"coral","#ff7f50"},{"tomato","#ff6347"},{"salmon","#fa8072"},
  {"khaki","#f0e68c"},{"lavender","#e6e6fa"},{"turquoise","#40e0d0"},{"skyblue","#87ceeb"},{"lightblue","#add8e6"},
  {"darkblue","#00008b"},{"darkred","#8b0000"},{"darkgreen","#006400"},{"lightgreen","#90ee90"},{"darkgray","#a9a9a9"},
  {"darkgrey","#a9a9a9"},{"lightgray","#d3d3d3"},{"lightgrey","#d3d3d3"},{"dimgray","#696969"},{"dimgrey","#696969"},
  {"orangered","#ff4500"},{"darkorange","#ff8c00"},{"limegreen","#32cd32"},{"royalblue","#4169e1"},{"steelblue","#4682b4"},
  {"dodgerblue","#1e90ff"},{"deepskyblue","#00bfff"},{"midnightblue","#191970"},{"slategray","#708090"},{"firebrick","#b22222"},
  {"goldenrod","#daa520"},{"sienna","#a0522d"},{"peru","#cd853f"},{"wheat","#f5deb3"},{"ivory","#fffff0"},{"snow","#fffafa"},
  {"darkviolet","#9400d3"},{"darkmagenta","#8b008b"},{"darkcyan","#008b8b"},{"seagreen","#2e8b57"},{"forestgreen","#228b22"},
};
static void colour(const char* in, char out[8]) {
  if (in && in[0] == '#') {
    unsigned v;
    if (strlen(in) == 7 && sscanf(in + 1, "%6x", &v) == 1) { snprintf(out, 8, "#%06x", v); return; }
    if (strlen(in) == 4 && sscanf(in + 1, "%3x", &v) == 1) {
      snprintf(out, 8, "#%02x%02x%02x", ((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17); return;
    }
  }
  if (in) for (size_t i = 0; i < sizeof css / sizeof *css; i++) if (!_stricmp(in, css[i].name)) { snprintf(out, 8, "%s", css[i].hex); return; }
  snprintf(out, 8, "#808080");
}

// GET https://ntl-slither.com<path> into a malloc'd, 0-terminated buffer (NULL on failure).
static char* fetch(const wchar_t* path, size_t* length) {
  char* body = NULL; size_t total = 0, cap = 0;
  HINTERNET session = WinHttpOpen(L"SlitherNative/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  HINTERNET connection = session ? WinHttpConnect(session, NTL_TAGS_ONLINE_HOST, NTL_TAGS_ONLINE_PORT, 0) : NULL;
  HINTERNET request = connection ? WinHttpOpenRequest(connection, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, NTL_TAGS_ONLINE_FLAGS) : NULL;
  if (session) WinHttpSetTimeouts(session, 8000, 8000, 15000, 30000);
  bool ok = false;
  if (request && WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0) && WinHttpReceiveResponse(request, NULL)) {
    DWORD code = 0, size = sizeof code;
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &code, &size, NULL);
    ok = code == 200;
    for (;;) {
      if (total + 65536 + 1 > cap) {
        if (cap >= 64u << 20) { ok = false; break; }          // (64 MB at most)
        cap = cap ? cap * 2 : 1 << 20;
        char* bigger = realloc(body, cap);
        if (!bigger) { ok = false; break; }
        body = bigger;
      }
      DWORD got = 0;
      if (!WinHttpReadData(request, body + total, 65536, &got)) { ok = false; break; }
      if (!got) break;
      total += got;
    }
  }
  if (request) WinHttpCloseHandle(request);
  if (connection) WinHttpCloseHandle(connection);
  if (session) WinHttpCloseHandle(session);
  if (!ok || !body) { free(body); return NULL; }
  body[total] = 0; *length = total;
  return body;
}

// A webp picture to RGBA through Windows' own decoder (WIC). malloc'd; NULL when it cannot.
static unsigned char* decode_webp(const unsigned char* data, size_t n, int* w, int* h) {
  IWICImagingFactory* factory = NULL; IWICStream* stream = NULL; IWICBitmapDecoder* decoder = NULL;
  IWICBitmapFrameDecode* frame = NULL; IWICFormatConverter* converter = NULL; unsigned char* out = NULL;
  UINT width = 0, height = 0;
  if (FAILED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void**)&factory))) goto done;
  if (FAILED(IWICImagingFactory_CreateStream(factory, &stream)) || FAILED(IWICStream_InitializeFromMemory(stream, (BYTE*)data, (DWORD)n))) goto done;
  if (FAILED(IWICImagingFactory_CreateDecoderFromStream(factory, (IStream*)stream, NULL, WICDecodeMetadataCacheOnDemand, &decoder))) goto done;
  if (FAILED(IWICBitmapDecoder_GetFrame(decoder, 0, &frame))) goto done;
  if (FAILED(IWICImagingFactory_CreateFormatConverter(factory, &converter))) goto done;
  if (FAILED(IWICFormatConverter_Initialize(converter, (IWICBitmapSource*)frame, &GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, NULL, 0, WICBitmapPaletteTypeCustom))) goto done;
  if (FAILED(IWICFormatConverter_GetSize(converter, &width, &height)) || !width || !height || width > 2048 || height > 2048) goto done;
  out = malloc((size_t)width * height * 4);
  if (out && FAILED(IWICFormatConverter_CopyPixels(converter, NULL, width * 4, width * height * 4, out))) { free(out); out = NULL; }
  *w = (int)width; *h = (int)height;
done:
  if (converter) IWICFormatConverter_Release(converter);
  if (frame) IWICBitmapFrameDecode_Release(frame);
  if (decoder) IWICBitmapDecoder_Release(decoder);
  if (stream) IWICStream_Release(stream);
  if (factory) IWICImagingFactory_Release(factory);
  return out;
}
// A data: URI picture written as t<id>.png (a PNG as it is; webp turned into PNG).
static bool save_picture(int id, const char* uri) {
  const char* comma = uri ? strchr(uri, ',') : NULL;
  if (!comma || strncmp(uri, "data:image/", 11) || !strstr(uri, ";base64")) return false;
  size_t n = strlen(comma + 1);
  char* raw = malloc(n * 3 / 4 + 4);
  if (!raw) return false;
  size_t len = mg_base64_decode(comma + 1, n, raw, n * 3 / 4 + 4);
  char path[96]; snprintf(path, sizeof path, NTL_TAGS_ONLINE_DIR "/t%d.png", id);
  bool ok = false;
  if (len > 8 && !memcmp(raw, "\x89PNG\r\n\x1a\n", 8)) {
    FILE* f = fopen(path, "wb");
    if (f) { ok = fwrite(raw, 1, len, f) == len; if (fclose(f)) ok = false; }
  } else if (len > 12 && !memcmp(raw, "RIFF", 4) && !memcmp(raw + 8, "WEBP", 4)) {
    int w = 0, h = 0;
    unsigned char* rgba = decode_webp((const unsigned char*)raw, len, &w, &h);
    if (rgba) ok = stbi_write_png(path, w, h, 4, rgba, w * 4) != 0;
    free(rgba);
  }
  free(raw);
  return ok;
}
static int field_int(struct mg_str o, const char* path, int fallback) {
  double v;
  if (mg_json_get_num(o, path, &v)) return (int)v;
  char* s = mg_json_get_str(o, path);
  int r = s && *s ? atoi(s) : fallback;
  free(s);
  return r;
}
// One list: fetched (asking for what is newer than the kept version), pictures and catalog kept.
// Returns how many tags it kept, -1 when nothing new, -2 on failure.
static int fetch_list(const char* kind, int first_id, int step, bool bought) {
  char ver_path[96], cat_path[96], tmp_path[100];
  snprintf(ver_path, sizeof ver_path, NTL_TAGS_ONLINE_DIR "/%s.ver", kind);
  snprintf(cat_path, sizeof cat_path, NTL_TAGS_ONLINE_DIR "/%s.txt", kind);
  snprintf(tmp_path, sizeof tmp_path, "%s.tmp", cat_path);
  int version = 0;
  FILE* f = fopen(ver_path, "rb");
  if (f) { if (fscanf(f, "%d", &version) != 1) version = 0; fclose(f); }
  if (!(f = fopen(cat_path, "rb"))) version = 0; else fclose(f);   // (no catalog kept: ask for all)
  wchar_t path[96]; swprintf(path, 96, L"/tags/%hs?v=%d", kind, version);
  size_t length = 0;
  char* body = fetch(path, &length);
  if (!body) return -2;
  struct mg_str json = mg_str_n(body, length);
  double v = 0; int toklen = 0, at = mg_json_get(json, "$.d", &toklen);
  if (!mg_json_get_num(json, "$.v", &v) || at < 0 || toklen < 2 || body[at] != '[') { free(body); return -1; }
  struct mg_str list = mg_str_n(body + at, toklen), item;
  FILE* cat = fopen(tmp_path, "wb");
  if (!cat) { free(body); return -2; }
  fprintf(cat, "# NTL %s (online, version %d): id width height colour1 colour2 x y\n", kind, (int)v);
  size_t offset = 0; int k = 0, kept = 0;
  while ((offset = mg_json_next(list, offset, NULL, &item)) != 0) {
    int id = first_id + step * k++;
    if (id < NTL_TAG_PUBLIC_FIRST || id > 580) continue;
    const char* p = bought ? "$.w" : "$.fst_blbw";
    int w = field_int(item, p, 0), h = field_int(item, bought ? "$.h" : "$.fst_blbh", 0);
    int bx = field_int(item, bought ? "$.bx" : "$.fst_blbx", -20), by = field_int(item, bought ? "$.by" : "$.fst_blby", -h / 2);
    char* c1 = mg_json_get_str(item, bought ? "$.c1" : "$.fst_atc1");
    char* c2 = mg_json_get_str(item, bought ? "$.c2" : "$.fst_atc2");
    char* src = mg_json_get_str(item, bought ? "$.src" : "$.fst_src");
    char h1[8], h2[8]; colour(c1, h1); colour(c2, h2);
    if (w > 42 && h > 42 && save_picture(id, src)) { fprintf(cat, "%d %d %d %s %s %d %d\n", id, w, h, h1, h2, bx, by); kept++; }
    else InterlockedIncrement(&failed_pictures);
    free(c1); free(c2); free(src);
  }
  bool ok = fclose(cat) == 0;
  free(body);
  if (!ok || !MoveFileExA(tmp_path, cat_path, MOVEFILE_REPLACE_EXISTING)) return -2;
  if ((f = fopen(ver_path, "wb"))) { fprintf(f, "%d\n", (int)v); fclose(f); }
  return kept;
}
static DWORD WINAPI worker(void* unused) {
  (void)unused;
  CoInitializeEx(NULL, COINIT_MULTITHREADED);
  CreateDirectoryA(NTL_TAGS_ONLINE_DIR, NULL);
  int pub = fetch_list("fstags", NTL_TAG_PUBLIC_FIRST + ntl_tags_bundled_public(), 1, false);
  int bought = fetch_list("cstags", 580, -1, true);   // (after: its pictures win where the ids meet)
  if (bought == -2 && pub == -2) snprintf(status, sizeof status, "NTL에 연결하지 못함 (지난번에 받은 것 사용)");
  else {
    char a[48], b[48];
    snprintf(a, sizeof a, bought >= 0 ? "비밀번호 태그 %d개 받음" : bought == -1 ? "비밀번호 태그 최신" : "비밀번호 태그 실패", bought);
    snprintf(b, sizeof b, pub >= 0 ? "공개 태그 %d개 받음" : pub == -1 ? "공개 태그 최신" : "공개 태그 실패", pub);
    snprintf(status, sizeof status, "%s · %s", a, b);
    if (failed_pictures) {
      size_t n = strlen(status);
      snprintf(status + n, sizeof status - n, " · 그림 %ld개 못 읽음 (Windows WebP 코덱 없음?)", (long)failed_pictures);
    }
  }
  FILE* note = fopen(NTL_TAGS_ONLINE_DIR "/status.txt", "wb");   // (what happened, for a look later)
  if (note) { fprintf(note, "%s\n", status); fclose(note); }
  if (bought >= 0 || pub >= 0) InterlockedExchange(&fresh, 1);
  CoUninitialize();
  return 0;
}
void ntl_tags_online_start(void) {
  merge_kept();
  if (InterlockedCompareExchange(&started, 1, 0) != 0) return;
  snprintf(status, sizeof status, "NTL에서 받는 중");
  HANDLE t = CreateThread(NULL, 0, worker, NULL, 0, NULL);
  if (t) CloseHandle(t);
}
bool ntl_tags_online_take(void) { return InterlockedCompareExchange(&fresh, 0, 1) == 1; }
const char* ntl_tags_online_status(void) { return status; }
#else
void ntl_tags_online_start(void) { merge_kept(); }
bool ntl_tags_online_take(void) { return false; }
const char* ntl_tags_online_status(void) { return "macOS에서는 받지 않음"; }
#endif
