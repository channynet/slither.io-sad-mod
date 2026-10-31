// Independently implemented interoperability adapter; no NTL code is included.
#include "ntl_protocol.h"
#include "utf8_text.h"
#include "../external/mongoose.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

unsigned ntl_snake_id(unsigned id, unsigned serial) {
  return ((serial & 63u) << 10) | (id & 1023u);
}
bool ntl_target_valid(const char* s) {
  // NTL TMS identity: skin_id + snake_id + (first color + pattern length).
  // Optional |1..6 is a ring assignment; keep it when relaying a sighting.
  if(!s || !*s || strlen(s)>=NTL_TARGET) return false;
  const unsigned bounds[]={255,65535,65535};
  for(int field=0;field<3;field++) {
    if(field==2 && (!*s || *s=='|')) break; // Unknown standard pattern in NTL.
    if(*s<'0'||*s>'9') return false;
    unsigned v=0;
    do { v=v*10+(*s++-'0'); if(v>bounds[field]) return false; } while(*s>='0'&&*s<='9');
    if(field<2 && *s++!='_') return false;
  }
  if(*s=='|') { s++; if(*s<'1'||*s>'6') return false; s++; }
  return !*s;
}
static int hex_digit(char c) {
  return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;
}
static bool hex4(const char* s,unsigned* value) {
  *value=0;
  for(int i=0;i<4;i++) { int h=hex_digit(s[i]); if(h<0) return false; *value=(*value<<4)|(unsigned)h; }
  return true;
}
// Mongoose's bundled unescaper accepts only one-byte \\u00xx values.
// Decode JSON strings here so Korean escapes never fall back to raw tokens.
static bool json_text(struct mg_str token,char* out,size_t capacity) {
  if(token.len<2 || token.buf[0]!='"' || token.buf[token.len-1]!='"' || !capacity) return false;
  size_t used=0;
  const char* s=token.buf+1; const char* end=token.buf+token.len-1;
  while(s<end) {
    char bytes[4]; const char* piece=s; size_t n;
    if(*s=='\\') {
      s++; if(s==end) return false;
      if(*s=='u') {
        unsigned cp; s++;
        if(end-s<4 || !hex4(s,&cp)) return false; s+=4;
        if(cp>=0xd800 && cp<=0xdbff) {
          unsigned low;
          if(end-s<6 || s[0]!='\\' || s[1]!='u' || !hex4(s+2,&low) || low<0xdc00 || low>0xdfff) return false;
          cp=0x10000+((cp-0xd800)<<10)+(low-0xdc00); s+=6;
        }
        if(!cp || !(n=utf8_scalar(cp,bytes))) return false;
      } else {
        const char* escaped="\"\\/bfnrt"; const char* decoded="\"\\/\b\f\n\r\t";
        const char* p=strchr(escaped,*s++); if(!p) return false;
        bytes[0]=decoded[p-escaped]; n=1;
      }
      piece=bytes;
    } else {
      if((unsigned char)*s<32 || !(n=utf8_char_bytes(s,(size_t)(end-s)))) return false;
      s+=n;
    }
    if(used+n>=capacity) return false;
    memcpy(out+used,piece,n); used+=n;
  }
  out[used]=0; return true;
}
static bool field(struct mg_str row, const char* path, char* dst, size_t cap) {
  struct mg_str token = mg_json_get_tok(row, path);
  if (!token.len) { dst[0] = 0; return true; }
  if(token.buf[0]=='"') return json_text(token,dst,cap);
  size_t len = token.len;
  bool ok = len < cap;
  if (ok) { memcpy(dst,token.buf,len); dst[len]=0; }
  return ok;
}
static double number(struct mg_str row, const char* path) {
  char value[64];
  if (!field(row, path, value, sizeof(value)) || !value[0]) return NAN;
  char* end; double n = strtod(value, &end);
  return *end || !isfinite(n) || fabs(n) > 1000000000 ? NAN : n;
}
static int metric(const char** cursor,int maximum) {
  const char* s=*cursor; while(*s==' ') s++;
  if(*s<'0'||*s>'9') return -1;
  unsigned value=0;
  while(*s>='0'&&*s<='9') {
    value=value*10+(unsigned)(*s++-'0');
    if(value>(unsigned)maximum) return -1;
  }
  *cursor=s; return (int)value;
}
static void telemetry(struct mg_str row,ntl_member* member) {
  member->fps=member->ping=-1;
  char raw[1024],plain[1024];
  if(!field(row,"$.dt",raw,sizeof(raw))) return;
  ntl_plain(raw,plain,sizeof(plain));
  const char* s=plain; while(*s==' ') s++;
  if(strncmp(s,"FPS:",4)) return;
  s+=4; int fps=metric(&s,10000);
  if(fps<0 || (*s && *s!=' ' && *s!='@')) return;
  member->fps=fps;
  while(*s==' ') s++; if(*s++!='@') return;
  int ping=metric(&s,60000); if(ping<0) return;
  if(*s=='(') { s++; if(metric(&s,60000)<0 || *s++!=')') return; }
  while(*s==' ') s++;
  if(strncmp(s,"ms",2) || (s[2] && s[2]!=' ' && s[2]!='@')) return;
  member->ping=ping;
}
int ntl_parse(const char* text, const char* identity, ntl_reply* out) {
  memset(out, 0, sizeof(*out));
  while (isspace((unsigned char)*text)) text++;
  const char* invalid = "bClBzw7tTJPV6GQ6gqmVsk5MR5oEclR3T1HcUDJR5FXisgl8L8IYXdTX01ZeF3k9Jor0UpxHavkxb73DEDM0SNRf23v0WHKtgJDHv40lWfhIxipG3LOIX0ZEUOtvVSeH";
  size_t length = strlen(text);
  while (length && isspace((unsigned char)text[length-1])) length--;
  if (length == strlen(invalid) && !strncmp(text, invalid, length)) return 1;
  if (length < 2 || text[0] != '[' || text[length-1] != ']') return 2;
  struct mg_str json = mg_str_n(text, length), row;
  int token_len = 0;
  if (mg_json_get(json, "$", &token_len) < 0 || token_len != (int)length) return 2;
  size_t offset = 0;
  while ((offset = mg_json_next(json, offset, NULL, &row)) != 0) {
    if (!row.len || row.buf[0] != '{' || out->count == NTL_MEMBERS) return 2;
    ntl_member* m = &out->rows[out->count];
    if (!field(row,"$.nick",m->nick,sizeof(m->nick))) return 2;
    if (!strcmp(m->nick,"00000000")) continue;
    if (strlen(m->nick) < 8 || !field(row,"$.srv",m->server,sizeof(m->server)) || !m->server[0] ||
        !field(row,"$.msg",m->message,sizeof(m->message))) return 2;
    m->x = number(row,"$.valx"); m->y = number(row,"$.valy");
    m->position = isfinite(m->x) && isfinite(m->y) && m->x >= 0 && m->y >= 0 && m->x <= 1000000 && m->y <= 1000000;
    double sid = number(row,"$.sid"), score = number(row,"$.score");
    m->sid = isfinite(sid) && sid >= 0 && sid <= 65535 ? (int)sid : -1;
    m->score = isfinite(score) && score >= 0 ? (int)score : 0;
    double tag = number(row,"$.tg");
    m->tag = isfinite(tag) && tag >= 0 && tag < 1000 ? (int)tag : -1;
    telemetry(row,m);
    if(!field(row,"$.tar",m->target,sizeof(m->target)) || !ntl_target_valid(m->target)) m->target[0]=0;
    char sos[16]; field(row,"$.sos",sos,sizeof(sos));
    m->sos = !strcmp(sos,"true") || !strcmp(sos,"1");
    if (!strcmp(m->nick, identity)) out->own_echo = true;
    out->count++;
  }
  return 0;
}
static bool append(char* out, size_t cap, const char* s) {
  size_t n = strlen(out), k = strlen(s);
  if (n + k >= cap) return false;
  memcpy(out + n, s, k + 1); return true;
}
static bool param(char* out, size_t cap, const char* key, const char* value) {
  if (!append(out, cap, "&") || !append(out, cap, key) || !append(out,cap,"=")) return false;
  static const char hex[] = "0123456789ABCDEF";
  for (const unsigned char* p = (const unsigned char*)value; *p; p++) {
    char escaped[4] = {0};
    if (isalnum(*p) || *p=='-' || *p=='_' || *p=='.' || *p=='~') escaped[0] = *p;
    else { escaped[0]='%'; escaped[1]=hex[*p >> 4]; escaped[2]=hex[*p & 15]; }
    if (!append(out,cap,escaped)) return false;
  }
  return true;
}
bool ntl_build_path(const ntl_request* r, char* out, size_t cap) {
  if (!cap) return false;
  out[0]=0;
  if (!append(out,cap,"/slither/ntlplay-mt.php?ver=9.68&di=1000&bot=")) return false;
  if (!append(out,cap,r->playing && r->bot ? "true" : "false") || !append(out,cap,"&sos=") ||
      !append(out,cap,r->playing && r->sos ? "true" : "false") || !append(out,cap,"&food=false&tlm=")) return false;
  char details[96]="";
  if(r->fps>=0 && r->fps<=10000) {
    if(r->playing && r->ping>=0 && r->ping<=60000) {
      // NTL's own format, "FPS: 60 @ 42(70) ms": the ping and in brackets the highest of the last 10 s.
      if(r->ping_peak>=r->ping && r->ping_peak<=60000) snprintf(details,sizeof(details),"FPS: %d @ %d(%d) ms",r->fps,r->ping,r->ping_peak);
      else snprintf(details,sizeof(details),"FPS: %d @ %d ms",r->fps,r->ping);
    }
    else snprintf(details,sizeof(details),"FPS: %d",r->fps);
  }
  if(!param(out,cap,"dt",details)) return false;
  if (!param(out,cap,"auth",r->auth) || !param(out,cap,"tid",r->team) || !param(out,cap,"nick",r->nick) ||
      !param(out,cap,"srv",r->playing ? r->server : "_GAME_MENU_") || !param(out,cap,"msg",r->message)) return false;
  if (r->playing && (!isfinite(r->x) || !isfinite(r->y) || r->x < 0 || r->y < 0 || r->x > 1000000 || r->y > 1000000 || r->sid < 0 || r->sid > 65535)) return false;
  char n[64];
  snprintf(n,sizeof(n),"%.1f",r->playing ? r->x : 0); if (!param(out,cap,"valx",n)) return false;
  snprintf(n,sizeof(n),"%.1f",r->playing ? r->y : 0); if (!param(out,cap,"valy",n)) return false;
  snprintf(n,sizeof(n),"%d",r->playing ? r->score : 0); if (!param(out,cap,"score",n)) return false;
  if (r->playing) {
    snprintf(n,sizeof(n),"%d",r->sid); if (!param(out,cap,"sid",n)) return false;
    snprintf(n,sizeof(n),"%d",r->rank); if (!param(out,cap,"rank",n)) return false;
    snprintf(n,sizeof(n),"&an=false&cs=-1&tg=%d",r->tag >= 0 && r->tag < 1000 ? r->tag : -1);
    if (!append(out,cap,n)) return false;
    if(r->target[0] && !ntl_target_valid(r->target)) return false;
    if(!param(out,cap,"tar",r->target)) return false;
  }
  return true;
}
bool ntl_encode_chat(const char* input, char* out, size_t cap) {
  if (!cap) return false;
  out[0]=0;
  if (!*input || strlen(input)>900) return false;
  for (const unsigned char* p=(const unsigned char*)input; *p; p++) {
    if (*p < 32 || *p==127) return false;
    const char* s = *p=='&' ? "&amp;" : *p=='<' ? "&lt;" : *p=='>' ? "&gt;" : *p==' ' ? "&nbsp;" : NULL;
    char single[2]={(char)*p,0};
    if (!append(out,cap,s ? s : single)) return false;
  }
  return true;
}
void ntl_plain(const char* input, char* out, size_t cap) {
  size_t n=0;
  const char* end=input+strlen(input);
  while (*input && n+1<cap) {
    if (!strncmp(input,"<br>",4)) { out[n++]='\n'; input+=4; continue; }
    if (*input=='<') { while (*input && *input!='>') input++; if (*input) input++; continue; }
    const char* entities[]={"&nbsp;","&amp;","&lt;","&gt;","&quot;","&#39;"};
    const char chars[]={' ','&','<','>','"','\''};
    bool found=false;
    for (int i=0;i<6;i++) if (!strncmp(input,entities[i],strlen(entities[i]))) {
      out[n++]=chars[i]; input+=strlen(entities[i]); found=true; break;
    }
    if(!found && input[0]=='&' && input[1]=='#') {
      const char* s=input+2; unsigned base=10,cp=0,digits=0;
      if(*s=='x'||*s=='X') { base=16; s++; }
      while(*s && digits<8) {
        int d=hex_digit(*s); if(d<0 || (unsigned)d>=base) break;
        cp=cp*base+(unsigned)d; digits++; s++;
      }
      char bytes[4]; size_t count=utf8_scalar(cp,bytes);
      if(digits && *s==';' && cp>=32 && cp!=127 && count) {
        if(n+count>=cap) break;
        memcpy(out+n,bytes,count); n+=count; input=s+1; found=true;
      }
    }
    if (!found) {
      unsigned char c=*input;
      if(c<32 && c!='\n') { input++; continue; }
      size_t count=utf8_char_bytes(input,(size_t)(end-input));
      if(!count) { input++; continue; }
      if(n+count>=cap) break;
      memcpy(out+n,input,count); n+=count; input+=count;
    }
  }
  if (cap) out[n]=0;
}
