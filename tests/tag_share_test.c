// Tag sharing: the relay's hash (FNV-1a 64, as tools/tag-share-worker checks it) and its room replies.
#include "../app/src/network/tag_share.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  char h[17];
  tag_share_hash((const unsigned char*)"", 0, h); assert(!strcmp(h, "cbf29ce484222325"));
  tag_share_hash((const unsigned char*)"a", 1, h); assert(!strcmp(h, "af63dc4c8601ec8c"));
  tag_share_hash((const unsigned char*)"foobar", 6, h); assert(!strcmp(h, "85944171f73967e8"));
  const char* reply = "{\"p\":[[12,\"ntl\",590],[13,\"img\",\"0123456789abcdef\",150,30,10,1],"
                      "[14,\"img\",\"NOTHEX\",1,2,3,0],[15,\"bad\",1],[70000,\"ntl\",3],[16,\"img\",\"fedcba9876543210\",9999,-5,500,0]]}";
  share_entry e[8];
  int n = tag_share_parse(reply, strlen(reply), e, 8);
  assert(n == 3);
  assert(e[0].sid == 12 && e[0].kind == SHARE_NTL && e[0].ntl == 590);
  assert(e[1].sid == 13 && e[1].kind == SHARE_IMAGE && !strcmp(e[1].hash, "0123456789abcdef") && e[1].size == 150 && e[1].gap == 30 && e[1].angle == 10 && e[1].sway);
  assert(e[2].sid == 16 && e[2].size == 320 && e[2].gap == 0 && e[2].angle == 180 && !e[2].sway);   // (kept in range)
  assert(tag_share_parse("{\"p\":[]}", 8, e, 8) == 0 && tag_share_parse("oops", 4, e, 8) == 0);
  assert(tag_share_parse(reply, strlen(reply), e, 1) == 1);
  printf("Tag share: hash, room reply passed\n");
  return 0;
}
