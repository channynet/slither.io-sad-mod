#ifndef NTL_PROTOCOL_H
#define NTL_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#define NTL_MEMBERS 128
#define NTL_MESSAGE 16001
#define NTL_TARGET 48
typedef struct {
  char nick[257], server[161], message[NTL_MESSAGE];
  char target[NTL_TARGET];
  double x, y;
  int sid, score;
  int tag;       // the NTL tag the teammate wears (-1: none)
  int fps, ping; // -1 when not supplied or invalid; dt is NTL's display telemetry.
  bool position, sos;
} ntl_member;
typedef struct {
  ntl_member rows[NTL_MEMBERS];
  int count;
  bool own_echo;
} ntl_reply;
typedef struct {
  char auth[513], team[513], nick[257], server[161], message[5401];
  char target[NTL_TARGET];
  double x, y;
  int sid, score, rank;
  int tag;                  // the NTL tag worn (-1: none)
  int fps, ping, ping_peak;  // ping_peak: highest ping of the last 10 s (NTL sends it in brackets)
  bool playing;
  bool sos, bot;            // NTL flags (sent only while playing)
} ntl_request;
// 0 = valid array, 1 = rejected key, 2 = malformed/over capacity.
int ntl_parse(const char* text, const char* identity, ntl_reply* out);
bool ntl_build_path(const ntl_request* request, char* out, size_t capacity);
bool ntl_encode_chat(const char* input, char* out, size_t capacity);
void ntl_plain(const char* input, char* out, size_t capacity);
unsigned ntl_snake_id(unsigned id, unsigned serial);
bool ntl_target_valid(const char* target);
#endif
