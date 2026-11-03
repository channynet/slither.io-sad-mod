#include "personal_tag.h"
#include "tag_image.h"
#include "image_library.h"
#include "cosmetic_geometry.h"
#include "tag_attachment.h"
#include "../user.h"
#include "../game/native_preferences.h"
#include "skin_gradient.h"
#include "ntl_tags_ui.h"
#include "../game/ntl_tags.h"
#include <math.h>
#include <external/stb/stb_image_write.h>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#else
#include "../platform/compat.h"
#endif

typedef struct {
  texture* tex;
  VkDescriptorSet descriptor;
  tag_image pending;
  bool enabled,changed,rotate,sway;
  float size,gap,angle;
  const char *image_path,*settings_path;
  image_library library;
  texture* thumbnails[IMAGE_LIBRARY_MAX];
  VkDescriptorSet thumbnail_descriptors[IMAGE_LIBRARY_MAX];
  bool thumbnail_attempted[IMAGE_LIBRARY_MAX],request_thumbnails;
  char message[256];
} personal_image;
static personal_image tag={.enabled=true,.size=128,.gap=24,.image_path="personal-tag.rgba",.settings_path="personal-tag.json"};
static personal_image cosmetic={.enabled=true,.rotate=true,.size=160,.gap=-18,.image_path="personal-cosmetic.rgba",.settings_path="personal-cosmetic.json"};
static tag_attachment tag_motion;
static tag_sway tag_swing,preview_swing;
static void save_settings(personal_image* p) {
  image_item* item=&p->library.items[p->library.selected];
  if(!item->name[0]) strcpy(item->name,"이름 없는 이미지");
  item->size=p->size; item->gap=p->gap; item->angle=p->angle; item->rotate=p->rotate; item->sway=p->sway;
  p->library.enabled=p->enabled;
  if(!image_library_save(&p->library)) snprintf(p->message,sizeof(p->message),"설정을 저장하지 못했습니다. 파일 접근 권한을 확인하세요.");
}
static bool decode_file(const wchar_t* path,tag_image* out) {
  FILE* f=_wfopen(path,L"rb"); if(!f) return false;
  fseek(f,0,SEEK_END); long length=ftell(f); rewind(f);
  unsigned char* bytes=length>0 && length<=16*1024*1024 ? malloc(length) : NULL;
  bool read=bytes && fread(bytes,1,length,f)==(size_t)length; fclose(f);
  bool ok=read && tag_image_decode(bytes,length,out); free(bytes); return ok;
}
static bool load_item(personal_image* p,int index,tag_image* out) {
  char path[256]; image_library_path(&p->library,index,path,sizeof(path));
  if(p->library.items[index].id>=100) return tag_image_load(path,out);
  wchar_t wide[256]; MultiByteToWideChar(CP_UTF8,0,path,-1,wide,256);
  return decode_file(wide,out);
}
static void apply_item(personal_image* p,int index,tag_image next) {
  p->library.selected=index;
  const image_item* item=&p->library.items[index];
  p->size=item->size; p->gap=item->gap; p->angle=item->angle; p->rotate=item->rotate; p->sway=item->sway;
  tag_image_free(&p->pending); p->pending=next; p->changed=true; tag_motion.valid=false; tag_swing.valid=preview_swing.valid=false;
}
static bool select_item(personal_image* p,int index) {
  tag_image next={0};
  if(!load_item(p,index,&next)) {
    snprintf(p->message,sizeof(p->message),"이미지를 읽지 못했습니다. 저장된 이미지 파일을 확인하세요."); return false;
  }
  save_settings(p); apply_item(p,index,next); save_settings(p); return true;
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
  tag_motion.valid=false; tag_swing.valid=preview_swing.valid=false;
  personal_image* slots[]={&tag,&cosmetic};
  for(int i=0;i<2;i++) {
    personal_image* p=slots[i]; image_library_seed(&p->library,i==1);
    int status=image_library_load(&p->library);
    if(status==0) {
      read_settings(p); p->library.enabled=p->enabled;
      tag_image legacy={0};
      if(tag_image_load(p->image_path,&legacy)) {
        image_item* item=image_library_add(&p->library,"기존 이미지");
        item->size=p->size; item->gap=p->gap; item->angle=p->angle; item->rotate=p->rotate; item->sway=i==0;
        char path[256]; image_library_path(&p->library,p->library.count-1,path,sizeof(path));
        if(tag_image_save(path,&legacy)) p->library.selected=p->library.count-1;
        else { p->library.count--; p->library.read_only=true; }
        tag_image_free(&legacy);
      }
      image_library_save(&p->library);
    }
    p->enabled=p->library.enabled;
    tag_image next={0};
    load_item(p,p->library.selected,&next);
    apply_item(p,p->library.selected,next);
    if(status<0 || p->library.read_only) snprintf(p->message,sizeof(p->message),"목록 파일을 읽지 못해 저장을 중지했습니다. 기존 파일은 보존됩니다.");
    else if(!next.rgba) snprintf(p->message,sizeof(p->message),"선택한 이미지 파일을 찾지 못했습니다. 목록에서 다시 선택하세요.");
  }
  personal_tag_prepare(env);
}
static void prepare_thumbnail(tenv* env,personal_image* p,int index) {
  p->thumbnail_attempted[index]=true;
  tag_image source={0}; if(!load_item(p,index,&source)) return;
  unsigned char pixels[64*64*4]={0};
  float scale=64.0f/fmaxf(source.width,source.height);
  int w=(int)(source.width*scale),h=(int)(source.height*scale);
  if(w<1) w=1; if(h<1) h=1;
  for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
    int sx=x*source.width/w,sy=y*source.height/h;
    memcpy(pixels+((y+(64-h)/2)*64+x+(64-w)/2)*4,source.rgba+(sy*source.width+sx)*4,4);
  }
  tag_image_free(&source);
  texture* tex=create_rgba_texture(env->ctx,pixels,64,64); if(!tex) return;
  VkDescriptorSet d=igImplVulkan_AddTexture(env->usr->r->linear_sampler,tex->view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  if(!d) { destroy_texture(env->ctx,tex); return; }
  p->thumbnails[index]=tex; p->thumbnail_descriptors[index]=d;
}
void personal_tag_prepare(tenv* env) {
  personal_image* slots[]={&tag,&cosmetic};
  for(int i=0;i<2;i++) {
    personal_image* p=slots[i];
    if(env->usr->gdata.curr_screen!=PLAYING && p->request_thumbnails) {
      for(int j=0;j<p->library.count;j++) if(!p->thumbnail_attempted[j]) { prepare_thumbnail(env,p,j); break; }
    }
    p->request_thumbnails=false;
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
    if(!p->library.read_only) snprintf(p->message,sizeof(p->message),"내 화면(태그 공유를 켜면 이 클라이언트 사용자에게도) · 아이템별 자동 저장");
  }
}
#ifndef _WIN32
#ifdef __APPLE__
#include "../platform/mac_dialog.h"
static bool pick_file(const char* title,char* out,size_t cap) { return mac_choose_image(title,out,cap); }
#else
// Linux test build: zenity or kdialog as a helper process.
static bool pick_file(const char* title,char* out,size_t cap) {
  char command[1024];
  snprintf(command,sizeof(command),"(zenity --file-selection --title='%s' --file-filter='*.png *.jpg *.jpeg *.bmp' || "
      "kdialog --getopenfilename . '*.png *.jpg *.jpeg *.bmp') 2>/dev/null",title);
  FILE* pipe=popen(command,"r"); if(!pipe) return false;
  bool got=fgets(out,(int)cap,pipe)!=NULL; pclose(pipe);
  if(!got) return false;
  size_t n=strlen(out); while(n && (out[n-1]=='\n' || out[n-1]=='\r')) out[--n]=0;
  return n>0;
}
#endif
#endif
static void choose_image(tenv* env,personal_image* p) {
  wchar_t path[32768]={0};
#ifdef _WIN32
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
#else
  (void)env;
  char chosen[4096];
  if(!pick_file(p==&tag?"내 머리에 다는 태그 이미지 선택":"내 cosmetic 이미지 선택",chosen,sizeof(chosen))) return;
  if(!MultiByteToWideChar(CP_UTF8,0,chosen,-1,path,32768)) return;
  for(wchar_t* c=path;*c;c++) if(*c==L'/') *c=L'\\';      // the name below is cut at the last '\\'
#endif
  tag_image next={0};
  if(!decode_file(path,&next)) { snprintf(p->message,sizeof(p->message),"PNG/JPG/BMP, 16MB 이하, 가로·세로 4096px 이하를 선택하세요"); return; }
  char name[96];
  const wchar_t* base=wcsrchr(path,L'\\'); base=base?base+1:path;
  if(!WideCharToMultiByte(CP_UTF8,0,base,-1,name,sizeof(name),NULL,NULL)) strcpy(name,"추가 이미지");
  char* dot=strrchr(name,'.'); if(dot && dot!=name) *dot=0;
  save_settings(p);
  int previous=p->library.selected;
  image_item* added=image_library_add(&p->library,name);
  if(!added) { tag_image_free(&next); snprintf(p->message,sizeof(p->message),"목록이 가득 찼거나 저장할 수 없습니다 (최대 64개)."); return; }
  char target[256]; image_library_path(&p->library,p->library.count-1,target,sizeof(target));
  p->library.selected=p->library.count-1;
  if(!tag_image_save(target,&next) || !image_library_save(&p->library)) {
    p->library.count--; p->library.selected=previous; tag_image_free(&next);
    snprintf(p->message,sizeof(p->message),"이미지를 목록에 저장하지 못했습니다"); return;
  }
  apply_item(p,p->library.count-1,next); p->enabled=true; save_settings(p);
}
static void controls(tenv* env,personal_image* p) {
  bool cos=p==&cosmetic;
  igPushID_Str(cos?"personal-cosmetic":"personal-tag");
  bool playing=env->usr->gdata.curr_screen==PLAYING;
  igBeginDisabled(playing || p->library.read_only);
  if(igButton("이미지 추가",(ImVec2){0})) choose_image(env,p);
  igSameLine(0,-1);
  if(igButton(cos?"cosmetic 선택":"태그 선택",(ImVec2){0})) igOpenPopup_Str("image-library",0);
  igSetNextWindowSize((ImVec2){390,480},ImGuiCond_Appearing);
  if(igBeginPopup("image-library",0)) {
    p->request_thumbnails=true;
    igText("%s 목록 · %d / %d",cos?"cosmetic":"태그",p->library.count,IMAGE_LIBRARY_MAX);
    igSeparator();
    if(igBeginChild_Str("items",(ImVec2){0,380},0,0)) {
      for(int i=0;i<p->library.count;i++) {
        igPushID_Int(i);
        VkDescriptorSet d=p->thumbnail_descriptors[i];
        bool chosen=false;
        if(d) chosen=igImageButton("image",(ImTextureRef){NULL,(ImTextureID)d},(ImVec2){48,48},(ImVec2){0,0},(ImVec2){1,1},(ImVec4){0,0,0,0},(ImVec4){1,1,1,1});
        else igDummy((ImVec2){56,56});
        igSameLine(0,-1);
        chosen=igSelectable_Bool(p->library.items[i].name,p->library.selected==i,0,(ImVec2){0,56}) || chosen;
        igPopID();
        if(chosen && select_item(p,i)) { igCloseCurrentPopup(); break; }
      }
    }
    igEndChild(); igEndPopup();
  }
  igEndDisabled();
  if(playing) igTextWrapped("이미지 추가와 선택은 게임 메뉴에서 가능합니다.");
  igSeparator();
  igText("선택: %s",p->library.items[p->library.selected].name);
  igBeginDisabled(p->library.read_only);
  igInputText("이름",p->library.items[p->library.selected].name,sizeof(p->library.items[0].name),0,NULL,NULL);
  if(igIsItemDeactivatedAfterEdit()) {
    if(!p->library.items[p->library.selected].name[0]) strcpy(p->library.items[p->library.selected].name,"이름 없는 이미지");
    save_settings(p);
  }
  if(igCheckbox(cos?"cosmetic 표시 · Ctrl+F9":"태그 표시 · Ctrl+F8",&p->enabled)) save_settings(p);
  if(!cos && igCheckbox("부드러운 흔들림",&p->sway)) {
    tag_swing.valid=preview_swing.valid=false; save_settings(p);
  }
  igSliderFloat("크기",&p->size,24,cos?320:192,"%.0f",ImGuiSliderFlags_AlwaysClamp);
  if(igIsItemDeactivatedAfterEdit()) save_settings(p);
  igSliderFloat(cos?"앞뒤 위치":"연결 줄 길이",&p->gap,cos?-100:0,cos?100:120,"%.0f",ImGuiSliderFlags_AlwaysClamp);
  if(igIsItemDeactivatedAfterEdit()) save_settings(p);
  if(cos && igCheckbox("방향에 맞춰 회전",&p->rotate)) save_settings(p);
  igSliderFloat("회전 보정",&p->angle,-180,180,"%.0f°",ImGuiSliderFlags_AlwaysClamp);
  if(igIsItemDeactivatedAfterEdit()) save_settings(p);
  igEndDisabled();
  igTextWrapped("지렁이 크기와 줌에 맞춰 표시됩니다.");
  if(p->tex) {
    float s=128.0f/fmaxf(p->tex->size[0],p->tex->size[1]);
    igImage((ImTextureRef){NULL,(ImTextureID)p->descriptor},(ImVec2){p->tex->size[0]*s,p->tex->size[1]*s},(ImVec2){0},(ImVec2){1,1});
  }
  igTextWrapped("%s",p->message);
  igPopID();
}
void personal_tag_controls(tenv* env) {
  if(igCollapsingHeader_TreeNodeFlags("태그 크기·줄 (내 태그 / 다른 사람)",0)) ntl_tags_look_controls();
  if(igCollapsingHeader_TreeNodeFlags("NTL 태그",0)) ntl_tags_controls(env);
  if(igCollapsingHeader_TreeNodeFlags("내 이미지 태그",ImGuiTreeNodeFlags_DefaultOpen)) {
    igTextWrapped("머리에 연결되어 뒤따라 도는 태그입니다. 일반 이미지는 위쪽 중앙을 연결점으로 사용합니다. 내 화면과, 태그 공유를 켠 이 클라이언트 사용자에게 보입니다."); controls(env,&tag);
  }
  if(igCollapsingHeader_TreeNodeFlags("내 cosmetic",0)) controls(env,&cosmetic);
}
void personal_image_editor(tenv* env) {
  ImFont* font=env->usr->imgui_data.regular_font[FONT_SIZE_SMALL]; igPushFont(font,font->LegacySize);
  igSetNextWindowPos((ImVec2){env->ctx->size[0]-12,100},ImGuiCond_FirstUseEver,(ImVec2){1,0});
  igSetNextWindowSize((ImVec2){380,600},ImGuiCond_FirstUseEver);
  if(igBegin("태그 / cosmetic",NULL,ImGuiWindowFlags_NoDocking)) {
    if(igBeginTabBar("personal-image-tabs",0)) {
      if(igBeginTabItem("그라데이션",NULL,0)) {
        skin_gradient_controls(); igEndTabItem();
      }
      if(igBeginTabItem("불꽃",NULL,0)) {
        fire_skin_controls(); igEndTabItem();
      }
      if(igBeginTabItem("태그",NULL,0)) {
        igSeparatorText("태그 크기·줄 (내 태그 / 다른 사람)");
        ntl_tags_look_controls();
        ntl_tags_controls(env);
        igSeparatorText("내 이미지 태그");
        igTextWrapped("머리에 매다는 이미지 태그 · 변경 내용은 스킨 미리보기에 반영되고 자동 저장됩니다.");
        controls(env,&tag); igEndTabItem();
      }
      if(igBeginTabItem("cosmetic",NULL,0)) {
        controls(env,&cosmetic); igEndTabItem();
      }
      if(igBeginTabItem("얼굴",NULL,0)) {
        igTextWrapped("기본 스킨과 직접 만든 스킨 모두에 적용됩니다. 내 화면에만 보이며 자동 저장됩니다.");
        igCheckbox("웃는 얼굴",&native_prefs.face_smile);
        igCheckbox("집게 모양 눈",&native_prefs.face_claws);
        igTextWrapped("두 가지를 함께 켤 수 있습니다. 모두 끄면 원래 눈으로 돌아갑니다. 장식은 충돌 판정에 영향을 주지 않습니다.");
        igEndTabItem();
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
void personal_tag_draw_image(VkDescriptorSet picture,int tw,int th,float size,float gap,float angle,
    float x,float y,float scale,float direction,float trailing,float swing,float alpha,ntl_tag_look look) {
  float points[8],cord[14];
  alpha*=look.alpha;                                  // (the shared tag look: size, rope, opacity)
  tag_attachment_geometry(points,cord,x,y,size*look.size,tw,th,
      scale,direction,trailing+swing*.35f,gap*look.rope,angle+swing*57.2957795f);
  ImDrawList* draw=igGetWindowDrawList();
  float thickness=fmaxf(1,1.5f*scale)*look.width;
  for(int i=0;i<6 && look.width>0;i++) {
    ImVec2 a={cord[2*i],cord[2*i+1]},b={cord[2*i+2],cord[2*i+3]};
    ImDrawList_AddLine(draw,a,b,igGetColorU32_Vec4((ImVec4){0,0,0,alpha}),thickness+1);
    ImDrawList_AddLine(draw,a,b,igGetColorU32_Vec4((ImVec4){.6f,.6f,.6f,alpha}),thickness);
  }
  ImDrawList_AddImageQuad(draw,(ImTextureRef){NULL,(ImTextureID)picture},
    (ImVec2){points[0],points[1]},(ImVec2){points[2],points[3]},(ImVec2){points[4],points[5]},(ImVec2){points[6],points[7]},
    (ImVec2){0,0},(ImVec2){1,0},(ImVec2){1,1},(ImVec2){0,1},igGetColorU32_Vec4((ImVec4){1,1,1,alpha}));
}
static void draw_tag(float x,float y,float scale,float direction,float trailing,float swing,float alpha) {
  personal_tag_draw_image(tag.descriptor,tag.tex->size[0],tag.tex->size[1],tag.size,tag.gap,tag.angle,
      x,y,scale,direction,trailing,swing,alpha,ntl_tag_look_for(true));
}
// ---- sharing my image tag: its picture as a small PNG (at most 192 px across, 120 KB)
static struct { unsigned char* png; size_t length; char hash[17]; int item; bool tried; } shared={.item=-1};
typedef struct { unsigned char* data; size_t length, cap; } png_buffer;
static void png_out(void* context,void* data,int size) {
  png_buffer* b=context;
  if(b->length+size>b->cap) {
    size_t cap=b->cap?b->cap*2:65536; while(cap<b->length+size) cap*=2;
    unsigned char* grown=realloc(b->data,cap); if(!grown) return;
    b->data=grown; b->cap=cap;
  }
  memcpy(b->data+b->length,data,size); b->length+=size;
}
static int imax(int a,int b) { return a>b?a:b; }
static unsigned char* small_png(const tag_image* src,int most,size_t* length) {
  float k=fminf(1,(float)most/fmaxf(src->width,src->height));
  int w=imax(1,(int)roundf(src->width*k)),h=imax(1,(int)roundf(src->height*k));
  unsigned char* pixels=malloc((size_t)w*h*4); if(!pixels) return NULL;
  for(int y=0;y<h;y++) for(int x=0;x<w;x++) {          // (box filter over the source pixels each covers)
    int x0=x*src->width/w,x1=imax(x0+1,(x+1)*src->width/w),y0=y*src->height/h,y1=imax(y0+1,(y+1)*src->height/h);
    float sum[4]={0},n=0;
    for(int sy=y0;sy<y1;sy++) for(int sx=x0;sx<x1;sx++) {
      const unsigned char* p=src->rgba+(sy*src->width+sx)*4; float a=p[3];
      for(int c=0;c<3;c++) sum[c]+=p[c]*a;
      sum[3]+=a; n++;
    }
    unsigned char* o=pixels+(y*w+x)*4;
    for(int c=0;c<3;c++) o[c]=sum[3]>0?(unsigned char)fminf(255,roundf(sum[c]/sum[3])):0;
    o[3]=(unsigned char)roundf(sum[3]/n);
  }
  png_buffer b={0};
  int ok=stbi_write_png_to_func(png_out,&b,w,h,4,pixels,w*4);
  free(pixels);
  if(!ok) { free(b.data); return NULL; }
  *length=b.length; return b.data;
}
bool personal_tag_share(share_image* out) {
  if(!tag.enabled || !tag.tex || tag.library.selected<0 || tag.library.selected>=tag.library.count) return false;
  if(shared.item!=tag.library.selected) {
    free(shared.png); shared.png=NULL; shared.length=0; shared.item=tag.library.selected; shared.tried=false;
  }
  if(!shared.png && !shared.tried) {
    shared.tried=true;
    tag_image src={0};
    if(load_item(&tag,tag.library.selected,&src)) {
      for(int most=192;most>=48 && !shared.png;most-=48) {
        shared.png=small_png(&src,most,&shared.length);
        if(shared.png && shared.length>120*1024) { free(shared.png); shared.png=NULL; }
      }
      tag_image_free(&src);
      if(shared.png) tag_share_hash(shared.png,shared.length,shared.hash);
    }
  }
  if(!shared.png) return false;
  *out=(share_image){shared.png,shared.length,"",tag.size,tag.gap,tag.angle,tag.sway};
  memcpy(out->hash,shared.hash,17);
  return true;
}
void personal_tag_preview(tenv* env,float x,float y,float scale,float direction,float alpha) {
  (void)env;
  if(tag.enabled && tag.tex && ntl_tag_prefs.mode!=1) {
    float swing=tag.sway?tag_sway_update(&preview_swing,direction,igGetTime()*1000,1):0;
    draw_tag(x,y,scale,direction,direction,swing,alpha);
  } else preview_swing.valid=false;
}
void personal_tag_draw(tenv* env) {
  game_data* game=&env->usr->gdata;
  if(game->curr_screen!=PLAYING || game->conn!=CONNECTED || !game->data.follow_view) { tag_motion.valid=false; tag_swing.valid=false; return; }
  snake* me=get_snake(game,game->data.snake_id); if(!me || me->dead) { tag_motion.valid=false; tag_swing.valid=preview_swing.valid=false; return; }
  float x=env->ctx->size[0]*0.5f+(me->xx+me->fx-game->data.view_xx)*game->data.gsc;
  float y=env->ctx->size[1]*0.5f+(me->yy+me->fy-game->data.view_yy)*game->data.gsc;
  float alpha=fminf(1,fmaxf(0,me->alive_amt));
  personal_cosmetic_preview(env,x,y,me->sc*game->data.gsc,me->ehang,alpha);
  if(!tag.enabled || !tag.tex || ntl_tag_prefs.mode==1 || ntl_tags_hidden) { tag_motion.valid=false; tag_swing.valid=preview_swing.valid=false; return; }   // (an NTL tag chosen instead)
  float trailing=tag_follow(&tag_motion,me->ehang,game->data.ctm,me->generation);
  float swing=tag.sway?tag_sway_update(&tag_swing,me->ehang,game->data.ctm,me->generation):0;
  draw_tag(x,y,me->sc*game->data.gsc,me->ehang,trailing,swing,alpha);
}
void personal_tag_toggle(void) { tag_motion.valid=false; tag_swing.valid=preview_swing.valid=false; tag.enabled=!tag.enabled; save_settings(&tag); }
void personal_cosmetic_toggle(void) { cosmetic.enabled=!cosmetic.enabled; save_settings(&cosmetic); }
void personal_tag_destroy(tenv* env) {
  personal_image* slots[]={&tag,&cosmetic};
  for(int i=0;i<2;i++) {
    personal_image* p=slots[i]; save_settings(p); tag_image_free(&p->pending);
    if(p->descriptor) igImplVulkan_RemoveTexture(p->descriptor);
    if(p->tex) destroy_texture(env->ctx,p->tex);
    p->descriptor=0; p->tex=NULL;
    for(int j=0;j<IMAGE_LIBRARY_MAX;j++) {
      if(p->thumbnail_descriptors[j]) igImplVulkan_RemoveTexture(p->thumbnail_descriptors[j]);
      if(p->thumbnails[j]) destroy_texture(env->ctx,p->thumbnails[j]);
      p->thumbnail_descriptors[j]=0; p->thumbnails[j]=NULL;
    }
  }
}
