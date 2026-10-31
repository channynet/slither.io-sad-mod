#ifndef NTL_TAGS_H
#define NTL_TAGS_H
// NTL tags: the pictures NTL mod users hang on a short rope behind their head.
//
// Where a snake's tag comes from (as NTL does it):
//  - free tags ride in the custom skin code every client gets from the game server: the 8 header
//    bytes say 254,18,..,..,..,..,tag for NTL's own tags 0..59 and ?,38,..,..,..,..,n for its public
//    tags 200+n;
//  - tags bought from NTL (a tag id and its password) go through NTL's tag server, which tells every
//    NTL client which game-server snake (its NTL id) wears which tag;
//  - teammates' tags come with the NTL team list;
//  - some official skins get a tag of their own when none is chosen.
// Pictures and their rope colours/placement are in app/res/ntl-tags (tags.txt and t<id>.png).
#include <stdbool.h>
#include <stdint.h>

#define NTL_TAG_MAX 700
#define NTL_TAG_BUILTIN 60          // NTL's own free tags: 0..59
#define NTL_TAG_PUBLIC_FIRST 200    // public free tags: 200..
#define NTL_TAG_CUSTOM_LAST 665     // bought tags: 60..199 and (first online one)..665
#define NTL_TAG_ROPE 10             // rope points (the first one is tied behind the head)

typedef struct {
  int w, h, bx, by;                 // picture box (with a 21 px margin all round) and its offset at the rope's end
  uint32_t c1, c2;                  // rope colours, 0xRRGGBB
  bool known;
} ntl_tag_def;

// The catalog (tags.txt). Returns how many tags were read.
int ntl_tags_load_catalog(const char* path);
// NTL's online lists (kept in ntl-tags-online/): kind 1 its bought tags (580 down), kind 2 its
// public tags after the bundled ones. Added to the catalog; returns how many.
int ntl_tags_merge_catalog(const char* path, int kind);
int ntl_tags_bundled_public(void);
int ntl_tags_revision(void);            // changes whenever the catalog does
bool ntl_tag_is_online(int id);         // its picture is one fetched from NTL (ntl-tags-online/)
const ntl_tag_def* ntl_tag_get(int id);
int ntl_tags_public_count(void);
// Bought tags need their password (checked by NTL's server) before they can be worn.
bool ntl_tag_needs_password(int id);
bool ntl_tag_is_free(int id);

// The skin code's 8 header bytes: the free tag they carry, or -1.
int ntl_tag_from_skin_header(const uint8_t* header, int length);
// Writes a free tag into the header (skin: the official skin number, or 255 for a custom skin).
void ntl_tag_write_skin_header(uint8_t header[8], int tag, int skin);
// The tag NTL shows on an official skin when none is chosen (-1: none).
int ntl_tag_for_official_skin(int skin);

// The rope: point 0 is tied 8 units behind the head; the others trail it (NTL's spring rope,
// stepped at 60 per second whatever the frame rate). Between steps it is drawn part way from the
// last step to the next (ntl_tag_rope_pose), so it glides on any frame rate instead of jumping.
typedef struct {
  float x[NTL_TAG_ROPE], y[NTL_TAG_ROPE], vx[NTL_TAG_ROPE], vy[NTL_TAG_ROPE];
  float px[NTL_TAG_ROPE], py[NTL_TAG_ROPE];   // the step before (for drawing in between)
  float turn, pturn;                // the picture's angle (eases towards the rope's end direction), and the step before's
  double time;                      // ms of the last step
  bool laid;
} ntl_tag_rope;
void ntl_tag_rope_update(ntl_tag_rope* r, float head_x, float head_y, float ang, float scale, double now_ms);
// Where to draw it now (after ntl_tag_rope_update with the same time): points and picture angle.
void ntl_tag_rope_pose(const ntl_tag_rope* r, double now_ms, float* x, float* y, float* turn);

// MD5 of a string as 32 lowercase hex digits (how NTL sends tag passwords).
void ntl_tag_md5_hex(const char* text, char out[33]);

// What this player wears.
typedef struct {
  int mode;                         // 0: my own image tag (only on my screen), 1: an NTL tag
  int tag;                          // the NTL tag chosen (-1: none)
  bool show_others;                 // draw other players' NTL tags
  bool local_only;                  // the bought tag chosen is worn without its password: on my screen only
  int auth_count;
  struct { int id; char pass_md5[33]; } auth[32];   // bought tags whose password was accepted
} ntl_tag_settings;
extern ntl_tag_settings ntl_tag_prefs;
void ntl_tag_settings_load(const char* path);
bool ntl_tag_settings_save(const char* path);
// The password hash for a bought tag, or NULL when not accepted yet.
const char* ntl_tag_auth_for(int id);
void ntl_tag_auth_set(int id, const char* pass_md5);   // NULL removes it
// The NTL tag this player shows (others see it): -1 when none or not allowed.
int ntl_tag_own(void);
// The NTL tag drawn on my own snake here: what others see, or a bought tag worn without its
// password on my screen only (after the warning). -1: none.
int ntl_tag_own_local(void);

// NTL's tag server's latest word: which NTL id wears which tag (set from its thread).
void ntl_tag_server_map_set(const uint16_t* ids, const int16_t* tags, int count);
// The tag the server gives this NTL id: >= 0, or -1 when none / not listed.
int ntl_tag_server_lookup(int ntl_id);
void ntl_tag_server_map_clear(void);
#endif
