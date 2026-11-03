#ifndef TAG_SHARE_H
#define TAG_SHARE_H
// Tags shared between players of this client (tools/tag-share-worker: a small Cloudflare Worker).
// While playing, the client tells the relay which snake it is on which game server and what tag
// it wears (an NTL tag, a bought one worn without its password too, or its own image tag), and
// learns the same of every other player of this client there. Image tags travel as PNG files
// named by their hash, kept in tag-share/.
#include <stdbool.h>
#include <stddef.h>
typedef struct tenv tenv;

#define TAG_SHARE_DIR "tag-share"
enum { SHARE_NONE = 0, SHARE_NTL = 1, SHARE_IMAGE = 2 };
typedef struct {
  int sid, kind, ntl;                 // the game server's snake id; SHARE_*; the NTL tag
  char hash[17];                      // SHARE_IMAGE: the picture
  float size, gap, angle; bool sway;  // SHARE_IMAGE: as its owner set it
} share_entry;

// My own image tag, for the relay: its PNG (at most 120 KB), hash and settings.
typedef struct { const unsigned char* png; size_t length; char hash[17]; float size, gap, angle; bool sway; } share_image;

void tag_share_tick(tenv* env);       // each frame (main thread)
// Another player's shared tag on the server I'm on (false: none).
bool tag_share_lookup(int sid, share_entry* out);
// The picture's file when it is here; otherwise false (and it is fetched).
bool tag_share_image_path(const char* hash, char* path, size_t cap);
const char* tag_share_status(void);
void tag_share_destroy(void);

// FNV-1a 64 of the bytes as 16 hex digits (the relay checks it).
void tag_share_hash(const unsigned char* bytes, size_t length, char out[17]);
// The relay's room reply, {"p":[[sid,"ntl",id],[sid,"img",hash,size,gap,angle,sway],...]}: up to cap entries.
int tag_share_parse(const char* json, size_t length, share_entry* out, int cap);
#endif
