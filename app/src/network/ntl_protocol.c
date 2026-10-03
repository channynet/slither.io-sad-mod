// Independently implemented interoperability adapter; no NTL code is included.
#include "ntl_protocol.h"
#include "../external/mongoose.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

unsigned ntl_snake_id(unsigned id, unsigned serial) {
  return ((serial & 63u) << 10) | (id & 1023u);
}
static bool field(struct mg_str row, const char* path, char* dst, size_t cap) {
  struct mg_str token = mg_json_get_tok(row, path);
  if (!token.len) { dst[0] = 0; return true; }
  char* value = token.buf[0] == '"' ? mg_json_get_str(row, path) : NULL;
  size_t len = value ? strlen(value) : token.len;
  bool ok = len < cap;
  if (ok) { memcpy(dst, value ? value : token.buf, len); dst[len] = 0; }
  free(value);
  return ok;
}
static double number(struct mg_str row, const char* path) {
  char value[64];
  if (!field(row, path, value, sizeof(value)) || !value[0]) return NAN;
  char* end; double n = strtod(value, &end);
  return *end || !isfinite(n) || fabs(n) > 1000000000 ? NAN : n;
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
  if (!append(out,cap,"/slither/ntlplay-mt.php?ver=9.68&di=1000&bot=false&sos=false&food=false&tlm=&dt=SlitherNative-0.1")) return false;
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
    if (!append(out,cap,"&an=false&cs=-1&tg=-1&tar=")) return false;
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
  while (*input && n+1<cap) {
    if (!strncmp(input,"<br>",4)) { out[n++]='\n'; input+=4; continue; }
    if (*input=='<') { while (*input && *input!='>') input++; if (*input) input++; continue; }
    const char* entities[]={"&nbsp;","&amp;","&lt;","&gt;","&quot;","&#39;"};
    const char chars[]={' ','&','<','>','"','\''};
    bool found=false;
    for (int i=0;i<6;i++) if (!strncmp(input,entities[i],strlen(entities[i]))) {
      out[n++]=chars[i]; input+=strlen(entities[i]); found=true; break;
    }
    if (!found) { unsigned char c=*input++; if (c>=32 || c=='\n') out[n++]=c; }
  }
  if (cap) out[n]=0;
}
