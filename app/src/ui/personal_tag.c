#include "personal_tag.h"
#include "tag_image.h"
#include "cosmetic_geometry.h"
#include "tag_attachment.h"
#include "../user.h"
#include <windows.h>
#include <commdlg.h>
#include <math.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

typedef struct {
  texture* tex;
  VkDescriptorSet descriptor;
  tag_image pending;
  bool enabled,changed,rotate;
  float size,gap,angle;
  const char *image_path,*settings_path;
  char message[256];
} personal_image;
static personal_image tag={.enabled=true,.size=128,.gap=24,.image_path="personal-tag.rgba",.settings_path="personal-tag.json"};
static personal_image cosmetic={.enabled=true,.rotate=true,.size=160,.gap=-18,.image_path="personal-cosmetic.rgba",.settings_path="personal-cosmetic.json"};
static tag_attachment tag_motion;
static const wchar_t* example_path=L"app/res/personal/black-wings.png";

static void save_settings(personal_image* p) {
  char temp[128]; snprintf(temp,sizeof(temp),"%s.tmp",p->settings_path);
  FILE* f=fopen(temp,"wb"); if(!f) return;
  bool ok=fprintf(f,"{\"version\":%d,\"enabled\":%s,\"size\":%.1f,\"gap\":%.1f,\"rotate\":%s,\"angle\":%.1f}\n",
    p==&tag?2:1,p->enabled?"true":"false",p->size,p->gap,p->rotate?"true":"false",p->angle)>0;
  if(fclose(f)) ok=false;
  if(ok) MoveFileExA(temp,p->settings_path,MOVEFILE_REPLACE_EXISTING);
}
static bool load_file(personal_image* p,const wchar_t* path) {
  FILE* f=_wfopen(path,L"rb");
  if(!f) { snprintf(p->message,sizeof(p->message),"이미지 파일을 열지 못했습니다"); return false; }
  fseek(f,0,SEEK_END); long length=ftell(f); rewind(f);
  unsigned char* bytes=length>0 && length<=16*1024*1024 ? malloc(length) : NULL;
  bool read=bytes && fread(bytes,1,length,f)==(size_t)length; fclose(f);
  tag_image next={0}; bool ok=read && tag_image_decode(bytes,length,&next); free(bytes);
  if(!ok) { snprintf(p->message,sizeof(p->message),"PNG/JPG/BMP, 16MB 이하, 가로·세로 4096px 이하를 선택하세요"); return false; }
  if(!tag_image_save(p->image_path,&next)) { tag_image_free(&next); snprintf(p->message,sizeof(p->message),"이미지를 저장하지 못했습니다"); return false; }
  tag_image_free(&p->pending); p->pending=next; p->changed=true;
  return true;
}
static void read_settings(personal_image* p) {
  FILE* f=fopen(p->settings_path,"rb"); if(!f) return;
  char data[1024]={0}; size_t n=fread(data,1,sizeof(data)-1,f); fclose(f);
  struct mg_str json=mg_str_n(data,n); double value;
  mg_json_get_bool(json,"$.enabled",&p->enabled);
  mg_json_get_bool(json,"$.rotate",&p->rotate);
  bool cos=p==&cosmetic;
  double version=1; mg_json_get_num(json,"$.version",&version);
  bool attached=cos || version>=2;
  if(mg_json_get_num(json,"$.size",&value) && isfinite(value) && value>=24 && value<=(cos?320:192)) p->size=value;
  if(attached && mg_json_get_num(json,"$.gap",&value) && isfinite(value) && value>=(cos?-100:0) && value<=(cos?100:120)) p->gap=value;
  if(attached && mg_json_get_num(json,"$.angle",&value) && isfinite(value) && value>=-180 && value<=180) p->angle=value;
}
void personal_tag_init(tenv* env) {
  tag_motion.valid=false;
  personal_image* slots[]={&tag,&cosmetic};
  for(int i=0;i<2;i++) {
    personal_image* p=slots[i]; read_settings(p);
    if(p==&tag) save_settings(p);
    if(tag_image_load(p->image_path,&p->pending)) p->changed=true;
    else if(load_file(p,example_path)) save_settings(p);
  }
  personal_tag_prepare(env);
}
void personal_tag_prepare(tenv* env) {
  personal_image* slots[]={&tag,&cosmetic};
  for(int i=0;i<2;i++) {
    personal_image* p=slots[i];
    if(!p->changed) continue;
    p->changed=false; if(!p->pending.rgba) continue;
    texture* next=create_rgba_texture(env->ctx,p->pending.rgba,p->pending.width,p->pending.height);
    tag_image_free(&p->pending);
    if(!next) { snprintf(p->message,sizeof(p->message),"이미지를 그래픽 메모리에 올리지 못했습니다"); continue; }
    VkDescriptorSet d=igImplVulkan_AddTexture(env->usr->r->linear_sampler,next->view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if(!d) { destroy_texture(env->ctx,next); snprintf(p->message,sizeof(p->message),"이미지 표시 준비 실패"); continue; }
    vkQueueWaitIdle(env->ctx->queue);
    if(p->descriptor) igImplVulkan_RemoveTexture(p->descriptor);
    if(p->tex) destroy_texture(env->ctx,p->tex);
    p->descriptor=d; p->tex=next;
    snprintf(p->message,sizeof(p->message),"내 화면에만 표시 · 이미지 저장됨");
  }
}
static void choose_image(tenv* env,personal_image* p) {
  wchar_t path[32768]={0};
  OPENFILENAMEW dialog={0}; dialog.lStructSize=sizeof(dialog); dialog.hwndOwner=glfwGetWin32Window(env->wnd->handle);
  dialog.lpstrFilter=L"Images (PNG, JPG, BMP)\0*.png;*.jpg;*.jpeg;*.bmp\0\0";
  dialog.lpstrFile=path; dialog.nMaxFile=32768;
  dialog.lpstrTitle=p==&tag?L"내 머리에 다는 태그 이미지 선택":L"내 cosmetic 이미지 선택";
  dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_EXPLORER;
  if(!GetOpenFileNameW(&dialog)) {
    DWORD error=CommDlgExtendedError();
    if(error) snprintf(p->message,sizeof(p->message),"파일 선택 창을 열지 못했습니다 (오류 %lu)",error);
    return;
  }
  if(load_file(p,path)) { p->enabled=true; save_settings(p); }
}
static void controls(tenv* env,personal_image* p) {
  bool cos=p==&cosmetic;
  igPushID_Str(cos?"personal-cosmetic":"personal-tag");
  bool playing=env->usr->gdata.curr_screen==PLAYING;
  igBeginDisabled(playing);
  if(igButton(p->tex?"이미지 바꾸기":"이미지 추가",(ImVec2){0})) choose_image(env,p);
  igSameLine(0,-1);
  if(igButton("검은 날개 예시",(ImVec2){0}) && load_file(p,example_path)) { p->enabled=true; save_settings(p); }
  igEndDisabled();
  if(playing) igTextWrapped("이미지 파일 선택은 게임 메뉴에서 가능합니다.");
  if(igCheckbox(cos?"cosmetic 표시 · Ctrl+F9":"태그 표시 · Ctrl+F8",&p->enabled)) save_settings(p);
  igSliderFloat("크기",&p->size,24,cos?320:192,"%.0f",ImGuiSliderFlags_AlwaysClamp);
  if(igIsItemDeactivatedAfterEdit()) save_settings(p);
  igSliderFloat(cos?"앞뒤 위치":"연결 줄 길이",&p->gap,cos?-100:0,cos?100:120,"%.0f",ImGuiSliderFlags_AlwaysClamp);
  if(igIsItemDeactivatedAfterEdit()) save_settings(p);
  if(cos && igCheckbox("방향에 맞춰 회전",&p->rotate)) save_settings(p);
  igSliderFloat("회전 보정",&p->angle,-180,180,"%.0f°",ImGuiSliderFlags_AlwaysClamp);
  if(igIsItemDeactivatedAfterEdit()) save_settings(p);
  igTextWrapped("지렁이 크기와 줌에 맞춰 표시됩니다.");
  if(p->tex) {
    float s=128.0f/fmaxf(p->tex->size[0],p->tex->size[1]);
    igImage((ImTextureRef){NULL,(ImTextureID)p->descriptor},(ImVec2){p->tex->size[0]*s,p->tex->size[1]*s},(ImVec2){0},(ImVec2){1,1});
  }
  igTextWrapped("%s",p->message);
  igPopID();
}
void personal_tag_controls(tenv* env) {
  if(igCollapsingHeader_TreeNodeFlags("내 이미지 태그",ImGuiTreeNodeFlags_DefaultOpen)) {
    igTextWrapped("머리에 연결되어 뒤따라 도는 태그입니다. 일반 이미지는 위쪽 중앙을 연결점으로 사용합니다. 내 화면에만 보입니다."); controls(env,&tag);
  }
  if(igCollapsingHeader_TreeNodeFlags("내 cosmetic",0)) controls(env,&cosmetic);
}
void personal_image_editor(tenv* env) {
  ImFont* font=env->usr->imgui_data.regular_font[FONT_SIZE_SMALL]; igPushFont(font,font->LegacySize);
  igSetNextWindowPos((ImVec2){env->ctx->size[0]-12,100},ImGuiCond_FirstUseEver,(ImVec2){1,0});
  igSetNextWindowSize((ImVec2){380,600},ImGuiCond_FirstUseEver);
  if(igBegin("태그 / cosmetic",NULL,ImGuiWindowFlags_NoDocking)) {
    if(igBeginTabBar("personal-image-tabs",0)) {
      if(igBeginTabItem("태그",NULL,0)) {
        igTextWrapped("머리에 매다는 이미지 태그 · 변경 내용은 스킨 미리보기에 반영되고 자동 저장됩니다.");
        controls(env,&tag); igEndTabItem();
      }
      if(igBeginTabItem("cosmetic",NULL,0)) {
        controls(env,&cosmetic); igEndTabItem();
      }
      igEndTabBar();
    }
  }
  igEnd(); igPopFont();
}
void personal_cosmetic_preview(tenv* env,float x,float y,float scale,float direction,float alpha) {
  personal_image* p=&cosmetic;
  if(!p->enabled || !p->tex) return;
  float points[8];
  cosmetic_quad(points,x,y,p->size,p->tex->size[0],p->tex->size[1],scale,direction,p->gap,p->rotate,p->angle);
  ImDrawList_AddImageQuad(igGetWindowDrawList(),(ImTextureRef){NULL,(ImTextureID)p->descriptor},
    (ImVec2){points[0],points[1]},(ImVec2){points[2],points[3]},(ImVec2){points[4],points[5]},(ImVec2){points[6],points[7]},
    (ImVec2){0,0},(ImVec2){1,0},(ImVec2){1,1},(ImVec2){0,1},igGetColorU32_Vec4((ImVec4){1,1,1,alpha}));
}
static void draw_tag(float x,float y,float scale,float direction,float trailing,float alpha) {
  float points[8],cord[14];
  tag_attachment_geometry(points,cord,x,y,tag.size,tag.tex->size[0],tag.tex->size[1],
      scale,direction,trailing,tag.gap,tag.angle);
  ImDrawList* draw=igGetWindowDrawList();
  float thickness=fmaxf(1,1.5f*scale);
  for(int i=0;i<6;i++) {
    ImVec2 a={cord[2*i],cord[2*i+1]},b={cord[2*i+2],cord[2*i+3]};
    ImDrawList_AddLine(draw,a,b,igGetColorU32_Vec4((ImVec4){0,0,0,alpha}),thickness+1);
    ImDrawList_AddLine(draw,a,b,igGetColorU32_Vec4((ImVec4){.6f,.6f,.6f,alpha}),thickness);
  }
  ImDrawList_AddImageQuad(draw,(ImTextureRef){NULL,(ImTextureID)tag.descriptor},
    (ImVec2){points[0],points[1]},(ImVec2){points[2],points[3]},(ImVec2){points[4],points[5]},(ImVec2){points[6],points[7]},
    (ImVec2){0,0},(ImVec2){1,0},(ImVec2){1,1},(ImVec2){0,1},igGetColorU32_Vec4((ImVec4){1,1,1,alpha}));
}
void personal_tag_preview(tenv* env,float x,float y,float scale,float direction,float alpha) {
  (void)env;
  if(tag.enabled && tag.tex) draw_tag(x,y,scale,direction,direction,alpha);
}
void personal_tag_draw(tenv* env) {
  game_data* game=&env->usr->gdata;
  if(game->curr_screen!=PLAYING || game->conn!=CONNECTED || !game->data.follow_view) { tag_motion.valid=false; return; }
  snake* me=get_snake(game,game->data.snake_id); if(!me || me->dead) { tag_motion.valid=false; return; }
  float x=env->ctx->size[0]*0.5f+(me->xx+me->fx-game->data.view_xx)*game->data.gsc;
  float y=env->ctx->size[1]*0.5f+(me->yy+me->fy-game->data.view_yy)*game->data.gsc;
  float alpha=fminf(1,fmaxf(0,me->alive_amt));
  personal_cosmetic_preview(env,x,y,me->sc*game->data.gsc,me->ehang,alpha);
  if(!tag.enabled || !tag.tex) { tag_motion.valid=false; return; }
  float trailing=tag_follow(&tag_motion,me->ehang,game->data.ctm,me->generation);
  draw_tag(x,y,me->sc*game->data.gsc,me->ehang,trailing,alpha);
}
void personal_tag_toggle(void) { tag_motion.valid=false; tag.enabled=!tag.enabled; save_settings(&tag); }
void personal_cosmetic_toggle(void) { cosmetic.enabled=!cosmetic.enabled; save_settings(&cosmetic); }
void personal_tag_destroy(tenv* env) {
  personal_image* slots[]={&tag,&cosmetic};
  for(int i=0;i<2;i++) {
    personal_image* p=slots[i]; save_settings(p); tag_image_free(&p->pending);
    if(p->descriptor) igImplVulkan_RemoveTexture(p->descriptor);
    if(p->tex) destroy_texture(env->ctx,p->tex);
    p->descriptor=0; p->tex=NULL;
  }
}
