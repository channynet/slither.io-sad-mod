#ifndef NTL_PROTOCOL_H
#define NTL_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#define NTL_MEMBERS 128
#define NTL_MESSAGE 16001
typedef struct {
  char nick[257], server[161], message[NTL_MESSAGE];
  double x, y;
  int sid, score;
  bool position, sos;
} ntl_member;
typedef struct {
  ntl_member rows[NTL_MEMBERS];
  int count;
  bool own_echo;
} ntl_reply;
typedef struct {
  char auth[513], team[513], nick[257], server[161], message[5401];
  double x, y;
  int sid, score, rank;
  bool playing;
} ntl_request;
// 0 = valid array, 1 = rejected key, 2 = malformed/over capacity.
int ntl_parse(const char* text, const char* identity, ntl_reply* out);
bool ntl_build_path(const ntl_request* request, char* out, size_t capacity);
bool ntl_encode_chat(const char* input, char* out, size_t capacity);
void ntl_plain(const char* input, char* out, size_t capacity);
unsigned ntl_snake_id(unsigned id, unsigned serial);
#endif
