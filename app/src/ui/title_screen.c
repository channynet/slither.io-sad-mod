#include "title_screen.h"

#include "../network/server.h"
#include "../network/server_history.h"
#include "../network/server_status.h"
#include "../user.h"
#include <external/stb/stb_image.h>

// Immutable frames are uploaded once at startup. Gameplay never decodes or draws them.
#define MENU_MEME_MAX_FRAMES 16
static struct {
  texture* frames[MENU_MEME_MAX_FRAMES];
  VkDescriptorSet descriptors[MENU_MEME_MAX_FRAMES];
  int delays[MENU_MEME_MAX_FRAMES],count,width,height,total_ms;
} menu_meme;

void ui_title_screen_destroy(tenv* env) {
  for(int i=0;i<MENU_MEME_MAX_FRAMES;i++) {
    if(menu_meme.descriptors[i]) igImplVulkan_RemoveTexture(menu_meme.descriptors[i]);
    if(menu_meme.frames[i]) destroy_texture(env->ctx,menu_meme.frames[i]);
  }
  memset(&menu_meme,0,sizeof(menu_meme));
}

void ui_title_screen_init(tenv* env) {
  // Decode the GIF's precomposited PNG frames: preserve its existing transparency.
  // stb's animated GIF background/disposal path leaves a white rectangle here.
  const int count=4;
  for(int i=0;i<count;i++) {
    char path[80]; snprintf(path,sizeof(path),"app/res/menu/cat-knock-%d.png",i);
    int width=0,height=0,components=0;
    unsigned char* pixels=stbi_load(path,&width,&height,&components,4);
    if(!pixels) break;
    bool valid=width>0 && height>0 && width<=512 && height<=512 &&
               (i==0 || (width==menu_meme.width && height==menu_meme.height));
    if(valid) menu_meme.frames[i]=create_rgba_texture(env->ctx,pixels,width,height);
    stbi_image_free(pixels);
    if(!menu_meme.frames[i]) break;
    menu_meme.width=width; menu_meme.height=height;
    menu_meme.descriptors[i]=igImplVulkan_AddTexture(env->usr->r->linear_sampler,
        menu_meme.frames[i]->view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if(!menu_meme.descriptors[i]) break;
    menu_meme.delays[i]=200;
    menu_meme.total_ms+=menu_meme.delays[i];
    menu_meme.count++;
  }
  if(menu_meme.count!=count) ui_title_screen_destroy(env);
}

static void draw_menu_meme(tenv* env) {
  if(!menu_meme.count) return;
  // Anchor to the bottom-right with padding; shrink with small windows.
  float available=env->ctx->size[1]-48;
  if(available<24) return;
  float width=fminf(300.0f,fminf(env->ctx->size[0]*.25f,available*menu_meme.width/menu_meme.height));
  if(width<24) return;
  float height=width*menu_meme.height/menu_meme.width;
  ImVec2 pos={env->ctx->size[0]-24-width,env->ctx->size[1]-24-height},window;
  igGetWindowPos(&window); pos.x+=window.x; pos.y+=window.y;
  // ImGui time does not reset when Play resets the game's GLFW clock.
  double elapsed=fmod(igGetTime()*1000,menu_meme.total_ms);
  int frame=0;
  while(frame<menu_meme.count-1 && elapsed>=menu_meme.delays[frame])
    elapsed-=menu_meme.delays[frame++];
  ImDrawList_AddImage(igGetWindowDrawList(),
      (ImTextureRef){NULL,(ImTextureID)menu_meme.descriptors[frame]},pos,
      (ImVec2){pos.x+width,pos.y+height},(ImVec2){0,0},(ImVec2){1,1},igGetColorU32_Vec4((ImVec4){1,1,1,1}));
}


void ui_title_screen(tenv* env) {
  tuser_data* usr = env->usr;
  tcontext* ctx = env->ctx;
  user_settings* usrs = &usr->usrs;
  ImGuiStyle* style = igGetStyle();
  ImGuiIO* io = igGetIO_Nil();
  game_data* gdata = &usr->gdata;

  // version
  char version_str[16] = {0};
  sprintf(version_str, "v%s", APP_VERSION);
  ImVec2 vtxtsz; igCalcTextSize(&vtxtsz, version_str, NULL, false, -1);
  igSetCursorPosX(ctx->size[0] - vtxtsz.x - style->WindowPadding.x);
  igPushFont(usr->imgui_data.regular_font[FONT_SIZE_SMALL],
             usr->imgui_data.regular_font[FONT_SIZE_SMALL]->LegacySize);
  igTextColored((ImVec4){0.168f, 0.668f, 0.375f, 1}, version_str);
  igPopFont();

  igPushFont(usr->imgui_data.regular_font[usrs->ui_font_size],
             usr->imgui_data.regular_font[usrs->ui_font_size]->LegacySize);

  usr->r->global.bg_opacity = 0;
  usr->r->global.bd_opacity = 0;
  usr->r->global.minimap_opacity = 0;

  float frame_height = igGetFrameHeight();

  float logo_size = 400;
  float logo_gap = 5;

  igPushFont(usr->imgui_data.mono_font[usrs->ui_font_size],
             usr->imgui_data.mono_font[usrs->ui_font_size]->LegacySize);
  igSetCursorPosX(ctx->size[0] / 2.0f - logo_size / 2);
  igSetCursorPosY(ctx->size[1] / 2.0f -
                  (igGetFrameHeight() - style->ItemSpacing.x) * 3);
  draw_menu_meme(env);
  int tot_sec = (int)usrs->play_time;
  int hours = tot_sec / 3600;
  int minutes = (tot_sec % 3600) / 60;
  int seconds = tot_sec % 60;
  igTextColored((ImVec4){1, 1, 1, 0.5f}, "\ue99e");
  igSameLine(0, -1);
  igTextColored((ImVec4){1, 1, 1, 0.5f}, "%d", usrs->score);
  igSetCursorPosX(ctx->size[0] / 2.0f - logo_size / 2);
  igSetCursorPosY(ctx->size[1] / 2.0f -
                  (igGetFrameHeight() - style->ItemSpacing.x) * 2);
  igTextColored((ImVec4){1, 1, 1, 0.5f}, "\ueaeb");
  igSameLine(0, -1);
  igTextColored((ImVec4){1, 1, 1, 0.5f}, "%d", usrs->kills);
  igSetCursorPosX(ctx->size[0] / 2.0f - logo_size / 2);
  igSetCursorPosY(ctx->size[1] / 2.0f -
                  (igGetFrameHeight() - style->ItemSpacing.x));
  igTextColored((ImVec4){1, 1, 1, 0.6}, "\ue952");
  igSameLine(0, -1);
  igTextColored((ImVec4){1, 1, 1, 0.6}, "%02d:%02d:%02d", hours, minutes,
                seconds);
  igPopFont();

  igSetCursorPosX(ctx->size[0] / 2.0f - logo_size / 2);
  igSetCursorPosY(ctx->size[1] / 2.0f + style->ItemSpacing.y);
  igPushItemWidth(logo_size);
  igPushStyleColor_Vec4(ImGuiCol_FrameBg,
                        (ImVec4){0.297f, 0.265f, 0.484f, 1.0f});
  igInputTextWithHint("##nickname_input", "Nickname", usrs->nickname,
                      MAX_NICKNAME_LEN + 1, ImGuiInputTextFlags_None, NULL,
                      NULL);
  igSetCursorPosX(ctx->size[0] / 2.0f - logo_size / 2);
  igSetCursorPosY(ctx->size[1] / 2.0f + style->ItemSpacing.y * 2 +
                  frame_height);
  igSetNextItemWidth(logo_size-frame_height-style->ItemSpacing.x);
  igInputTextWithHint("##ipv4_input", "IPv4:Port", usrs->ipv4, MAX_IPV4_LEN + 1,
                      ImGuiInputTextFlags_None, NULL, NULL);
  igSameLine(0,-1);
  igSetNextItemWidth(frame_height);
  igSetNextWindowSizeConstraints((ImVec2){480,0},(ImVec2){900,650},NULL,NULL);
  if(igBeginCombo("##recent_servers",NULL,ImGuiComboFlags_NoPreview)) {
    igTextDisabled("서버  |  인원 (마지막 접속 기준)  |  핑");
    igSeparator();
    for(int i=0;i<recent_servers.count;i++) {
      const char* address=recent_servers.addresses[i];
      server_status_request(address);
      server_status status; server_status_get(address,&status);
      char population[80],ping[80],label[220];
      long long age=status.players_at>0?(long long)time(NULL)-status.players_at:0;
      if(age<0) age=0;
      if(status.players<0) snprintf(population,sizeof(population),"인원 미확인");
      else if(age<60) snprintf(population,sizeof(population),"%d명 · %lld초 전",status.players,age);
      else if(age<3600) snprintf(population,sizeof(population),"%d명 · %lld분 전",status.players,age/60);
      else snprintf(population,sizeof(population),"%d명 · %lld시간 전",status.players,age/3600);
      if(status.pending) snprintf(ping,sizeof(ping),"측정 중");
      else if(status.failed) snprintf(ping,sizeof(ping),"응답 없음");
      else if(status.ping_ms==0) snprintf(ping,sizeof(ping),"<1 ms");
      else if(status.ping_ms>0) snprintf(ping,sizeof(ping),"%d ms%s",status.ping_ms,status.game_ping?" (게임)":"");
      else snprintf(ping,sizeof(ping),"핑 미확인");
      snprintf(label,sizeof(label),"%s  |  %s  |  %s###server",address,population,ping);
      igPushID_Int(i);
      if(igSelectable_Bool(label,!strcmp(usrs->ipv4,address),0,(ImVec2){0})) strcpy(usrs->ipv4,address);
      if(igIsItemHovered(0)) igSetTooltip("핑: 이 주소의 /ptc 응답 3회 중앙값. 게임 중 핑과 차이 날 수 있습니다.\n인원: 마지막 접속 때 서버가 알려준 수. 플레이하면 갱신됩니다.");
      igPopID();
    }
    if(!recent_servers.count) igTextDisabled("플레이한 서버가 여기에 저장됩니다.");
    igEndCombo();
  }
  if(igIsItemHovered(0)) igSetTooltip("최근 서버 · 인원 · 핑");
  igPopStyleColor(1);
  igPopItemWidth();
  igSetCursorPosX(ctx->size[0] / 2.0f - logo_size / 2);
  igSetCursorPosY(ctx->size[1] / 2.0f + style->ItemSpacing.y * 3 +
                  frame_height * 2);

  if (igButton("\uea1c Play", (ImVec2){logo_size})) {
    usr->gdata.conn = CONNECTING;
    usr->gdata.curr_screen = PLAYING;
    glfwSetTime(0);
    server_connect(env);
  }
  igSetCursorPosX(ctx->size[0] / 2.0f - logo_size / 2);
  igSetCursorPosY(ctx->size[1] / 2.0f + style->ItemSpacing.y * 4 +
                  frame_height * 3);
  if (igButton("\ue90c Skin editor",
               (ImVec2){logo_size / 2 - style->ItemSpacing.x / 2}))
    usr->gdata.curr_screen = SKIN_EDITOR;
  igSameLine(0, -1);
  if (igButton("\ue991 Settings",
               (ImVec2){logo_size / 2 - style->ItemSpacing.x / 2})) {
    usr->gdata.curr_screen = SETTINGS;
  }
  igSetCursorPosX(ctx->size[0] / 2.0f - logo_size / 2);
  igSetCursorPosY(ctx->size[1] / 2.0f + style->ItemSpacing.y * 5 +
                  frame_height * 4);
  if (igButton("\ue9b6 Quit", (ImVec2){logo_size})) {
    env->config.running = false;
    save_user_settings(usrs);
  }
  if(server_history_save_failed) {
    igSetCursorPosX(ctx->size[0]/2.0f-logo_size/2);
    igTextColored((ImVec4){1,0.6f,0.3f,1},"Could not save server history.");
  }

  igPopFont();
}
