#include "callback.h"
#include <stdarg.h>
#include "../platform/compat.h"
#include "server.h"
#include "network_pump.h"
#include "server_status.h"
#include "target_identity.h"
#include "socket_latency.h"
#include "../game/perf_trace.h"
#include "../game/death_review.h"
#include "../game/duel_board.h"

#include "../game/food.h"
#include "../game/snake.h"
#include "../game/ntl_tags.h"
#include "../user.h"

void snl(game_data* gdata, snake* o) {
  float orl = o->tl;
  o->tl = o->sct + fminf(1, o->fam);
  float d = o->tl - orl;
  int k = o->flpos;
  for (int j = 0; j < GD_EEZ; j++) {
    o->fls[k] -= d * gdata->data.xfas[j];
    k++;
    if (k >= GD_EEZ) k = 0;
  }
  o->fl = o->fls[o->flpos];
  o->fltg = GD_EEZ;
}

static char last_error[160];
static char end_reason[120];
static double opened_at;
void server_note_end(const char* format, ...) {
  if(end_reason[0]) return;
  va_list args; va_start(args,format); vsnprintf(end_reason,sizeof(end_reason),format,args); va_end(args);
}
static void log_session_end(tenv* env,bool playing) {
  WIN32_FILE_ATTRIBUTE_DATA info;
  if(GetFileAttributesExA("session-log.txt",GetFileExInfoStandard,&info) && info.nFileSizeLow>512*1024)
    MoveFileExA("session-log.txt","session-log.previous.txt",MOVEFILE_REPLACE_EXISTING);
  FILE* f=fopen("session-log.txt","ab"); if(!f) return;
  SYSTEMTIME now; GetLocalTime(&now);
  fprintf(f,"%04d-%02d-%02d %02d:%02d:%02d %s #%u %s %.1fs score %d: %s\n",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,
          env->usr->usrs.ipv4,(unsigned)env->usr->gdata.network_session,playing?"playing":"connecting",
          opened_at>0?(perf_now()-opened_at)/1000.0:0.0,env->usr->gdata.data.score,end_reason[0]?end_reason:"closed by the server");
  fclose(f);
  end_reason[0]=0; opened_at=0;
}
const char* server_last_error(void) { return last_error; }
void server_clear_error(void) { last_error[0]=0; }
void server_set_error(const char* format, ...) {
  va_list args; va_start(args,format); vsnprintf(last_error,sizeof(last_error),format,args); va_end(args);
}

void decode_secret(const uint8_t* packet, size_t packet_len, uint8_t* result) {
  uint8_t string[92] = {0};
  int string_idx = 0;
  int a, b, c = 23, d = 0, e = 0, f = 1;

  while (f < 184 && f < packet_len) {
    b = packet[f];
    f++;

    if (b <= 96) b += 32;
    b = (b - 97 - c) % 26;
    if (b < 0) b += 26;

    d *= 16;
    d += b;
    c += 17;

    if (e == 1) {
      string[string_idx++] = d;
      e = 0;
      d = 0;
    } else {
      e++;
    }
  }

  uint8_t secret_1[27] = {0};
  int idx = 0;
  for (int i = 0; i < 92; i++) {
    if ((i >= 9 && i <= 13) || (i >= 20 && i <= 41)) {
      secret_1[idx++] = string[i];
    }
  }

  int b2 = 0;
  for (int i = 0; i < 27; i++) {
    int d2 = 65;
    a = secret_1[i];

    if (a >= 97) {
      d2 += 32;
      a -= 32;
    }

    a -= 65;

    if (i == 0) {
      b2 = 3 + a;
    }

    int e2 = (a + b2) % 26;
    b2 += 2 + a;

    result[i] = e2 + d2;
  }
}

uint8_t* get_skin_compressed(tuser_data* usr) {
  user_settings* usrs = &usr->usrs;

  int skin_len = strlen(usrs->skin_code);
  uint8_t* reduced = tdarray_create(uint8_t);
  uint8_t sequence_count = 0;

  for (int i = 0; i < skin_len; i++) {
    sequence_count++;

    uint8_t cg_id = get_cg_id(&usr->gdata, usrs->skin_code[i]);

    if (usrs->skin_code[i + 1] != usrs->skin_code[i]) {
      tdarray_push(&reduced, &sequence_count);
      tdarray_push(&reduced, &cg_id);
      sequence_count = 0;
    }
  }
  return reduced;
}

void got_packet(tenv* env, uint8_t* a, int a_len) {
  if (a_len < 1 || death_review.frozen) return;
  tuser_data* usr = env->usr;
  tcontext* ctx = env->ctx;
  user_settings* usrs = &usr->usrs;
  game_data* gdata = &usr->gdata;
  struct mg_connection* c = gdata->connection;

  char cmd = (char)a[0];

  int alen = a_len;
  int plen = a_len;
  int dlen = a_len - 1;

  int m = 1;
  if (cmd == 'S') {
    if (a_len != 7) return;
    int id = ((unsigned)a[1] << 8) | a[2];
    uint32_t serial = ((uint32_t)a[3] << 24) | ((uint32_t)a[4] << 16) | ((uint32_t)a[5] << 8) | a[6];
    snake* peer = get_snake(gdata, id);
    if (peer) { peer->ntl_id = ((serial & 63u) << 10) | (id & 1023); peer->serial=serial; peer->has_serial=true; }
  } else if (cmd == '6') {
    uint8_t secret[27] = {0};
    decode_secret(a, a_len, secret);
    mg_ws_send(c, secret, 27, WEBSOCKET_OP_BINARY);

    int nick_len = strlen(usrs->nickname);
    uint8_t* ba = NULL;
    uint8_t* skin_compressed = NULL;
    int skin_compressed_len = 0;

    // An NTL tag chosen (a free one) rides in the skin code's header, as NTL sends it: with an
    // official skin, the skin code then spells out that skin's own colours.
    int own_tag = ntl_tag_own();
    bool tag_in_skin = own_tag >= 0 && ntl_tag_is_free(own_tag);
    bool send_skin = usrs->custom_skin || tag_in_skin;
    if (usrs->custom_skin) {
      skin_compressed = get_skin_compressed(usr);
    } else if (tag_in_skin) {
      skin_compressed = tdarray_create(uint8_t);
      const uint8_t* colours = gdata->default_skins[usrs->default_skin % NUM_DEFAULT_SKINS];
      for (int i = 1; i <= colours[0] && i < 64; ) {
        uint8_t run = 0, colour = colours[i];
        while (i <= colours[0] && i < 64 && colours[i] == colour && run < 255) { run++; i++; }
        tdarray_push(&skin_compressed, &run); tdarray_push(&skin_compressed, &colour);
      }
    }
    if (send_skin) {
      skin_compressed_len = tdarray_length(skin_compressed);
      ba = malloc(8 + 20 + nick_len + 8 + skin_compressed_len);
    } else {
      ba = malloc(8 + 20 + nick_len);
    }

    ba[0] = 115;
    ba[1] = 30;
    ba[2] = CLIENT_VERSION >> 8 & 255;
    ba[3] = CLIENT_VERSION & 255;
    uint8_t cwa[20] = {54, 206, 204, 169, 97, 178, 74,  136, 124, 117,
                       14, 210, 106, 236, 8,  208, 136, 213, 140, 111};
    for (int i = 0; i < 20; i++) {
      ba[4 + i] = cwa[i];
    }
    ba[24] = usrs->default_skin;
    ba[25] = nick_len;
    int m = 26;
    for (int i = 0; i < nick_len; i++) {
      ba[m] = (uint8_t)usrs->nickname[i];
      m++;
    }
    ba[m] = 0;
    m++;
    ba[m] = usrs->accessory;
    m++;

    if (send_skin) {
      uint8_t header[8] = {255, 255, 255, 0, 0, 0, rand() % 256, rand() % 256};
      if (tag_in_skin) ntl_tag_write_skin_header(header, own_tag, usrs->custom_skin ? 255 : usrs->default_skin);
      for (int i = 0; i < 8; i++) ba[m++] = header[i];

      for (int i = 0; i < skin_compressed_len; i++) {
        ba[m] = skin_compressed[i];
        m++;
      }

      tdarray_destroy(skin_compressed);
    }

    mg_ws_send(c, ba, m, WEBSOCKET_OP_BINARY);
    free(ba);
  } else if (cmd == 'a') {
    gdata->data.grd = a[m] << 16 | a[m + 1] << 8 | a[m + 2];
    m += 3;
    int nmscps = a[m] << 8 | a[m + 1];
    m += 2;
    gdata->data.sector_size = a[m] << 8 | a[m + 1];
    gdata->data.ssd256 = gdata->data.sector_size / 256.0f;
    m += 2;
    m += 2;
    gdata->data.spangdv = a[m] / 10.0f;
    m++;
    gdata->data.nsp1 = (a[m] << 8 | a[m + 1]) / 100.0f;
    m += 2;
    gdata->data.nsp2 = (a[m] << 8 | a[m + 1]) / 100.0f;
    m += 2;
    gdata->data.nsp3 = (a[m] << 8 | a[m + 1]) / 100.0f;
    m += 2;
    gdata->data.mamu = (a[m] << 8 | a[m + 1]) / 1e3;
    m += 2;
    gdata->data.mamu2 = (a[m] << 8 | a[m + 1]) / 1e3;
    m += 2;
    gdata->data.cst = (a[m] << 8 | a[m + 1]) / 1e3;
    m += 2;

    if (m < alen) {
      gdata->data.protocol_version = a[m];
      m++;
    }
    if (m < alen) {
      gdata->data.default_msl = a[m];
      m++;
    }

    gdata->data.server_number = 0;
    if (m + 1 < alen) {
      int number = a[m] << 8 | a[m + 1];
      gdata->data.server_number = number > 0 && number <= 9999 ? number : 0;
    }
    if (m < alen) {
      m += 2;
    }

    gdata->data.flux_announced = m < alen;
    if (m < alen) {
      gdata->data.flux_grd = a[m] << 16 | a[m + 1] << 8 | a[m + 2];
      m += 3;
    } else
      gdata->data.flux_grd = gdata->data.grd * .98;
    gdata->data.real_flux_grd = gdata->data.flux_grd;

    for (int i = 0; i < GD_FLXC; i++)
      gdata->data.flux_grds[i] = gdata->data.flux_grd;

    recalc_sep_mults(gdata);
    set_mscps(gdata, nmscps);
  } else if (cmd == 's') {
    int id = a[m] << 8 | a[m + 1];
    m += 2;

    if (dlen > 6) {
      snake o = {0};
      float ang =
          (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) * 2 * PI / 16777215.0f;
      m += 3;
      int dir = a[m] - 48;
      m++;
      float wang =
          (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) * 2 * PI / 16777215.0f;
      m += 3;
      float speed = (a[m] << 8 | a[m + 1]) / 1E3;
      m += 2;
      float fam = (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) / 16777215.0f;
      m += 3;
      int cv = a[m];
      m++;
      float snx = (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) / 5.0f;
      m += 3;
      float sny = (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) / 5.0f;
      m += 3;
      int nl = a[m];
      m++;
      size_t cp_len = GLM_MIN(nl, MAX_NICKNAME_LEN);
      memcpy(o.nk, a + m, cp_len);
      o.nk[cp_len] = '\0';
      m += nl;
      int skl = a[m];
      m++;
      if(skl>alen-m) return;
      target_identity(o.target_key,id,cv,a+m,skl,gdata->default_skins);
      // NTL tags ride in the skin code's header; an NTL skin code may stand for an official skin.
      o.ntl_tag = ntl_tag_from_skin_header(a + m, skl);
      o.official_skin = skl == 0 ? cv % NUM_DEFAULT_SKINS
                        : skl >= 8 && (a[m + 1] == 18 || a[m + 1] == 38) && a[m + 2] != 255 ? a[m + 2] % NUM_DEFAULT_SKINS : -1;
      if (skl > 0) {
        // printf("TAG DATA:\n");
        // for (int unread = 0; unread < 8; unread++) {
        //   printf("[%d] = %d\n", unread, a[m + unread]);
        // }
        // printf("TAG DATA END:\n");

        for (int j = 8; j+1 < skl; j += 2) {
          for (int i = 0; i < a[m + j]; i++) {
            if (o.cusk_len < MAX_SKIN_CODE_LEN) {
              o.cusk_data[o.cusk_len++] = a[m + j + 1];
            }
          }
        }
      }
      m += skl;
      uint8_t accessory = a[m++];
      float msl = gdata->data.default_msl;
      float xx = 0;
      float yy = 0;
      float lx = 0;
      float ly = 0;
      bool fp = false;
      int alen_m2 = alen - 2;

      int pts_dp_len = tdarray_length(gdata->data.pts_dp);
      body_part* pts;
      if (pts_dp_len) {
        pts = gdata->data.pts_dp[pts_dp_len - 1];
        tdarray_pop(gdata->data.pts_dp);
        tdarray_clear(pts);
      } else
        pts = tdarray_create(body_part);

      body_part po = {0};

      while (m < alen) {
        lx = xx;
        ly = yy;
        if (!fp) {
          // tail
          xx = (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) / 5.0f;
          m += 3;
          yy = (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) / 5.0f;
          m += 3;
          lx = xx;
          ly = yy;
          fp = true;
        } else if (m == alen_m2 && gdata->data.protocol_version >= 15) {
          // head (protocol 15+ ends with an angle; older servers send one more offset pair)
          float iang = a[m] << 8 | a[m + 1];
          po.iang = iang;
          m += 2;
          float ang = iang * GD_K64A;
          xx += cosf(ang) * gdata->data.default_msl;
          yy += sinf(ang) * gdata->data.default_msl;
        } else {
          // body
          xx += (a[m] - 127) / 2.0f;
          m++;
          yy += (a[m] - 127) / 2.0f;
          m++;
        }

        po.smu = 1;
        po.xx = xx;
        po.yy = yy;
        po.ltn = 1;
        po.ebx = xx - lx;
        po.eby = yy - ly;

        tdarray_push(&pts, &po);
      }

      int j = 0;
      int pts_len = tdarray_length(pts);
      float k = 1;
      for (int i = pts_len - 1; i >= 0; i--) {
        if (j < GD_SMUC_M3) {
          k = gdata->data.smus[j];
          j++;
        }
        pts[i].smu = k;
      }

      int gptz_dp_len = tdarray_length(gdata->data.gptz_dp);
      if (gptz_dp_len) {
        o.gptz = gdata->data.gptz_dp[gptz_dp_len - 1];
        tdarray_pop(gdata->data.gptz_dp);
        tdarray_clear(o.gptz);
      } else
        o.gptz = tdarray_create(gpt);

      o.pts = pts;
      if (pts) {
        o.pts = pts;
        o.sct = pts_len;
        if (pts[0].dying) o.sct--;
      };

      if (gdata->data.dead) {
        // player snake:
        usr->r->global.lview[0] = gdata->data.lview_xx;
        usr->r->global.lview[1] = gdata->data.lview_yy;

        gdata->data.snake_id = id;
        gdata->data.dead = false;
        gdata->data.follow_view = true;

        gdata->data.view_xx = xx;
        gdata->data.view_yy = yy;
        gdata->data.md = false;
        gdata->data.wmd = false;

        gdata->data.lfsx = -1;
        gdata->data.lfsy = -1;
        gdata->data.lfcv = 0;
        gdata->data.lfvsx = -1;
        gdata->data.lfvsy = -1;
        gdata->data.lfesid = -1;

        gdata->conn = CONNECTED;
        if (gdata->data.protocol_version < MIN_PROTOCOL_VERSION ||
            gdata->data.protocol_version > PROTOCOL_VERSION) {
          printf("Protocol version %d is not supported.\n", gdata->data.protocol_version);
          snprintf(last_error,sizeof(last_error),"이 서버의 프로토콜 %d은(는) 지원하지 않습니다 (지원: %d~%d).",
                   gdata->data.protocol_version,MIN_PROTOCOL_VERSION,PROTOCOL_VERSION);
          server_note_end("unsupported protocol %d",gdata->data.protocol_version);
          c->is_closing = true;
        }
        glfwSetTime(0);
      }

      o.accessory = accessory;
      o.tl = o.sct + o.fam;
      o.cfl = o.tl - .6;
      o.id = id;
      o.ntl_id = id;
      static uint64_t next_generation;
      o.generation=++next_generation;
      // A server may send a snake again with the same id (a practice server resizing the
      // player's snake at once). The new body replaces the old one in its place in the list
      // (the player's own snake is the last one), so the player's snake stays the player's.
      int replace_at = -1;
      for (int i = tdarray_length(gdata->data.snakes) - 1; i >= 0; i--) {
        snake* e = gdata->data.snakes + i;
        if (e->id != id || e->dead) continue;
        o.generation = e->generation;
        o.alive_amt = e->alive_amt;               // no fade-in: it is the same snake
        tdarray_push(&gdata->data.pts_dp, &e->pts);
        tdarray_push(&gdata->data.gptz_dp, &e->gptz);
        replace_at = i;
        break;
      }
      o.xx = snx;
      o.yy = sny;
      o.cv = cv % NUM_DEFAULT_SKINS;
      o.cusk = skl != 0;
      o.sc = 1;
      o.ssp = gdata->data.nsp1 + gdata->data.nsp2 * o.sc;
      o.fsp = o.ssp + .1;
      o.msp = gdata->data.nsp3;
      o.ehang = ang;
      o.wehang = ang;
      o.msl = msl;
      o.ang = ang;
      o.eang = o.wang = wang;
      o.sp = o.tsp = speed;
      o.spang = o.sp / gdata->data.spangdv;
      if (o.spang > 1) o.spang = 1;
      o.fam = fam;
      o.sc = fminf(6, 1 + (o.sct - 2) / 106.0f);
      o.scang = .13 + .87 * powf((7 - o.sc) / 6.0f, 2);
      o.ssp = gdata->data.nsp1 + gdata->data.nsp2 * o.sc;
      o.fsp = o.ssp + .1;
      o.wsep = 6 * o.sc;
      float mwsep = GD_NSEP / 1;
      if (o.wsep < mwsep) o.wsep = mwsep;
      o.sep = o.wsep;
      snl(gdata, &o);
      if (replace_at >= 0) gdata->data.snakes[replace_at] = o;
      else tdarray_insert(&gdata->data.snakes, 0, &o);
    } else {
      bool is_kill = a[m] == 1;
      m++;
      int snakes_len = tdarray_length(gdata->data.snakes);
      for (int i = snakes_len - 1; i >= 0; i--) {
        snake* o = gdata->data.snakes + i;
        if (o->id == id) {
          duel_snake_removed(gdata, o, is_kill);
          // Preserve the last intact scene before the server removes our body.
          if(id==gdata->data.snake_id) server_note_end("own snake removed by the server (kill=%d)",(int)is_kill);
          if(is_kill && id==gdata->data.snake_id && death_review_begin(env)) return;
          if (o->id == gdata->data.snake_id) {
            o->id = gdata->data.snake_id = -1;
          } else
            o->id = -1234;
          if (is_kill) {
            o->dead = true;
            o->dead_amt = 0;
            o->edir = 0;
          } else {
            tdarray_push(&gdata->data.pts_dp, &o->pts);
            tdarray_push(&gdata->data.gptz_dp, &o->gptz);

            tdarray_remove(gdata->data.snakes, i);
          }
          break;
        }
      }
    }
  } else if (cmd == 'e' || cmd == 'E' || cmd == '3' || cmd == '4' ||
             cmd == '5' || cmd == 'd' || cmd == '7') {
    snake* o = NULL;
    bool is_my_snake = false;
    if (cmd == 'd' || cmd == '7' ||
        dlen <= 2 && (cmd == 'e' || cmd == 'E' || cmd == '3' || cmd == '4' ||
                      cmd == '5')) {
      int snakes_len = tdarray_length(gdata->data.snakes);
      o = gdata->data.snakes + (snakes_len - 1);
      is_my_snake = true;
    } else {
      int id = a[m] << 8 | a[m + 1];
      m += 2;
      o = get_snake(gdata, id);
      // Some servers address the player's own snake by id as well.
      is_my_snake = o && o->id == gdata->data.snake_id;
    }

    if (o) {
      int dir = -1;
      float ang = -1;
      float wang = -1;
      float speed = -1;

      if (plen == 6) {
        if (cmd == 'e')
          dir = 1;
        else
          dir = 2;
        ang = a[m] * 2 * PI / 256.0f;
        m++;
        wang = a[m] * 2 * PI / 256.0f;
        m++;
        speed = a[m] / 18.0f;
        m++;
      } else if (plen == 5 || plen == 3) {
        if (cmd == 'e') {
          ang = a[m] * 2 * PI / 256.0f;
          m++;
          speed = a[m] / 18.0f;
          m++;
        } else if (cmd == 'E') {
          dir = 1;
          wang = a[m] * 2 * PI / 256.0f;
          m++;
          speed = a[m] / 18.0f;
          m++;
        } else if (cmd == '4') {
          dir = 2;
          wang = a[m] * 2 * PI / 256.0f;
          m++;
          speed = a[m] / 18.0f;
          m++;
        } else if (cmd == '3') {
          dir = 1;
          ang = a[m] * 2 * PI / 256.0f;
          m++;
          wang = a[m] * 2 * PI / 256.0f;
          m++;
        } else {
          if (cmd == '5') {
            dir = 2;
            ang = a[m] * 2 * PI / 256.0f;
            m++;
            wang = a[m] * 2 * PI / 256.0f;
            m++;
          }
        }
      } else {
        if (plen == 4 || plen == 2) {
          if (cmd == 'e') {
            ang = a[m] * 2 * PI / 256.0f;
            m++;
          } else if (cmd == 'E') {
            dir = 1;
            wang = a[m] * 2 * PI / 256.0f;
            m++;
          } else if (cmd == '4') {
            dir = 2;
            wang = a[m] * 2 * PI / 256.0f;
            m++;
          } else if (cmd == '3') {
            speed = a[m] / 18.0f;
            m++;
          } else if (cmd == 'd') {
            dir = 1;
            ang = a[m] * 2 * PI / 256.0f;
            m++;
            wang = a[m] * 2 * PI / 256.0f;
            m++;
            speed = a[m] / 18.0f;
            m++;
          } else if (cmd == '7') {
            dir = 2;
            ang = a[m] * 2 * PI / 256.0f;
            m++;
            wang = a[m] * 2 * PI / 256.0f;
            m++;
            speed = a[m] / 18.0f;
            m++;
          }
        }
      }

      if (dir != -1) o->dir = dir;
      if (ang != -1) {
        float da = fmodf(ang - o->ang, PI2);
        if (da < 0) da += PI2;
        if (da > PI) da -= PI2;
        int k = o->fapos;
        for (int j = 0; j < GD_AFC; j++) {
          o->fas[k] -= da * gdata->data.afas[j];
          k++;
          if (k >= GD_AFC) k = 0;
        }
        o->fatg = GD_AFC;
        o->ang = ang;
      }
      if (wang != -1) {
        o->wang = wang;
        if (!is_my_snake) o->eang = wang;
      }
      if (speed != -1) {
        o->sp = speed;
        o->spang = o->sp / gdata->data.spangdv;
        if (o->spang > 1) o->spang = 1;
      }
    }
  } else if (cmd == 'h') {
    int id = a[m] << 8 | a[m + 1];
    m += 2;
    float fam = (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) / 16777215.0f;
    m += 3;
    snake* o = get_snake(gdata, id);
    if (o) {
      o->fam = fam;
      snl(gdata, o);
    }
  } else if (cmd == 'r') {
    int id = a[m] << 8 | a[m + 1];
    m += 2;
    snake* o = get_snake(gdata, id);
    if (o) {
      if (dlen >= 4) {
        o->fam = (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) / 16777215.0f;
        m += 3;
      }
      int pts_len = tdarray_length(o->pts);
      for (int j = 0; j < pts_len; j++)
        if (!o->pts[j].dying) {
          o->pts[j].dying = true;
          o->sct--;
          o->sc = fminf(6, 1 + (o->sct - 2) / 106.0f);
          o->scang = .13 + .87 * powf((7 - o->sc) / 6.0f, 2);
          o->ssp = gdata->data.nsp1 + gdata->data.nsp2 * o->sc;
          o->fsp = o->ssp + .1;
          o->wsep = 6 * o->sc;
          float mwsep = GD_NSEP / 1;
          if (o->wsep < mwsep) o->wsep = mwsep;
          break;
        }
      snl(gdata, o);
    }
  } else if (cmd == 'R') {
    int snakes_len = tdarray_length(gdata->data.snakes);
    snake* o = gdata->data.snakes + (snakes_len - 1);
    o->rsc = a[m];
    m++;
  } else if (cmd == 'g' || cmd == 'n' || cmd == 'G' || cmd == 'N' ||
             cmd == '+' || cmd == '=') {
    bool adding_only = cmd == 'n' || cmd == 'N' || cmd == '+';
    snake* o = NULL;
    bool is_my_snake = false;
    // Protocol 14 and older: no head angle, own-snake packets are told apart by length.
    bool angle_moves = gdata->data.protocol_version >= 15;
    if (angle_moves ? (cmd == 'G' || cmd == 'N' || cmd == '=' && dlen == 6 ||
                       cmd == '+' && dlen == 9)
                    : (cmd == 'g' && dlen == 4 || cmd == 'G' && dlen == 2 ||
                       cmd == 'n' && dlen == 7 || cmd == 'N' && dlen == 5)) {
      int snakes_len = tdarray_length(gdata->data.snakes);
      o = gdata->data.snakes + (snakes_len - 1);
      is_my_snake = true;
    } else {
      int id = a[m] << 8 | a[m + 1];
      m += 2;
      o = get_snake(gdata, id);
      // Some servers address the player's own snake by id as well.
      is_my_snake = o && o->id == gdata->data.snake_id;
    }
    if (o) {
      if (adding_only)
        o->sct++;
      else {
        int pts_len = tdarray_length(o->pts);
        for (int j = 0; j < pts_len; j++)
          if (!o->pts[j].dying) {
            o->pts[j].dying = true;
            break;
          }
      }

      int pts_len = tdarray_length(o->pts);
      int lpo_i = pts_len - 1;
      float dx, dy, ox, oy, xx, yy;
      float dltn;
      float dsmu;
      float osmu;
      float d;
      body_part tmppo = {0};
      body_part* po = &tmppo;
      float msl = o->msl;
      if (!angle_moves) {
        if (cmd == 'g' || cmd == 'n') {
          xx = a[m] << 8 | a[m + 1];
          m += 2;
          yy = a[m] << 8 | a[m + 1];
          m += 2;
        } else {
          xx = o->pts[lpo_i].xx + a[m] - 128;
          m++;
          yy = o->pts[lpo_i].yy + a[m] - 128;
          m++;
        }
      } else if (cmd == '+' || cmd == '=') {
        float iang = a[m] << 8 | a[m + 1];
        po->iang = iang;
        m += 2;
        xx = a[m] << 8 | a[m + 1];
        m += 2;
        yy = a[m] << 8 | a[m + 1];
        m += 2;
      } else {
        float iang;
        if (cmd == 'G' && dlen == 2 || cmd == 'N' && dlen == 5 ||
            cmd == 'g' && dlen == 4 || cmd == 'n' && dlen == 7) {
          iang = a[m] << 8 | a[m + 1];
          m += 2;
        } else
          iang = o->pts[lpo_i].iang;

        po->iang = iang;
        float ang = iang * GD_K64A;
        xx = o->pts[lpo_i].xx + cosf(ang) * msl;
        yy = o->pts[lpo_i].yy + sinf(ang) * msl;
      }

      if (adding_only) {
        o->fam = (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) / 16777215.0f;
        m += 3;
      }

      po->fpos = 0;
      po->ftg = 0;
      po->smu = 1;
      po->fsmu = 0;
      po->xx = xx;
      po->yy = yy;
      po->fx = 0;
      po->fy = 0;
      po->fltn = 0;
      po->da = 0;
      po->ltn = sqrtf(powf(po->xx - o->pts[lpo_i].xx, 2) +
                      powf(po->yy - o->pts[lpo_i].yy, 2)) /
                msl;
      po->ebx = po->xx - o->pts[lpo_i].xx;
      po->eby = po->yy - o->pts[lpo_i].yy;

      tdarray_push(&o->pts, po);
      po = o->pts + (tdarray_length(o->pts) - 1);

      if (o->iiv) {
        float hx = o->xx + o->fx;
        float hy = o->yy + o->fy;
        dx = hx - (o->pts[lpo_i].xx + o->pts[lpo_i].fx);
        dy = hy - (o->pts[lpo_i].yy + o->pts[lpo_i].fy);
        d = sqrtf(dx * dx + dy * dy);

        if (d > 1) {
          dx /= d;
          dy /= d;
        }
        float d2 = po->ltn * msl;
        float d3 = 0;
        if (d < msl)
          d3 = d;
        else
          d3 = d2;
        ox = o->pts[lpo_i].xx + o->pts[lpo_i].fx + dx * d3;
        oy = o->pts[lpo_i].yy + o->pts[lpo_i].fy + dy * d3;
        dltn = 1 - d3 / d2;
        dx = po->xx - ox;
        dy = po->yy - oy;
        int k = po->fpos;
        for (int j = 0; j < GD_EEZ; j++) {
          po->fxs[k] -= dx * gdata->data.xfas[j];
          po->fys[k] -= dy * gdata->data.xfas[j];
          po->fltns[k] -= dltn * gdata->data.xfas[j];
          k++;
          if (k >= GD_EEZ) k = 0;
        }
        po->fx = po->fxs[po->fpos];
        po->fy = po->fys[po->fpos];
        po->fltn = po->fltns[po->fpos];
        po->fsmu = po->fsmus[po->fpos];
        po->ftg = GD_EEZ;
      }
      int n2 = 3;
      pts_len = tdarray_length(o->pts);
      int k = pts_len - 3;
      int lmpo_i;
      int mpo_i;

      if (k >= 1) {
        lmpo_i = k;
        int n = 0;
        float mv = 0;
        dsmu = 0;
        for (int m = k - 1; m >= 0; m--) {
          mpo_i = m;
          n++;
          ox = o->pts[mpo_i].xx;
          oy = o->pts[mpo_i].yy;
          osmu = o->pts[mpo_i].smu;
          if (n <= 4) mv = gdata->data.cst * n / 4.0f;
          o->pts[mpo_i].xx += (o->pts[lmpo_i].xx - o->pts[mpo_i].xx) * mv;
          o->pts[mpo_i].yy += (o->pts[lmpo_i].yy - o->pts[mpo_i].yy) * mv;
          if (o->pts[mpo_i].smu != gdata->data.smus[n2]) {
            osmu = o->pts[mpo_i].smu;
            o->pts[mpo_i].smu = gdata->data.smus[n2];
            dsmu = o->pts[mpo_i].smu - osmu;
          } else
            dsmu = 0;
          if (n2 < GD_SMUC_M3) n2++;
          if (o->iiv) {
            dx = o->pts[mpo_i].xx - ox;
            dy = o->pts[mpo_i].yy - oy;
            k = o->pts[mpo_i].fpos;
            for (int j = 0; j < GD_EEZ; j++) {
              o->pts[mpo_i].fxs[k] -= dx * gdata->data.xfas[j];
              o->pts[mpo_i].fys[k] -= dy * gdata->data.xfas[j];
              o->pts[mpo_i].fsmus[k] -= dsmu * gdata->data.xfas[j];
              k++;
              if (k >= GD_EEZ) k = 0;
            }
            o->pts[mpo_i].fx = o->pts[mpo_i].fxs[o->pts[mpo_i].fpos];
            o->pts[mpo_i].fy = o->pts[mpo_i].fys[o->pts[mpo_i].fpos];
            o->pts[mpo_i].fsmu = o->pts[mpo_i].fsmus[o->pts[mpo_i].fpos];
            o->pts[mpo_i].ftg = GD_EEZ;
          }
          lmpo_i = mpo_i;
        }
      }
      o->sc = fminf(6, 1 + (o->sct - 2) / 106.0f);
      o->scang = .13 + .87 * powf((7 - o->sc) / 6.0f, 2);
      o->ssp = gdata->data.nsp1 + gdata->data.nsp2 * o->sc;
      o->fsp = o->ssp + .1;
      o->wsep = 6 * o->sc;
      float mwsep = GD_NSEP / 1;
      if (o->wsep < mwsep) o->wsep = mwsep;
      if (adding_only) snl(gdata, o);
      if (is_my_snake) {
        gdata->data.ovxx = o->xx + o->fx;
        gdata->data.ovyy = o->yy + o->fy;
      }

      float csp = o->sp * (gdata->data.etm / 8.0f) / 4.0f;
      csp *= gdata->data.lag_mult;
      float ochl = o->chl - 1;
      o->chl = csp / o->msl;
      dx = xx - o->xx;
      dy = yy - o->yy;
      float dchl = o->chl - ochl;
      o->xx = xx;
      o->yy = yy;
      k = o->fpos;
      for (int j = 0; j < GD_EEZ; j++) {
        o->fxs[k] -= dx * gdata->data.xfas[j];
        o->fys[k] -= dy * gdata->data.xfas[j];
        o->fchls[k] -= dchl * gdata->data.xfas[j];
        k++;
        if (k >= GD_EEZ) k = 0;
      }
      o->fx = o->fxs[o->fpos];
      o->fy = o->fys[o->fpos];
      o->fchl = o->fchls[o->fpos];
      o->ftg = GD_EEZ;
      if (is_my_snake) {
        float lvx = gdata->data.view_xx;
        float lvy = gdata->data.view_yy;
        if (gdata->data.follow_view) {
          gdata->data.view_xx = o->xx + o->fx;
          gdata->data.view_yy = o->yy + o->fy;
        }
        float dx = gdata->data.view_xx - gdata->data.ovxx;
        float dy = gdata->data.view_yy - gdata->data.ovyy;
        k = gdata->data.fvpos;
        for (int j = 0; j < GD_VFC; j++) {
          gdata->data.fvxs[k] -= dx * gdata->data.vfas[j];
          gdata->data.fvys[k] -= dy * gdata->data.vfas[j];
          k++;
          if (k >= GD_VFC) k = 0;
        }
        gdata->data.fvtg = GD_VFC;
      }
    }
  } else if (cmd == 'p') {
    if(!gdata->data.wfpr || !ping_received(&gdata->ping_tracker,glfwGetTime()*1000.0,&gdata->data.ping)) return;
    gdata->data.wfpr = false;
    gdata->data.pings[gdata->data.cping] = gdata->ping_tracker.last;
    perf_ping(gdata->data.pings[gdata->data.cping]);
    socket_latency latency=socket_latency_read((SOCKET)(uintptr_t)c->fd);
    perf_connection(gdata->network_session,latency.rtt_ms,latency.min_rtt_ms,latency.retrans,
      gdata->ping_tracker.queue_ms,gdata->network_poll_gap,gdata->prompt_ack);
    gdata->network_poll_gap=0;
    server_status_game_ping(usrs->ipv4,gdata->data.pings[gdata->data.cping]);
    gdata->data.cping = (gdata->data.cping + 1) % PING_SAMPLE_COUNT;
    if (gdata->data.lagging) {
      gdata->data.etm *= gdata->data.lag_mult;
      gdata->data.lagging = false;
    }
  } else if (cmd == 'z') {
    gdata->data.flux_announced = true;
    gdata->data.real_flux_grd = a[m] << 16 | a[m + 1] << 8 | a[m + 2];
    m += 3;
    int k = gdata->data.flux_grd_pos;
    for (int j = 0; j < GD_FLXC; j++) {
      gdata->data.flux_grds[k] =
          gdata->data.flux_grds[k] +
          (gdata->data.real_flux_grd - gdata->data.flux_grds[k]) *
              gdata->data.flxas[j];
      k++;
      if (k >= GD_FLXC) k = 0;
    }
    gdata->data.flx_tg = GD_FLXC;
  } else if (cmd == 'F') {
    int sx = a[m];
    m++;
    int sy = a[m];
    m++;
    float axx = sx * gdata->data.sector_size;
    float ayy = sy * gdata->data.sector_size;
    float xx, yy;
    int rx, ry;
    int cv;
    float rad;
    int id;
    while (m < alen) {
      cv = a[m];
      m++;
      rx = a[m];
      m++;
      ry = a[m];
      m++;
      xx = axx + rx * gdata->data.ssd256;
      yy = ayy + ry * gdata->data.ssd256;
      rad = a[m] / 5.0f;
      m++;
      id = sx << 24 | sy << 16 | rx << 8 | ry;
      tdarray_push(
          &gdata->data.foods,
          (&(food){.id = id,
                   .cv = cv % NUM_COLOR_GROUPS,
                   .cv2 = GLM_MIN(NUM_FOOD_SIZES - 1, GLM_MAX(0, (int)floorf(NUM_FOOD_SIZES * rad / 16.5f))),
                   .xx = xx,
                   .yy = yy,
                   .rsp = 3,
                   .rad = 1e-5,
                   .sz = rad,
                   .lrrad = 1e-5,
                   .gfr = rand() % 64,
                   .gr = 0.65f + 0.1f * rad,
                   .wsp = (2 * ((float)rand() / RAND_MAX) - 1) * 0.0225f,
                   .sx = sx,
                   .sy = sy}));
    }
  } else if (cmd == 'b' || cmd == 'f') {
    int sx, sy;
    if (dlen >= 5) {
      sx = a[m];
      m++;
      sy = a[m];
      m++;
      gdata->data.lfsx = sx;
      gdata->data.lfsy = sy;
    } else {
      sx = gdata->data.lfsx;
      sy = gdata->data.lfsy;
    }
    int rx = a[m];
    m++;
    int ry = a[m];
    m++;
    float xx = sx * gdata->data.sector_size + rx * gdata->data.ssd256;
    float yy = sy * gdata->data.sector_size + ry * gdata->data.ssd256;
    int id = sx << 24 | sy << 16 | rx << 8 | ry;
    int cv;
    if (dlen == 4 || dlen == 6) {
      cv = a[m];
      m++;
      gdata->data.lfcv = cv;
    } else
      cv = gdata->data.lfcv;
    float rad = a[m] / 5.0f;
    m++;
    tdarray_push(&gdata->data.foods,
                 (&(food){.id = id,
                          .cv = cv % NUM_COLOR_GROUPS,
                          .cv2 = GLM_MIN(NUM_FOOD_SIZES - 1, GLM_MAX(0, (int)floorf(NUM_FOOD_SIZES * rad / 16.5f))),
                          .xx = xx,
                          .yy = yy,
                          .rsp = (cmd == 'b') ? 3 : 1,
                          .rad = 1e-5,
                          .sz = rad,
                          .lrrad = 1e-5,
                          .gfr = rand() % 64,
                          .gr = 0.65f + 0.1f * rad,
                          .wsp = (2 * ((float)rand() / RAND_MAX) - 1) * 0.0225f,
                          .sx = sx,
                          .sy = sy}));
  } else if (cmd == 'c' || cmd == 'C' || cmd == '<') {
    int id;
    int ebid = -1;
    int sx, sy, rx, ry;
    if (cmd == 'c' && dlen == 2 || cmd == '<' && dlen == 4 ||
        cmd == 'C' && dlen == 2) {
      sx = gdata->data.lfvsx;
      sy = gdata->data.lfvsy;
    } else {
      sx = a[m];
      m++;
      sy = a[m];
      m++;
      gdata->data.lfvsx = sx;
      gdata->data.lfvsy = sy;
    }
    rx = a[m];
    m++;
    ry = a[m];
    m++;
    id = sx << 24 | sy << 16 | rx << 8 | ry;
    if (cmd == '<') {
      ebid = a[m] << 8 | a[m + 1];
      m += 2;
      gdata->data.lfesid = ebid;
    } else if (cmd == 'C')
      ebid = gdata->data.lfesid;

    int cm1 = (int)tdarray_length(gdata->data.foods) - 1;
    for (int i = cm1; i >= 0; i--) {
      food* fo = gdata->data.foods + i;
      if (fo->id == id) {
        fo->eaten = true;
        if (ebid >= 0) {
          fo->ebid = ebid;
          fo->eaten_fr = 0;
        } else {
          if (i != cm1) {
            gdata->data.foods[i] = gdata->data.foods[cm1];
          }
          tdarray_pop(gdata->data.foods);
          cm1--;
        }
        id = -1;
        break;
      }
    }
  } else if (cmd == 'w') {
    int xx = a[m];
    m++;
    int yy = a[m];
    m++;

    int cm1 = (int)tdarray_length(gdata->data.foods) - 1;
    for (int i = cm1; i >= 0; i--) {
      food* fo = gdata->data.foods + i;
      if (fo->sx == xx)
        if (fo->sy == yy) {
          if (i != cm1) {
            gdata->data.foods[i] = gdata->data.foods[cm1];
          }
          tdarray_pop(gdata->data.foods);
          cm1--;
        }
    }
  } else if (cmd == 'j') {
    int id = a[m] << 8 | a[m + 1];
    m += 2;
    float xx = 1 + (a[m] << 8 | a[m + 1]) * 3;
    m += 2;
    float yy = 1 + (a[m] << 8 | a[m + 1]) * 3;
    m += 2;
    prey* pr = NULL;
    int preys_len = tdarray_length(gdata->data.preys);
    for (int i = preys_len - 1; i >= 0; i--) {
      if (gdata->data.preys[i].id == id) {
        pr = gdata->data.preys + i;
        break;
      }
    }
    if (pr) {
      float csp = pr->sp * (gdata->data.etm / 8) / 4;
      csp *= gdata->data.lag_mult;
      float ox = pr->xx;
      float oy = pr->yy;
      if (dlen == 15) {
        pr->dir = a[m] - 48;
        m++;
        pr->ang =
            (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) * 2 * PI / 16777215.0f;
        m += 3;
        pr->wang =
            (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) * 2 * PI / 16777215.0f;
        m += 3;
        pr->sp = (a[m] << 8 | a[m + 1]) / 1E3;
        m += 2;
      } else if (dlen == 11) {
        pr->ang =
            (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) * 2 * PI / 16777215.0f;
        m += 3;
        pr->sp = (a[m] << 8 | a[m + 1]) / 1E3;
        m += 2;
      } else if (dlen == 12) {
        pr->dir = a[m] - 48;
        m++;
        pr->wang =
            (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) * 2 * PI / 16777215.0f;
        m += 3;
        pr->sp = (a[m] << 8 | a[m + 1]) / 1E3;
        m += 2;
      } else if (dlen == 13) {
        pr->dir = a[m] - 48;
        m++;
        pr->ang =
            (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) * 2 * PI / 16777215.0f;
        m += 3;
        pr->wang =
            (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) * 2 * PI / 16777215.0f;
        m += 3;
      } else if (dlen == 9) {
        pr->ang =
            (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) * 2 * PI / 16777215.0f;
        m += 3;
      } else if (dlen == 10) {
        pr->dir = a[m] - 48;
        m++;
        pr->wang =
            (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) * 2 * PI / 16777215.0f;
        m += 3;
      } else if (dlen == 8) {
        pr->sp = (a[m] << 8 | a[m + 1]) / 1E3;
        m += 2;
      }
      pr->xx = xx + cosf(pr->ang) * csp;
      pr->yy = yy + sinf(pr->ang) * csp;
      float dx = pr->xx - ox;
      float dy = pr->yy - oy;
      int k = pr->fpos;
      for (int j = 0; j < GD_EEZ; j++) {
        pr->fxs[k] -= dx * gdata->data.xfas[j];
        pr->fys[k] -= dy * gdata->data.xfas[j];
        k++;
        if (k >= GD_EEZ) k = 0;
      }
      pr->fx = pr->fxs[pr->fpos];
      pr->fy = pr->fys[pr->fpos];
      pr->ftg = GD_EEZ;
    }
  } else if (cmd == 'y') {
    int id = a[m] << 8 | a[m + 1];
    m += 2;
    if (dlen == 2) {
      int preys_len = tdarray_length(gdata->data.preys);
      for (int i = preys_len - 1; i >= 0; i--) {
        prey* pr = gdata->data.preys + i;
        if (pr->id == id) {
          tdarray_remove(gdata->data.preys, i);
          break;
        }
      }
    }
    else if (dlen == 4) {
      int ebid = a[m] << 8 | a[m + 1];
      m += 2;
      int preys_len = tdarray_length(gdata->data.preys);
      for (int i = preys_len - 1; i >= 0; i--) {
        prey* pr = gdata->data.preys + i;
        if (pr->id == id) {
          pr->eaten = true;
          pr->ebid = ebid;
          if (get_snake(gdata, ebid))
            pr->eaten_fr = 0;
          else {
            tdarray_remove(gdata->data.preys, i);
          }
          break;
        }
      }
    } else {
      int cv = a[m];
      m++;
      float xx = (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) / 5.0f;
      m += 3;
      float yy = (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) / 5.0f;
      m += 3;
      float rad = a[m] / 5.0f;
      m++;
      int dir = a[m] - 48;
      m++;
      float wang =
          (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) * 2 * PI / 16777215.0f;
      m += 3;
      float ang =
          (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) * 2 * PI / 16777215.0f;
      m += 3;
      float speed = (a[m] << 8 | a[m + 1]) / 1E3;
      m += 2;

      tdarray_push(&gdata->data.preys, (&(prey){
        .id = id,
        .xx = xx,
        .yy = yy,
        .rad = 1e-5,
        .sz = rad,
        .cv = cv % NUM_COLOR_GROUPS,
        .dir = dir,
        .wang = wang,
        .ang = ang,
        .sp = speed,
        .gfr = rand() % 64,
        .gr = 0.5f + ((float)rand() / (float)RAND_MAX) * 0.15f + 0.1f * 6, // * rad
        .cv2 = GLM_MIN(NUM_PREY_SIZES - 1, GLM_MAX(0, (int)floorf(NUM_PREY_SIZES * rad / 9)))
      }));
    }
  } else if (cmd == 'u') {
    // Protocol 14 and older: fixed 80x80 minimap, filled forward from the top-left.
    gdata->data.mmsz = 80;
    for (int row = 0; row < 80; row++)
      memset(gdata->data.mm_data + row * MAX_MINIMAP_SIZE, 0, 80);
    int i;
    int k = 0;
    int xx = 0;
    int yy = 0;
    while (m < alen) {
      if (yy >= 80) break;
      k = a[m++];
      if (k >= 128) {
        k -= 128;
        for (i = 0; i < k; i++) {
          xx++;
          if (xx >= 80) {
            xx = 0;
            yy++;
            if (yy >= 80) break;
          }
        }
      } else
        for (i = 0; i < 7; i++) {
          if ((k & gdata->u_m[i]) > 0) {
            gdata->data.mm_data[yy * MAX_MINIMAP_SIZE + xx] = 255;
          }
          xx++;
          if (xx >= 80) {
            xx = 0;
            yy++;
            if (yy >= 80) break;
          }
        }
    }
    gdata->data.mmgad = true;
  } else if (cmd == 'M') {
    int sz = a[m] << 8 | a[m + 1];
    m += 2;
    if (sz > MAX_MINIMAP_SIZE) sz = MAX_MINIMAP_SIZE;
    if (gdata->data.mmsz != sz) gdata->data.mmsz = sz;

    for (int row = 0; row < gdata->data.mmsz; row++)
      memset(gdata->data.mm_data + row * MAX_MINIMAP_SIZE, 0, gdata->data.mmsz);

    int i;
    int k = 0;
    int xx = gdata->data.mmsz - 1;
    int yy = gdata->data.mmsz - 1;
    while (m < alen) {
      if (yy < 0) break;
      k = a[m++];
      if (k >= 128) {
        if (k == 255)
          k = 126 * a[m++];
        else
          k -= 128;
        for (i = 0; i < k; i++) {
          xx--;
          if (xx < 0) {
            xx = gdata->data.mmsz - 1;
            yy--;
            if (yy < 0) break;
          }
        }
      } else
        for (i = 0; i < 7; i++) {
          if ((k & gdata->u_m[i]) > 0) {
            gdata->data.mm_data[yy * MAX_MINIMAP_SIZE + xx] = 255;
          }
          xx--;
          if (xx < 0) {
            xx = gdata->data.mmsz - 1;
            yy--;
            if (yy < 0) break;
          }
        }
    }
    if (!gdata->data.mmgad) {
      gdata->data.mmgad = true;
    }
  } else if (cmd == 'V') {
    int i;
    int j;
    int k = 0;
    int xx = gdata->data.mmsz - 1;
    int yy = gdata->data.mmsz - 1;
    while (m < alen) {
      if (yy < 0) break;
      k = a[m++];
      if (k >= 128) {
        if (k == 255)
          k = 126 * a[m++];
        else
          k -= 128;
        for (i = 0; i < k; i++) {
          xx--;
          if (xx < 0) {
            xx = gdata->data.mmsz - 1;
            yy--;
            if (yy < 0) break;
          }
        }
      } else
        for (i = 0; i < 7; i++) {
          if ((k & gdata->u_m[i]) > 0) {
            j = yy * MAX_MINIMAP_SIZE + xx;
            if (gdata->data.mm_data[j] == 255) {
              gdata->data.mm_data[j] = 0;
            } else {
              gdata->data.mm_data[j] = 255;
            }
          }
          xx--;
          if (xx < 0) {
            xx = gdata->data.mmsz - 1;
            yy--;
            if (yy < 0) break;
          }
        }
    }
  } else if (cmd == 'l') {
    if(alen<6) return;
    gdata->data.gotlb = true;

    gdata->data.lb_pos = a[m];
    int pos = 0;
    m++;
    gdata->data.rank = a[m] << 8 | a[m + 1];
    m += 2;
    gdata->data.slither_count = a[m] << 8 | a[m + 1];
    server_status_population(usrs->ipv4,gdata->data.slither_count);
    m += 2;

    while (m < alen) {
      pos++;
      int sct = a[m] << 8 | a[m + 1];
      m += 2;
      float fam = (a[m] << 16 | a[m + 1] << 8 | a[m + 2]) / 16777215.0f;
      m += 3;
      gdata->data.lb.entries[pos - 1].score = (int) floorf((gdata->data.fpsls[sct] + fam / gdata->data.fmlts[sct] - 1) * 15 - 5) / 1;
      gdata->data.lb.entries[pos - 1].cv = a[m] % 9;
      m++;
      int nl = a[m];
      m++;
      if (gdata->data.lb_pos == pos) {
        memcpy(gdata->data.lb.entries[pos - 1].nickname, usrs->nickname, MAX_NICKNAME_LEN + 1);
      } else {
        char nk[MAX_NICKNAME_LEN + 1] = {0};
        size_t cp_len = GLM_MIN(nl, MAX_NICKNAME_LEN);
        memcpy(gdata->data.lb.entries[pos - 1].nickname, a + m, cp_len);
        gdata->data.lb.entries[pos - 1].nickname[cp_len] = '\0';
      }
      m += nl;
    }
  } else if (cmd == 'k') {
    gdata->data.kills++;
    duel_kill_confirmed();
  } else if (cmd == 'v') {
    server_note_end("game over packet from the server (v=%d)",dlen>0?(int)a[1]:-1);
    if(dlen<1 || a[1]!=2) duel_game_over(gdata);
    gdata->data.follow_view = false;
    gdata->data.lview_xx = gdata->data.view_xx;
    gdata->data.lview_yy = gdata->data.view_yy;
    usrs->kills = gdata->data.kills;
    usrs->score = gdata->data.score;
    usrs->play_time = gdata->data.play_etm;

    save_user_settings(usrs);

    if (death_review_begin(env)) return;
    if (usrs->instant_restart) {
      // (already noted as a game over packet; the restart is automatic)
      gdata->restart_req = true;
      c->is_closing = true;
    }
  }
}

void server_callback(struct mg_connection* c, int ev, void* ev_data) {
  tenv* env = c->fn_data;
  tuser_data* usr = env->usr;
  tcontext* ctx = env->ctx;
  game_data* gdata = &usr->gdata;

  if (ev == MG_EV_OPEN) {
    end_reason[0]=0; opened_at=perf_now();
    if(!game_prepare_receive(c)) { server_note_end("could not allocate the receive buffer"); c->is_closing=true; }
    printf("Connection opened\n");
  } else if(ev==MG_EV_CONNECT) {
    gdata->prompt_ack=socket_prompt_ack((SOCKET)(uintptr_t)c->fd);
  } else if(ev==MG_EV_WRITE) {
    long bytes=*(long*)ev_data;
    if(bytes>0) ping_written(&gdata->ping_tracker,glfwGetTime()*1000.0,(size_t)bytes);
  } else if(ev==MG_EV_READ) {
    long bytes=*(long*)ev_data;
    if(bytes>0) perf_received((size_t)bytes);
  } else if (ev == MG_EV_WS_OPEN) {
    printf("Connection established\n");
    // Nothing has been sent yet, so the server has not created a snake for this connection.
    if(server_path_check(env,c)) { server_note_end("slow path, reconnecting"); c->is_closing = true; return; }

    mg_ws_send(c, (uint8_t[]){1}, 1, WEBSOCKET_OP_BINARY);
    mg_ws_send(c, (uint8_t[]){'c', 0}, 2, WEBSOCKET_OP_BINARY);
  } else if (ev == MG_EV_WS_MSG) {
    struct mg_ws_message* msg = (struct mg_ws_message*)ev_data;
    uint8_t* a = (uint8_t*)msg->data.buf;
    if (msg->data.len == 0) return;
    int m = 0;
    if (a[m] < 32) {
      int l = msg->data.len;
      while (m < l) {
        int len;
        if (a[m] < 32) {
          if (m + 1 >= l) { server_note_end("truncated packet header (%d bytes)",l); c->is_closing = true; return; }
          len = a[m] << 8 | a[m + 1];
          m += 2;
        } else {
          len = a[m] - 32;
          m++;
        }
        uint8_t* a2 = a + m;
        if (len < 1 || len > l - m) { server_note_end("bad packet length %d at %d of %d",len,m,l); c->is_closing = true; return; }
        got_packet(env, a2, len);
        m += len;
      }
    } else {
      uint8_t* a2 = a + m;
      int len = msg->data.len - m;
      got_packet(env, a2, len);
    }
  } else if (ev == MG_EV_ERROR) {
    printf("Connection error: %s, closing connection...\n", (char*)ev_data);
    server_note_end("socket error: %.80s",(char*)ev_data);
    // Only before the game starts: a socket error after playing is the normal end of a session.
    if(!last_error[0] && gdata->conn!=CONNECTED) snprintf(last_error,sizeof(last_error),"서버에 연결하지 못했습니다: %.80s",(char*)ev_data);
    c->is_closing = true;
  } else if (ev == MG_EV_CLOSE) {
    printf("Connection closed\n");
    if(gdata->connection==c) log_session_end(env,gdata->conn==CONNECTED);
    // A connection dropped for its slow path is replaced by server_poll, not reported as closed.
    if(!(server_path_retrying() && gdata->connection==c)) gdata->closed = true;
    if(gdata->connection==c) gdata->connection=NULL;
  }
}
