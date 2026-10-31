#ifndef UTF8_TEXT_H
#define UTF8_TEXT_H
#include <stddef.h>
#include <string.h>
// Return one complete, valid UTF-8 scalar's byte length, or zero.
static inline size_t utf8_char_bytes(const char* text,size_t available) {
  if(!available) return 0;
  const unsigned char* s=(const unsigned char*)text;
  unsigned c=s[0],n=c<0x80?1:c>=0xc2&&c<=0xdf?2:c>=0xe0&&c<=0xef?3:c>=0xf0&&c<=0xf4?4:0;
  if(!n || n>available) return 0;
  for(unsigned i=1;i<n;i++) if((s[i]&0xc0)!=0x80) return 0;
  if(n==3 && ((c==0xe0&&s[1]<0xa0)||(c==0xed&&s[1]>=0xa0))) return 0;
  if(n==4 && ((c==0xf0&&s[1]<0x90)||(c==0xf4&&s[1]>=0x90))) return 0;
  return n;
}
static inline size_t utf8_scalar(unsigned c,char out[4]) {
  if(c>0x10ffff || (c>=0xd800&&c<=0xdfff)) return 0;
  if(c<0x80) { out[0]=(char)c; return 1; }
  if(c<0x800) { out[0]=(char)(0xc0|(c>>6)); out[1]=(char)(0x80|(c&63)); return 2; }
  if(c<0x10000) { out[0]=(char)(0xe0|(c>>12)); out[1]=(char)(0x80|((c>>6)&63)); out[2]=(char)(0x80|(c&63)); return 3; }
  out[0]=(char)(0xf0|(c>>18)); out[1]=(char)(0x80|((c>>12)&63)); out[2]=(char)(0x80|((c>>6)&63)); out[3]=(char)(0x80|(c&63)); return 4;
}
static inline size_t utf8_copy(char* out,size_t capacity,const char* text) {
  size_t used=0,left=strlen(text);
  while(left && capacity) {
    size_t n=utf8_char_bytes(text,left);
    if(!n || used+n>=capacity) break;
    memcpy(out+used,text,n); used+=n; text+=n; left-=n;
  }
  if(capacity) out[used]=0;
  return used;
}
#endif
