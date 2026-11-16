#include "skin_editor.h"
#include "personal_tag.h"
#include "ntl_tags_ui.h"
#include "head_face.h"
#include "gradient_color.h"
#include "skin_gradient.h"
#include "fire_skin.h"
#include "toast.h"
#include "../game/native_preferences.h"
#include "../game/skin_library.h"

#include <string.h>

#include "../user.h"

int skin_code_filter(ImGuiInputTextCallbackData* data) {
  return !((data->EventChar >= 'a' && data->EventChar <= 'z') ||
           (data->EventChar == '-') || (data->EventChar == ',') ||
           (data->EventChar >= '0' && data->EventChar <= '9'));
}

static skin_library saved_skins;
static int selected_skin=-1;
static char skin_save_status[160];
static skin_design capture_skin(tenv* env) {
  user_settings* u=&env->usr->usrs;skin_design d={0};
  snprintf(d.code,sizeof(d.code),"%s",u->skin_code);d.custom=u->custom_skin;
  d.default_skin=u->default_skin;d.accessory=u->accessory;
  d.fire=native_prefs.fire_skin;d.fire_strength=native_prefs.fire_strength;
  d.gradient=native_prefs.gradient_enabled;d.gradient_count=native_prefs.gradient_count;
  memcpy(d.colors,native_prefs.gradient_colors,sizeof(d.colors));
  d.smile=native_prefs.face_smile;d.claws=native_prefs.face_claws;
  d.tag_mode=ntl_tag_prefs.mode;d.tag_id=ntl_tag_prefs.tag;d.local_tag=ntl_tag_prefs.local_only;
  ntl_tag_look look=ntl_tag_look_for(true);d.look[0]=look.size;d.look[1]=look.rope;d.look[2]=look.width;d.look[3]=look.alpha;
  personal_images_capture(d.images);return d;
}
void ui_skin_editor_init(tenv* env) {
  (void)env;
  if(!skin_library_load(&saved_skins,"saved-skins.json")) snprintf(skin_save_status,sizeof(skin_save_status),"스킨 목록을 읽지 못했습니다. 기존 파일은 보존했습니다.");
}
void ui_skin_remember(tenv* env) {
  skin_design d=capture_skin(env);
  selected_skin=skin_library_remember(&saved_skins,"saved-skins.json",&d);
  snprintf(skin_save_status,sizeof(skin_save_status),selected_skin>=0?"사용한 스킨을 목록에 자동 저장했습니다.":"스킨 목록 저장 실패: 파일 권한 또는 목록 한도(128개)를 확인하세요.");
  if(selected_skin<0) toast_show(skin_save_status,1,.5f,.3f,6);
  save_user_settings(&env->usr->usrs);
  native_preferences_tick(&env->usr->usrs,true);
}
static void saved_skin_controls(tenv* env,float width) {
  char label[80];snprintf(label,sizeof(label),"저장된 스킨 · %d개",saved_skins.count);
  igSetCursorPos((ImVec2){24,16});igSetNextItemWidth(fmaxf(100,width-16));
  if(igBeginCombo("##saved_skins",label,0)) {
    if(!saved_skins.count) igTextDisabled("스킨을 적용하거나 게임에 접속하면 자동 보관됩니다.");
    for(int i=0;i<saved_skins.count;i++) {
      skin_design* d=&saved_skins.items[i];
      snprintf(label,sizeof(label),"스킨 %03d%s%s",i+1,d->fire?" · 불꽃":"",d->gradient?" · 그라데이션":"");
      if(igSelectable_Bool(label,selected_skin==i,0,(ImVec2){0})) {
        if(!personal_images_restore(d->images)) { snprintf(skin_save_status,sizeof(skin_save_status),"보관 스킨의 태그·장식 이미지를 찾지 못했습니다."); continue; }
        user_settings* u=&env->usr->usrs;strcpy(u->skin_code,d->code);u->custom_skin=d->custom;
        u->default_skin=d->default_skin<NUM_DEFAULT_SKINS?d->default_skin:0;
        u->accessory=d->accessory<NUM_ACCESSORIES?d->accessory:NO_ACCESSORY;
        native_prefs.fire_skin=d->fire;native_prefs.fire_strength=d->fire_strength;
        native_prefs.gradient_enabled=d->gradient;native_prefs.gradient_count=d->gradient_count;
        memcpy(native_prefs.gradient_colors,d->colors,sizeof(d->colors));
        native_prefs.face_smile=d->smile;native_prefs.face_claws=d->claws;
        ntl_tag_prefs.mode=d->tag_mode;ntl_tag_prefs.tag=d->tag_id;ntl_tag_prefs.local_only=d->local_tag;
        ntl_tag_prefs.look=ntl_tag_look_clamp((ntl_tag_look){d->look[0],d->look[1],d->look[2],d->look[3]});
        ntl_tag_settings_save("ntl-tags.json");save_user_settings(u);native_preferences_tick(u,true);
        selected_skin=i;snprintf(skin_save_status,sizeof(skin_save_status),"보관한 스킨을 적용했습니다.");
      }
    }
    igEndCombo();
  }
  if(igIsItemHovered(0)) igSetTooltip("%s",skin_save_status[0]?skin_save_status:"스킨·태그·장식·불꽃·그라데이션을 함께 보관합니다.");
}

void ui_skin_editor(tenv* env) {
  skin_effect_time = (float)glfwGetTime();
  tuser_data* usr = env->usr;
  tcontext* ctx = env->ctx;
  ImGuiStyle* style = igGetStyle();
  game_data* gdata = &usr->gdata;
  user_settings* usrs = &usr->usrs;

  igPushFont(usr->imgui_data.regular_font[usrs->ui_font_size],
             usr->imgui_data.regular_font[usrs->ui_font_size]->LegacySize);

  usr->r->global.bg_opacity = 0;
  usr->r->global.bd_opacity = 0;
  usr->r->global.minimap_opacity = 0;

  float frame_height = igGetFrameHeight();
  float scale = fmaxf(16,fminf(48,(ctx->size[1]-frame_height*4-style->ItemSpacing.y*9-200)/8));
  float inspector_width=fminf(380,fmaxf(280,ctx->size[0]*.34f));
  float editor_width=ctx->size[0]-inspector_width-48;
  scale=fminf(scale,fmaxf(16,(editor_width-style->ItemSpacing.x*6-32)/7));
  float center_x = 16+editor_width*.5f;
  float palette_top = 176 + scale*2 + (frame_height+style->ItemSpacing.y)*4;
  static int palette = 0;
  saved_skin_controls(env,editor_width);

  vec2 tot_size = {style->ItemSpacing.x * 6 + 7 * scale,
                   style->ItemSpacing.y * 5 + 6 * scale};
  float controls_width=fminf(editor_width-48,fmaxf(320,tot_size[0]));

  float palette_scale=scale;
  float bounds[4]; personal_preview_bounds(bounds);
  float preview_height=fminf(180,fmaxf(100,ctx->size[1]*.25f-10));
  float preview_factor=fminf((editor_width-48)/(bounds[2]-bounds[0]),preview_height/(bounds[3]-bounds[1]));
  float head_x=16+editor_width*.5f-(bounds[0]+bounds[2])*.5f*preview_factor;
  float head_y=64-bounds[1]*preview_factor;
  int preview_half=128,preview_total=256;
  int code_length=(int)strlen(usrs->skin_code);
  float preview_step=4*preview_factor;
  scale=29*preview_factor;
  float preview_left=head_x-scale*.5f-preview_step*(preview_half-1);
  skin_graphics_begin((float[]){1,1,1},true);
  skin_effect_remote(NULL);
  igSetCursorPos((ImVec2){preview_left,head_y+44*preview_factor-scale*.5f});

  for (int i = strlen(usrs->skin_code); i < MAX_SKIN_CODE_LEN + 1; i++)
    usrs->skin_code[i] = 0;

  for (int i = preview_half - 1; i >= 0; i--) {
    bp_renderer_push(
        usr->r->bpr,
        &(bp_instance){
            {(igGetCursorPosX() + i * preview_step) - 10 * (scale / 48),
             igGetCursorPosY() - 10 * (scale / 48), 68 * (scale / 48)},
            gdata->SHADOW_UV,
            {0, 0, 0, 0.2f}});
  }

  int cbp = 0;
  for (int i = preview_half - 1; i >= 0; i--) {
    int cg_id;
    if (usrs->custom_skin) {
      cg_id =
          get_cg_id(&usr->gdata, usrs->skin_code[code_length ? (preview_total-1-cbp)%code_length : 0]);
    } else {
      int sk_len = gdata->default_skins[usrs->default_skin][0];
      cg_id =
          gdata->default_skins[usrs->default_skin]
                              [1 + ((preview_total - 1 - cbp) % sk_len)];
    }

    if (native_prefs.gradient_enabled || native_prefs.fire_skin || cg_id != -1) {
      if(cg_id<0) cg_id=9;
      float we =
          gdata->worm_effect[(preview_total - 1 - cbp) % WORM_EFFECT_LEN] *
              (!usrs->custom_skin) +
          usrs->custom_skin;
      skin_gradient_body(env,native_prefs.gradient_enabled||native_prefs.fire_skin,0,preview_total-1-cbp,preview_total, &(bp_instance){{igGetCursorPosX() +
                                                        i * preview_step,
                                                    igGetCursorPosY(), scale},
                                                   gdata->cg_uvs[cg_id],
                                                   {we, we, we, 1}});
    }

    cbp++;
  }

  igSetCursorPos((ImVec2){preview_left,head_y-scale*.5f});

  for (int i = 0; i < preview_half; i++) {
    bp_renderer_push(
        usr->r->bpr,
        &(bp_instance){
            {(igGetCursorPosX() + i * preview_step) - 10 * (scale / 48),
             igGetCursorPosY() - 10 * (scale / 48), 68 * (scale / 48)},
            gdata->SHADOW_UV,
            {0, 0, 0, 0.2f}});
  }

  vec2 last_bp_pos;
  int head_cg = 9, head_index = 0;  // the last body part drawn is the head

  for (int i = 0; i < preview_half; i++) {
    int cg_id;
    if (usrs->custom_skin) {
      cg_id =
          get_cg_id(&usr->gdata, usrs->skin_code[code_length ? (preview_total-1-cbp)%code_length : 0]);
    } else {
      int sk_len = gdata->default_skins[usrs->default_skin][0];
      cg_id =
          gdata->default_skins[usrs->default_skin]
                              [1 + ((preview_total - 1 - cbp) % sk_len)];
    }

    last_bp_pos[0] = igGetCursorPosX() + i * preview_step;
    last_bp_pos[1] = igGetCursorPosY();

    head_cg = cg_id < 0 ? 9 : cg_id;
    head_index = preview_total - 1 - cbp;
    if (native_prefs.gradient_enabled || native_prefs.fire_skin || cg_id != -1) {
      if(cg_id<0) cg_id=9;
      float we =
          gdata->worm_effect[(preview_total - 1 - cbp) % WORM_EFFECT_LEN] *
              (!usrs->custom_skin) +
          usrs->custom_skin;
      skin_gradient_body(env,native_prefs.gradient_enabled||native_prefs.fire_skin,0,preview_total-1-cbp,preview_total, &(bp_instance){{last_bp_pos[0],
                                                    last_bp_pos[1], scale, PI},
                                                   gdata->cg_uvs[cg_id],
                                                   {we, we, we, 1}});
    }

    cbp++;
  }

  float ssc = scale / 29;
  float ed = 6 * ssc;   // o->ed
  float esp = 6 * ssc;  // o->esp
  float er = 6;         // o->er
  default_skin_data* dfs =
      gdata->dfs + ((1 - usrs->custom_skin) * (1 + usrs->default_skin));
  float pr = dfs->pr;
  float iris_r = er * ssc;
  float pupil_r = pr * ssc;

  float ex = ed;
  float ey = -esp - .5;

  if(!head_face_available(native_prefs.face_smile,native_prefs.face_claws)) {
    bp_renderer_push(
        usr->r->bpr,
        &(bp_instance){{((last_bp_pos[0] + scale / 2) + ex) - iris_r,
                        ((last_bp_pos[1] + scale / 2) + ey) - iris_r, iris_r * 2},
                       gdata->cg_uvs[BLANK_UV],
                       {dfs->ec.r, dfs->ec.g, dfs->ec.b, 1}});

    ex = ed;
    ey = esp + .5;

    bp_renderer_push(
        usr->r->bpr,
        &(bp_instance){{((last_bp_pos[0] + scale / 2) + ex) - iris_r,
                        ((last_bp_pos[1] + scale / 2) + ey) - iris_r, iris_r * 2},
                       gdata->cg_uvs[BLANK_UV],
                       {dfs->ec.r, dfs->ec.g, dfs->ec.b, 1}});

    ex = ed + .5 + 2 * ssc;
    ey = -esp;

    bp_renderer_push(
        usr->r->bpr,
        &(bp_instance){
            {((last_bp_pos[0] + scale / 2) + ex) - pupil_r,
             ((last_bp_pos[1] + scale / 2) + ey) - pupil_r, pupil_r * 2},
            gdata->cg_uvs[BLANK_UV],
            {dfs->ppc.r, dfs->ppc.g, dfs->ppc.b, 1}});

    ex = (ed + .5) + 2 * ssc;
    ey = esp;

    bp_renderer_push(
        usr->r->bpr,
        &(bp_instance){
            {((last_bp_pos[0] + scale / 2) + ex) - pupil_r,
             ((last_bp_pos[1] + scale / 2) + ey) - pupil_r, pupil_r * 2},
            gdata->cg_uvs[BLANK_UV],
            {dfs->ppc.r, dfs->ppc.g, dfs->ppc.b, 1}});
  }
  float head_rgb[3];
  if(native_prefs.gradient_enabled || native_prefs.fire_skin)
    skin_effect_color(head_rgb,head_index,preview_total);
  else {
    if(head_cg>=NUM_COLOR_GROUPS) head_cg=9;
    head_rgb[0]=gdata->cg_colors[head_cg].r; head_rgb[1]=gdata->cg_colors[head_cg].g; head_rgb[2]=gdata->cg_colors[head_cg].b;
  }
  head_face_draw(last_bp_pos[0]+scale/2,last_bp_pos[1]+scale/2,ssc,1,0,1,
                 native_prefs.face_smile,native_prefs.face_claws,2,0,head_rgb);
  if (usrs->accessory < NUM_ACCESSORIES) {
    accessory_data* acc = gdata->accessories + usrs->accessory;
    ex = acc->of * ed;
    ey = 0;
    float m = scale * 0.5f * acc->sc;
    float acx = (ex + last_bp_pos[0] + scale / 2);
    float acy = (ey + last_bp_pos[1] + scale / 2);

    bp_renderer_push(
        usr->r->bpr,
        &(bp_instance){{acx - m, acy - m, m * 2, 0}, acc->uv, {1, 1, 1, 1}});
  }

  personal_cosmetic_preview(env,last_bp_pos[0]+scale/2,last_bp_pos[1]+scale/2,scale/29,0,1);
  personal_tag_preview(env,last_bp_pos[0]+scale/2,last_bp_pos[1]+scale/2,scale/29,0,1);
  ntl_tags_preview(env,last_bp_pos[0]+scale/2,last_bp_pos[1]+scale/2,scale/29);
  scale=palette_scale;

  igSetCursorPosX(center_x - controls_width * 0.5f);
  igSetCursorPosY((palette_top) -
                  ((style->ItemSpacing.y + frame_height) * 4));
  if (usrs->custom_skin) {
    igSetNextItemWidth(controls_width - 2 * (frame_height + style->ItemSpacing.x));
    igInputTextWithHint("##ss", "Skin code", usrs->skin_code,
                        MAX_SKIN_CODE_LEN + 1,
                        ImGuiInputTextFlags_CallbackCharFilter |
                            ImGuiInputTextFlags_EnterReturnsTrue,
                        skin_code_filter, NULL);
    igSameLine(0, -1);
    igPushStyleVarX(ImGuiStyleVar_FramePadding, 0);
    int skin_code_len = strlen(usrs->skin_code);
    if (igButton("C", (ImVec2){frame_height, frame_height}) &&
        skin_code_len > 0) {
      usrs->skin_code[strlen(usrs->skin_code) - 1] = 0;
    }
    igSameLine(0, -1);
    if (igButton("\ue9ac", (ImVec2){frame_height, frame_height})) {
      usrs->skin_code[0] = 0;
    }
    igPopStyleVar(1);
  } else {
    if (igButton("\uea38",
                 (ImVec2){controls_width / 2 - style->ItemSpacing.x / 2, 0}))
      usrs->default_skin =
          (usrs->default_skin + (NUM_DEFAULT_SKINS - 1)) % NUM_DEFAULT_SKINS;
    igSameLine(0, -1);
    if (igButton("\uea34",
                 (ImVec2){controls_width / 2 - style->ItemSpacing.x / 2, 0}))
      usrs->default_skin = (usrs->default_skin + 1) % NUM_DEFAULT_SKINS;
  }
  igSetCursorPosX(center_x - controls_width * 0.5f);
  igSetCursorPosY((palette_top) -
                  ((style->ItemSpacing.y + frame_height) * 3));
  if (igButton(usrs->custom_skin ? "\uea40 Default" : "\ue905 Custom",
               (ImVec2){controls_width}))
    usrs->custom_skin = !usrs->custom_skin;

  igSetCursorPosX(center_x - controls_width * 0.5f);
  igSetCursorPosY((palette_top) -
                  ((style->ItemSpacing.y + frame_height) * 2));
  if (igButton("OK", (ImVec2){controls_width})) {
    if (usrs->skin_code[0] == 0) usrs->custom_skin = false;
    gdata->curr_screen = TITLE_SCREEN;
    save_user_settings(usrs);
    ui_skin_remember(env);
  }

  if (usrs->custom_skin) {
    igSetCursorPos((ImVec2){center_x-controls_width*.5f,palette_top-frame_height-style->ItemSpacing.y});
    float tab_width=(controls_width-style->ItemSpacing.x)*.5f;
    for(int tab=0;tab<2;tab++) {
      if(tab) igSameLine(0,-1);
      igPushStyleColor_Vec4(ImGuiCol_Button,style->Colors[palette==tab?ImGuiCol_ButtonHovered:ImGuiCol_Button]);
      if(igButton(tab==0?"Colors":"Accessories",(ImVec2){tab_width,0})) palette=tab;
      igPopStyleColor(1);
    }
    igSetCursorPosX(center_x - tot_size[0] * 0.5f);
    igSetCursorPosY(palette_top);
    igPushStyleVar_Vec2(ImGuiStyleVar_FramePadding,(ImVec2){0,0});
    igPushFont(usr->imgui_data.regular_font[FONT_SIZE_SMALL],fminf(20,scale*.48f));
    if(palette==0) {
    float cx = igGetCursorPosX();
    float cy = igGetCursorPosY();

    int skin_code_len = strlen(usrs->skin_code);

    for (int i = 0; i < 6; i++) {
      for (int j = 0; j < 7; j++) {
        igSetCursorPosY(cy + style->ItemSpacing.y * i + scale * i);
        igSetCursorPosX(cx + style->ItemSpacing.x * j + scale * j);
        float ncx = igGetCursorPosX();
        float ncy = igGetCursorPosY();

        bp_renderer_push(usr->r->bpr,
                         &(bp_instance){{ncx - scale * 0.25f,
                                         ncy - scale * 0.25f, scale * 1.5f},
                                        gdata->SHADOW_UV,
                                        {0, 0, 0, 0.2f}});
      }
    }

    igSetCursorPosX(center_x - tot_size[0] * 0.5f);
    igSetCursorPosY(palette_top);
    cx = igGetCursorPosX();
    cy = igGetCursorPosY();

    int cg_id = 0;
    for (int i = 0; i < 6; i++) {
      for (int j = 0; j < 7; j++) {
        bool is_disabled =
            (cg_id == 36 || cg_id == 38 || cg_id == 40 || cg_id == 41 ||
             !(skin_code_len < MAX_SKIN_CODE_LEN));

        igSetCursorPosY(cy + style->ItemSpacing.y * i + scale * i);
        igSetCursorPosX(cx + style->ItemSpacing.x * j + scale * j);
        float ncx = igGetCursorPosX();
        float ncy = igGetCursorPosY();
        bool ct = gdata->cg_colors_ct[cg_id];

        float a = 0.8f * (1.0f - 0.5f * is_disabled);
        igBeginDisabled(is_disabled);
        char label[2] = {gdata->ntl_cg_map[cg_id], 0};
        igPushStyleVar_Float(ImGuiStyleVar_FrameRounding, scale / 2);
        igPushStyleColor_Vec4(ImGuiCol_Text, (ImVec4){ct, ct, ct, 1});
        igPushStyleColor_Vec4(ImGuiCol_Button, (ImVec4){0, 0, 0, 0});
        igPushStyleColor_Vec4(ImGuiCol_Border, (ImVec4){0, 0, 0, 0});
        igPushStyleColor_Vec4(ImGuiCol_BorderShadow, (ImVec4){0, 0, 0, 0});
        igPushStyleColor_Vec4(ImGuiCol_ButtonHovered, (ImVec4){0, 0, 0, 0});
        igPushStyleColor_Vec4(ImGuiCol_ButtonActive, (ImVec4){0, 0, 0, 0});
        igPushID_Int(cg_id);
        bool pressed = igButton(label, (ImVec2){scale, scale});
        igPopID();
        igPopStyleVar(1);
        igPopStyleColor(6);
        igEndDisabled();
        if (pressed) {
          if (strlen(usrs->skin_code) < MAX_SKIN_CODE_LEN) {
            usrs->skin_code[strlen(usrs->skin_code)] = gdata->ntl_cg_map[cg_id];
          }
        }
        bool active = igIsItemActive();
        bool hovered = igIsItemHovered(ImGuiHoveredFlags_None);

        if (hovered) a = 1;
        if (active) a = 0.6f;

        bp_renderer_push(usr->r->bpr, &(bp_instance){{ncx, ncy, scale},
                                                     gdata->cg_uvs[cg_id],
                                                     {1, 1, 1, a}});
        cg_id++;
      }
    }
    } else {
    igSetCursorPosX(center_x - tot_size[0] * 0.5f);
    igSetCursorPosY(palette_top);
    float cx = igGetCursorPosX();
    float cy = igGetCursorPosY();
    int aid = 0;
    for (int i = 0; i < 5; i++) {
      for (int j = 0; j < 7; j++) {
        if (aid > NUM_ACCESSORIES) break;
        bool is_no_accessory = aid == NUM_ACCESSORIES;

        igSetCursorPosY(cy + style->ItemSpacing.y * i + scale * i);
        igSetCursorPosX(cx + style->ItemSpacing.x * j + scale * j);
        float ncx = igGetCursorPosX();
        float ncy = igGetCursorPosY();
        float a = 0.8f;

        char label[16] = {0};
        snprintf(label, sizeof label, "a%d", aid);
        igPushStyleColor_Vec4(ImGuiCol_Text, (ImVec4){1, 0.5f, 0.5f, 1});
        if (!is_no_accessory) {
          igPushStyleVar_Float(ImGuiStyleVar_FrameRounding, scale / 2);
          igPushStyleColor_Vec4(ImGuiCol_Button, (ImVec4){0, 0, 0, 0});
          igPushStyleColor_Vec4(ImGuiCol_Border, (ImVec4){0, 0, 0, 0});
          igPushStyleColor_Vec4(ImGuiCol_BorderShadow, (ImVec4){0, 0, 0, 0});
          igPushStyleColor_Vec4(ImGuiCol_ButtonHovered, (ImVec4){0, 0, 0, 0});
          igPushStyleColor_Vec4(ImGuiCol_ButtonActive, (ImVec4){0, 0, 0, 0});
        }
        igPushID_Str(label);
        bool pressed = igButton(is_no_accessory ? "\uea0f" : "", (ImVec2){scale, scale});
        igPopID();
        if (!is_no_accessory) igPopStyleVar(1);
        igPopStyleColor(is_no_accessory ? 1 : 6);
        if (pressed) {
          usrs->accessory = is_no_accessory ? NO_ACCESSORY : aid;
        }
        bool active = igIsItemActive();
        bool hovered = igIsItemHovered(ImGuiHoveredFlags_None);

        if (hovered) a = 1;
        if (active) a = 0;

        if (!is_no_accessory)
          bp_renderer_push(
              usr->r->bpr,
              &(bp_instance){
                  {ncx, ncy, scale}, gdata->accessories[aid].uv, {1, 1, 1, a}});
        aid++;
      }
    }
    }
    igPopFont();
    igPopStyleVar(1);
  }

  igPopFont();
  personal_image_editor(env);
}

void ui_skin_editor_destroy(tenv* env) {}
