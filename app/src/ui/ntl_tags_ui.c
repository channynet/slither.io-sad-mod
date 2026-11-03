#include "ntl_tags_ui.h"
#include "tag_image.h"
#include "../user.h"
#include "../rendering/texture.h"
#include "../game/ntl_tags.h"
#include "../network/ntl.h"
#include "../network/ntl_tag_server.h"
#include "../network/ntl_tags_online.h"
#include "picture_worker.h"
#include "personal_tag.h"
#include "tag_attachment.h"
#include "../network/tag_share.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SETTINGS_PATH "ntl-tags.json"
#define TEXTURES_KEPT 160            // pictures kept on the GPU (the least recently drawn go first)
#define PICTURE_SCALE .285f          // NTL: picture units to world units (x snake scale)

typedef struct { texture* tex; VkDescriptorSet descriptor; int used; bool failed, asked; } picture;
static picture pictures[NTL_TAG_MAX];
static int frame, live, wanted[8], wanted_count;
typedef struct { ntl_tag_rope rope; tag_attachment follow; tag_sway sway; int id; uint64_t generation; int used; bool taken; } rope_slot;
static rope_slot ropes[1024];
// Each snake's own rope, by its id and generation (a dying snake and the new one with its id keep
// separate ropes, so neither is laid out afresh every frame).
static rope_slot* rope_for(const snake* s) {
  unsigned h = ((unsigned)s->id * 2654435761u ^ (unsigned)s->generation * 40503u) & 1023, free_slot = 1024;
  for (unsigned n = 0; n < 1024; n++) {
    rope_slot* r = &ropes[(h + n) & 1023];
    if (r->taken && r->id == s->id && r->generation == s->generation) { r->used = frame; return r; }
    if (free_slot == 1024 && (!r->taken || r->used < frame - 120)) free_slot = (h + n) & 1023;
    if (!r->taken) break;
  }
  if (free_slot == 1024) free_slot = h;
  rope_slot* r = &ropes[free_slot];
  memset(r, 0, sizeof *r); r->id = s->id; r->generation = s->generation; r->used = frame; r->taken = true;
  return r;
}
// Other players' shared image tags (network/tag_share.h): their pictures, by hash.
#define SHARE_PICTURE_KEY 10000
#define SHARE_SLOTS 64
typedef struct { char hash[17]; texture* tex; VkDescriptorSet descriptor; unsigned stamp; int used; bool asked, failed; } share_picture;
static share_picture shared[SHARE_SLOTS];
static unsigned share_stamp;
static void share_picture_ready(tenv* env, picture_result* r);
static void share_pictures_trim(tenv* env);
static bool loaded;
// settings UI
static int pick = -1;                 // a bought tag picked, waiting for its password
static char password[64], message[256];

void ntl_tags_init(tenv* env) {
  (void)env;
  ntl_tags_load_catalog("app/res/ntl-tags/tags.txt");
  ntl_tags_online_start();                             // NTL's online tags: those kept from last time, and newer ones
  ntl_tag_settings_load(SETTINGS_PATH);
  loaded = true;
  ntl_tag_passwords_recheck();
}

static VkDescriptorSet picture_for(int id) {
  if (id < 0 || id >= NTL_TAG_MAX) return 0;
  picture* p = &pictures[id];
  p->used = frame;
  if (p->descriptor || p->failed) return p->descriptor;
  for (int i = 0; i < wanted_count; i++) if (wanted[i] == id) return 0;
  if (wanted_count < 8) wanted[wanted_count++] = id;
  return 0;
}
void ntl_tags_prepare(tenv* env) {
  frame++;
  if (ntl_tags_online_take()) {                       // newer online tags arrived: add them, redo their pictures
    ntl_tags_online_merge();
    bool waited = false;
    for (int i = 0; i < NTL_TAG_MAX; i++) if (ntl_tag_is_online(i)) {
      if (pictures[i].descriptor) {
        if (!waited) { vkQueueWaitIdle(env->ctx->queue); waited = true; }
        igImplVulkan_RemoveTexture(pictures[i].descriptor); destroy_texture(env->ctx, pictures[i].tex); live--;
      }
      pictures[i] = (picture){0};
    }
  }
  // let go of pictures not drawn lately when there are many
  if (live > TEXTURES_KEPT) {
    bool waited = false;
    while (live > TEXTURES_KEPT - 16) {
      int oldest = -1;
      for (int i = 0; i < NTL_TAG_MAX; i++)
        if (pictures[i].descriptor && pictures[i].used < frame - 2 && (oldest < 0 || pictures[i].used < pictures[oldest].used)) oldest = i;
      if (oldest < 0) break;
      if (!waited) { vkQueueWaitIdle(env->ctx->queue); waited = true; }
      igImplVulkan_RemoveTexture(pictures[oldest].descriptor);
      destroy_texture(env->ctx, pictures[oldest].tex);
      pictures[oldest].descriptor = 0; pictures[oldest].tex = NULL; live--;
    }
  }
  share_pictures_trim(env);
  // pictures asked for last frame: read and made on the picture thread (not here: a big one took
  // several ms), at most two textures made per frame from what it finished
  for (int i = 0; i < wanted_count; i++) {
    int id = wanted[i];
    const ntl_tag_def* d = ntl_tag_get(id);
    picture* p = &pictures[id];
    if (!d || p->descriptor || p->failed || p->asked) continue;
    picture_job job = {.key = id, .stamp = (unsigned)ntl_tags_revision(), .kind = PICTURE_NTL_BOX, .box_w = d->w, .box_h = d->h};
    if (ntl_tag_is_online(id)) {
      snprintf(job.path, sizeof job.path, NTL_TAGS_ONLINE_DIR "/t%d.png", id);
      snprintf(job.fallback, sizeof job.fallback, NTL_TAGS_ONLINE_BUNDLED "/t%d.png", id);   // (the copy shipped with the client)
    } else snprintf(job.path, sizeof job.path, "app/res/ntl-tags/t%d.png", id);
    if (picture_worker_request(&job)) p->asked = true;
  }
  wanted_count = 0;
  picture_result r;
  for (int made = 0; made < 2 && picture_worker_take(&r);) {
    if (r.key >= SHARE_PICTURE_KEY) { share_picture_ready(env, &r); made++; continue; }
    if (r.key < 0 || r.key >= NTL_TAG_MAX) { free(r.rgba); continue; }
    picture* p = &pictures[r.key];
    p->asked = false;
    if (r.stamp != (unsigned)ntl_tags_revision() || p->descriptor) { free(r.rgba); continue; }   // (the catalog changed meanwhile: ask again)
    texture* tex = r.rgba ? create_rgba_texture(env->ctx, r.rgba, r.w, r.h) : NULL;
    free(r.rgba); made++;
    VkDescriptorSet d = tex ? igImplVulkan_AddTexture(env->usr->r->linear_sampler, tex->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) : 0;
    if (!d) { if (tex) destroy_texture(env->ctx, tex); p->failed = true; continue; }
    p->tex = tex; p->descriptor = d; live++;
  }
}
void ntl_tags_destroy(tenv* env) {
  ntl_tag_server_destroy();
  tag_share_destroy();
  picture_worker_stop();
  if (loaded) ntl_tag_settings_save(SETTINGS_PATH);
  vkQueueWaitIdle(env->ctx->queue);
  for (int i = 0; i < NTL_TAG_MAX; i++) {
    if (pictures[i].descriptor) igImplVulkan_RemoveTexture(pictures[i].descriptor);
    if (pictures[i].tex) destroy_texture(env->ctx, pictures[i].tex);
    pictures[i] = (picture){0};
  }
  for (int i = 0; i < SHARE_SLOTS; i++) {
    if (shared[i].descriptor) igImplVulkan_RemoveTexture(shared[i].descriptor);
    if (shared[i].tex) destroy_texture(env->ctx, shared[i].tex);
    shared[i] = (share_picture){0};
  }
  live = 0;
}

// The tag a snake shows here (as NTL decides it).
static int tag_of(tenv* env, const snake* s, bool own) {
  if (own) {
    if (ntl_tag_prefs.mode != 1) return -1;
    int t = ntl_tag_own_local();                       // (a bought tag without its password: here only)
    return t >= 0 ? t : ntl_tag_for_official_skin(s->official_skin);
  }
  if (!ntl_tag_prefs.show_others) return -1;
  int team = ntl_teammate_tag(env, s->ntl_id);         // teammates: their team-list tag
  if (team >= -1) return team;
  int t = s->ntl_tag;                                  // a free tag in the skin code
  int bought = ntl_tag_server_lookup(s->ntl_id);       // a bought one, from NTL's tag server
  if (bought >= 0) t = bought;
  if (t < 0) t = ntl_tag_for_official_skin(s->official_skin);
  return t;
}
static ImU32 colour(uint32_t rgb, float alpha) {
  return igGetColorU32_Vec4((ImVec4){((rgb >> 16) & 255) / 255.f, ((rgb >> 8) & 255) / 255.f, (rgb & 255) / 255.f, alpha});
}
static void rope_path(ImDrawList* draw, const ImVec2* p) {
  int e = NTL_TAG_ROPE - 1;
  ImDrawList_PathLineTo(draw, p[e]);
  for (int k = e - 1; k >= 1; k--)
    ImDrawList_PathBezierQuadraticCurveTo(draw, p[k], (ImVec2){(p[k].x + p[k - 1].x) * .5f, (p[k].y + p[k - 1].y) * .5f}, 6);
  ImDrawList_PathLineTo(draw, p[0]);
}
bool ntl_tags_hidden;
// ---- shared image tags
static share_picture* share_picture_for(const char* hash) {
  share_picture* p = NULL;
  for (int i = 0; i < SHARE_SLOTS; i++) if (!strcmp(shared[i].hash, hash)) { p = &shared[i]; break; }
  if (!p) {
    for (int i = 0; i < SHARE_SLOTS && !p; i++) if (!shared[i].hash[0]) p = &shared[i];
    if (!p) return NULL;                              // (all taken: prepare lets old ones go)
    *p = (share_picture){0}; memcpy(p->hash, hash, 17); p->stamp = ++share_stamp;
  }
  p->used = frame;
  if (p->descriptor || p->failed || p->asked) return p;
  picture_job job = {.key = SHARE_PICTURE_KEY + (int)(p - shared), .stamp = p->stamp, .kind = PICTURE_PLAIN};
  if (tag_share_image_path(hash, job.path, sizeof job.path) && picture_worker_request(&job)) p->asked = true;
  return p;
}
static void share_picture_ready(tenv* env, picture_result* r) {
  int i = r->key - SHARE_PICTURE_KEY;
  share_picture* p = i >= 0 && i < SHARE_SLOTS ? &shared[i] : NULL;
  if (!p || p->stamp != r->stamp || p->descriptor) { free(r->rgba); return; }
  p->asked = false;
  texture* tex = r->rgba ? create_rgba_texture(env->ctx, r->rgba, r->w, r->h) : NULL;
  free(r->rgba);
  VkDescriptorSet d = tex ? igImplVulkan_AddTexture(env->usr->r->linear_sampler, tex->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) : 0;
  if (!d) { if (tex) destroy_texture(env->ctx, tex); p->failed = true; return; }
  p->tex = tex; p->descriptor = d;
}
static void share_pictures_trim(tenv* env) {          // (slots not drawn for a while, when most are taken)
  int taken = 0;
  for (int i = 0; i < SHARE_SLOTS; i++) if (shared[i].hash[0]) taken++;
  if (taken < SHARE_SLOTS - 8) return;
  bool waited = false;
  for (int i = 0; i < SHARE_SLOTS; i++) {
    share_picture* p = &shared[i];
    if (!p->hash[0] || p->used > frame - 600 || p->asked) continue;
    if (p->descriptor) {
      if (!waited) { vkQueueWaitIdle(env->ctx->queue); waited = true; }
      igImplVulkan_RemoveTexture(p->descriptor); destroy_texture(env->ctx, p->tex);
    }
    *p = (share_picture){0};
  }
}
static void draw_shared_image(tenv* env, snake* s, const share_entry* e, double now) {
  game_data* g = &env->usr->gdata;
  share_picture* p = share_picture_for(e->hash);
  if (!p || !p->descriptor) return;
  rope_slot* slot = rope_for(s);
  float gsc = g->data.gsc;
  float x = env->ctx->size[0] * .5f + (s->xx + s->fx - g->data.view_xx) * gsc;
  float y = env->ctx->size[1] * .5f + (s->yy + s->fy - g->data.view_yy) * gsc;
  float reach = (e->size + e->gap + 30) * (s->sc > 0 ? s->sc : 1) * gsc * 3;
  if (x < -reach || y < -reach || x > env->ctx->size[0] + reach || y > env->ctx->size[1] + reach) { slot->follow.valid = slot->sway.valid = false; return; }
  float trailing = tag_follow(&slot->follow, s->ehang, now, s->generation);
  float swing = e->sway ? tag_sway_update(&slot->sway, s->ehang, now, s->generation) : 0;
  float alpha = s->alive_amt * (1 - s->dead_amt);
  personal_tag_draw_image(p->descriptor, p->tex->size[0], p->tex->size[1], e->size, e->gap, e->angle,
                          x, y, (s->sc > 0 ? s->sc : 1) * gsc, s->ehang, trailing, swing, alpha * alpha, ntl_tag_look_for(false));
}
void ntl_tags_draw(tenv* env) {
  game_data* g = &env->usr->gdata;
  if (!loaded || ntl_tags_hidden || g->curr_screen != PLAYING || g->conn != CONNECTED || !g->data.follow_view) return;
  ImDrawList* draw = igGetWindowDrawList();
  float cx = env->ctx->size[0] * .5f, cy = env->ctx->size[1] * .5f, gsc = g->data.gsc;
  double now = glfwGetTime() * 1000;
  int count = tdarray_length(g->data.snakes);
  for (int i = 0; i < count; i++) {
    snake* s = g->data.snakes + i;
    if (s->id < 0 || s->alive_amt <= 0) continue;
    bool own = s->id == g->data.snake_id;
    share_entry se;                                    // a player of this client: the tag it shares wins
    bool shared_tag = !own && ntl_tag_prefs.show_others && tag_share_lookup(s->id, &se);
    if (shared_tag && se.kind == SHARE_IMAGE) { draw_shared_image(env, s, &se, now); continue; }
    int tag = shared_tag && se.kind == SHARE_NTL ? se.ntl : tag_of(env, s, own);
    const ntl_tag_def* d = ntl_tag_get(tag);
    if (!d) continue;
    rope_slot* slot = rope_for(s);
    float hx = s->xx + s->fx, hy = s->yy + s->fy, sc = s->sc > 0 ? s->sc : 1;
    float sx = cx + (hx - g->data.view_xx) * gsc, sy = cy + (hy - g->data.view_yy) * gsc;
    ntl_tag_look look = ntl_tag_look_for(own);
    float reach = (60 * look.rope + (d->w > d->h ? d->w : d->h) * PICTURE_SCALE * look.size) * sc * gsc;
    if (sx < -reach || sy < -reach || sx > env->ctx->size[0] + reach || sy > env->ctx->size[1] + reach) { slot->rope.laid = false; continue; }
    ntl_tag_rope_update_len(&slot->rope, hx, hy, s->ang, sc, look.rope, now);
    float alpha = s->alive_amt * (1 - s->dead_amt);
    alpha = alpha * alpha * look.alpha;
    if (alpha <= .01f) continue;
    float rx[NTL_TAG_ROPE], ry[NTL_TAG_ROPE], turn;
    ntl_tag_rope_pose(&slot->rope, now, rx, ry, &turn);  // (part way between its 60 Hz steps)
    ImVec2 p[NTL_TAG_ROPE];
    for (int k = 0; k < NTL_TAG_ROPE; k++) p[k] = (ImVec2){cx + (rx[k] - g->data.view_xx) * gsc, cy + (ry[k] - g->data.view_yy) * gsc};
    float jb = sc * gsc;
    // the rope: dark outline, then its lighter core (NTL: 5 wide, then 4, 3 and 2 at half strength)
    if (look.width > 0) {
      float wj = jb * look.width;
      rope_path(draw, p); ImDrawList_PathStroke(draw, colour(d->c1, alpha), 0, 5 * wj);
      for (int wdt = 4; wdt >= 2; wdt--) { rope_path(draw, p); ImDrawList_PathStroke(draw, colour(d->c2, .5f * alpha), 0, wdt * wj); }
    }
    VkDescriptorSet picture = picture_for(tag);
    if (!picture) continue;
    // the picture at the rope's end, turned with it
    ImVec2 e = p[NTL_TAG_ROPE - 1];
    float k = PICTURE_SCALE * jb * look.size, x0 = d->bx * k, y0 = d->by * k, x1 = x0 + d->w * k, y1 = y0 + d->h * k;
    float c = cosf(turn), sn = sinf(turn);
    #define CORNER(X, Y) (ImVec2){e.x + (X) * c - (Y) * sn, e.y + (X) * sn + (Y) * c}
    ImDrawList_AddImageQuad(draw, (ImTextureRef){NULL, (ImTextureID)picture}, CORNER(x0, y0), CORNER(x1, y0), CORNER(x1, y1), CORNER(x0, y1),
                            (ImVec2){0, 0}, (ImVec2){1, 0}, (ImVec2){1, 1}, (ImVec2){0, 1}, igGetColorU32_Vec4((ImVec4){1, 1, 1, alpha}));
    #undef CORNER
  }
}

// ---- settings
static void save(void) { if (!ntl_tag_settings_save(SETTINGS_PATH)) snprintf(message, sizeof message, "설정을 저장하지 못했습니다"); }
static bool thumbnail(int id, bool selected) {
  const ntl_tag_def* d = ntl_tag_get(id);
  if (!d) return false;
  ImVec2 size = {54, 54};
  char label[24]; snprintf(label, sizeof label, "##ntltag%d", id);
  bool clicked;
  if (igIsRectVisible_Nil((ImVec2){size.x + 8, size.y + 8})) {
    VkDescriptorSet t = picture_for(id);
    if (t) {
      ImVec4 bg = selected ? (ImVec4){.2f, .5f, 1, .6f} : (ImVec4){0, 0, 0, .25f};
      clicked = igImageButton(label, (ImTextureRef){NULL, (ImTextureID)t}, size, (ImVec2){0, 0}, (ImVec2){1, 1}, bg, (ImVec4){1, 1, 1, 1});
    } else {
      char text[24]; snprintf(text, sizeof text, "%d%s", id, label);
      clicked = igButton(text, (ImVec2){size.x + 6, size.y + 6});
    }
  } else clicked = igButton(label, (ImVec2){size.x + 6, size.y + 6});
  if (igIsItemHovered(0)) igSetTooltip("NTL 태그 %d%s", id, ntl_tag_needs_password(id) ? (ntl_tag_auth_for(id) ? " · 비밀번호 확인됨" : " · 비밀번호 없으면 내 화면·이 클라이언트 사용자에게만") : " · 무료");
  return clicked;
}
static void grid(const int* ids, int n) {
  ImVec2 avail; igGetContentRegionAvail(&avail);
  float width = avail.x;
  int per_row = (int)(width / 66); if (per_row < 1) per_row = 1;
  int shown = 0;
  for (int i = 0; i < n; i++) {
    int id = ids[i];
    if (!ntl_tag_get(id)) continue;
    if (shown % per_row) igSameLine(0, 4);
    if (thumbnail(id, ntl_tag_prefs.tag == id || pick == id)) {
      if (!ntl_tag_needs_password(id) || ntl_tag_auth_for(id)) {
        ntl_tag_prefs.tag = id; ntl_tag_prefs.local_only = false; pick = -1; save();
        snprintf(message, sizeof message, "태그 %d 선택 · %s", id,
                 ntl_tag_is_free(id) ? "다른 사람에게는 다음 판(다시 접속)부터 보입니다" : "NTL 태그 서버로 다른 NTL 사용자에게 보입니다");
      } else { pick = id; password[0] = 0; snprintf(message, sizeof message, "태그 %d: NTL에서 받은 비밀번호를 넣고 확인을 누르세요 (없으면 '비밀번호 없이 쓰기')", id); }
    }
    shown++;
  }
}
static bool percent(const char* label, float* value, float lo, float hi) {
  float v = *value * 100;
  igSliderFloat(label, &v, lo * 100, hi * 100, "%.0f%%", ImGuiSliderFlags_AlwaysClamp);
  *value = v / 100;
  return igIsItemDeactivatedAfterEdit();
}
static bool look_sliders(const char* id, ntl_tag_look* l) {
  igPushID_Str(id);
  *l = ntl_tag_look_clamp(*l);
  bool changed = false;
  changed |= percent("태그 크기", &l->size, .25f, 3);
  changed |= percent("줄 길이", &l->rope, .2f, 3);
  changed |= percent("줄 굵기", &l->width, 0, 3);
  changed |= percent("불투명도", &l->alpha, .1f, 1);
  if (igSmallButton("기본값으로")) { *l = NTL_TAG_LOOK_DEFAULT; changed = true; }
  igPopID();
  return changed;
}
void ntl_tags_look_controls(void) {
  igPushID_Str("ntl-tag-look");
  bool changed = false;
  igTextUnformatted("내 태그 (내 이미지 태그·NTL 태그)", NULL);
  changed |= look_sliders("mine", &ntl_tag_prefs.look);
  igSpacing();
  igTextUnformatted("다른 사람 태그 (NTL·공유 태그)", NULL);
  changed |= look_sliders("others", &ntl_tag_prefs.look_other);
  igTextDisabled("따로 저장됩니다 · 내 화면에서만 바뀜");
  if (changed) save();
  igPopID();
}
void ntl_tags_controls(tenv* env) {
  (void)env;
  if (!loaded) return;
  igPushID_Str("ntl-tags");
  igSeparatorText("머리 태그 고르기");
  int mode = ntl_tag_prefs.mode;
  if (igRadioButton_IntPtr("내 이미지 태그", &mode, 0)) { ntl_tag_prefs.mode = 0; save(); }
  igSameLine(0, 12);
  if (igRadioButton_IntPtr("NTL 태그 (다른 사람에게도)", &mode, 1)) { ntl_tag_prefs.mode = 1; save(); }
  if (igCheckbox("다른 사람의 NTL 태그 보기", &ntl_tag_prefs.show_others)) save();
  igTextDisabled("NTL 태그 서버: %s", ntl_tag_server_status());
  igTextDisabled("NTL 온라인 태그: %s", ntl_tags_online_status());
  // players of this client: each other's tags through the relay (tools/tag-share-worker)
  if (igCheckbox("이 클라이언트 사용자끼리 태그 공유", &ntl_tag_prefs.share)) save();
  if (igIsItemHovered(0)) igSetTooltip("내 이미지 태그와 비밀번호 없이 쓰는 태그를 이 클라이언트를 쓰는 다른 사람도 보고,\n그 사람들의 것도 내 화면에 보입니다. 게임 서버 주소·지렁이 번호·태그만 보냅니다(닉네임 없음).");
  igSetNextItemWidth(260);
  igInputTextWithHint("태그 공유 서버", "비우면 기본 공유 서버", ntl_tag_prefs.share_url, sizeof ntl_tag_prefs.share_url, 0, NULL, NULL);
  if (igIsItemDeactivatedAfterEdit()) save();
  igTextDisabled("태그 공유: %s", tag_share_status());
  // a password check's answer
  char answer[256];
  int result = ntl_tag_password_result(answer, sizeof answer);
  if (result == 2) {
    if (pick >= 0) ntl_tag_prefs.tag = pick;
    ntl_tag_prefs.local_only = false;
    snprintf(message, sizeof message, "태그 %d 비밀번호 확인됨 · 착용", ntl_tag_prefs.tag); pick = -1; save();
  } else if (result == 3) snprintf(message, sizeof message, "비밀번호를 받지 않았습니다: %s", answer);
  if (ntl_tag_prefs.mode == 1) {
    int own = ntl_tag_own(), local = ntl_tag_own_local();
    if (own >= 0) igText("착용 중: NTL 태그 %d", own);
    else if (local >= 0) igTextColored((ImVec4){1, .75f, .3f, 1}, "착용 중: NTL 태그 %d (비밀번호 없음 · 내 화면·이 클라이언트 사용자에게만)", local);
    else igText("착용 중: 없음");
    igSameLine(0, 12);
    if (igSmallButton("태그 없음")) { ntl_tag_prefs.tag = -1; ntl_tag_prefs.local_only = false; pick = -1; save(); snprintf(message, sizeof message, "태그를 뺐습니다"); }
    if (pick >= 0) {
      igText("태그 %d 비밀번호", pick);
      igSetNextItemWidth(160);
      igInputText("##ntl-password", password, sizeof password, ImGuiInputTextFlags_Password, NULL, NULL);
      igSameLine(0, 6);
      igBeginDisabled(result == 1 || !password[0]);
      if (igButton(result == 1 ? "확인 중…" : "확인", (ImVec2){0, 0})) { ntl_tag_password_check(pick, password); password[0] = 0; }
      igEndDisabled();
      igSameLine(0, 6);
      if (igSmallButton("취소")) pick = -1;
      if (igSmallButton("비밀번호 없이 쓰기 (내 화면에만)")) igOpenPopup_Str("비밀번호 없이 쓰기", 0);
    }
    // Without the password: warned first, then worn on this screen only (never sent to anyone).
    if (igBeginPopupModal("비밀번호 없이 쓰기", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
      igTextColored((ImVec4){1, .75f, .3f, 1}, "주의: 태그 %d의 비밀번호가 없습니다.", pick);
      igTextUnformatted("비밀번호 없이 쓰면 이 태그는 내 화면과, 태그 공유를 켠 이 클라이언트 사용자에게만 보입니다.", NULL);
      igTextUnformatted("NTL 사용자(팀원 포함)에게는 보내지 않으며, 그들에게는 태그 없이 보입니다.", NULL);
      igTextUnformatted("나중에 비밀번호를 확인하면 그때부터 다른 사람에게도 보입니다.", NULL);
      if (igButton("내 화면에만 쓰기", (ImVec2){0, 0}) && pick >= 0) {
        ntl_tag_prefs.tag = pick; ntl_tag_prefs.local_only = true; save();
        snprintf(message, sizeof message, "태그 %d를 비밀번호 없이 착용 · 내 화면과 이 클라이언트 사용자에게만 보입니다", pick);
        pick = -1; igCloseCurrentPopup();
      }
      igSameLine(0, 8);
      if (igButton("취소##local", (ImVec2){0, 0})) igCloseCurrentPopup();
      igEndPopup();
    }
    if (message[0]) igTextWrapped("%s", message);
    static int free_ids[NTL_TAG_MAX], bought_ids[NTL_TAG_MAX], nf, nb, listed = -1;
    if (listed != ntl_tags_revision()) {
      listed = ntl_tags_revision(); nf = nb = 0;
      for (int i = 0; i < NTL_TAG_MAX; i++) if (ntl_tag_get(i)) { if (ntl_tag_is_free(i)) free_ids[nf++] = i; else if (ntl_tag_needs_password(i)) bought_ids[nb++] = i; }
    }
    if (igCollapsingHeader_TreeNodeFlags("무료 태그 (누구나)", ImGuiTreeNodeFlags_DefaultOpen)) {
      igBeginChild_Str("##free", (ImVec2){0, 220}, ImGuiChildFlags_Borders, 0); grid(free_ids, nf); igEndChild();
    }
    if (igCollapsingHeader_TreeNodeFlags("비밀번호 태그 (NTL에서 받은 태그)", 0)) {
      igTextWrapped("비밀번호가 있으면 다른 사람에게도 보입니다(비밀번호는 저장하지 않고 NTL과 같은 방식의 해시만 저장). 비밀번호가 없으면 경고를 확인한 뒤 내 화면(과 태그 공유를 켠 이 클라이언트 사용자)에만 보이게 쓸 수 있습니다.");
      igBeginChild_Str("##bought", (ImVec2){0, 220}, ImGuiChildFlags_Borders, 0); grid(bought_ids, nb); igEndChild();
    }
  } else if (message[0]) igTextWrapped("%s", message);
  igPopID();
}
