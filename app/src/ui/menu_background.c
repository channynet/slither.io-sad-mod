#include "menu_background.h"
#include "menu_background_image.h"
#include "../user.h"
#include "../platform/compat.h"
#include <external/stb/stb_image.h>
#include <errno.h>
#ifdef _WIN32
#include <commdlg.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#elif defined(__APPLE__)
#include "../platform/mac_dialog.h"
#endif

static struct {
  texture* texture;
  VkDescriptorSet descriptor;
  unsigned char* pending;
  int width, height;
  bool change, reset;
  char message[256];
} background;

static void release_texture(tenv* env) {
  if(background.descriptor) igImplVulkan_RemoveTexture(background.descriptor);
  if(background.texture) destroy_texture(env->ctx,background.texture);
  background.descriptor=VK_NULL_HANDLE; background.texture=NULL;
}

void menu_background_prepare(tenv* env) {
  if(!background.change) return;
  background.change=false;
  if(background.reset) {
    vkQueueWaitIdle(env->ctx->queue);
    release_texture(env);
    background.reset=false;
    return;
  }
  texture* next=create_rgba_texture(env->ctx,background.pending,background.width,background.height);
  stbi_image_free(background.pending); background.pending=NULL;
  VkDescriptorSet descriptor=next ? igImplVulkan_AddTexture(env->usr->r->linear_sampler,
      next->view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) : VK_NULL_HANDLE;
  if(!descriptor) {
    if(next) destroy_texture(env->ctx,next);
    snprintf(background.message,sizeof(background.message),"배경 이미지를 표시하지 못했습니다. 더 작은 이미지를 선택하세요.");
    return;
  }
  // Swap before this frame's draw lists, after prior frames finish.
  vkQueueWaitIdle(env->ctx->queue);
  release_texture(env);
  background.texture=next; background.descriptor=descriptor;
}

void menu_background_init(tenv* env) {
  size_t length;
  FILE* file=fopen(BACKGROUND_PATH,"rb");
  if(!file) return;
  unsigned char* bytes=read_image(file,&length,&background.pending,&background.width,&background.height);
  if(!bytes) {
    snprintf(background.message,sizeof(background.message),"저장된 배경을 읽지 못했습니다. 이미지를 다시 선택하세요.");
    return;
  }
  free(bytes); background.change=true;
  menu_background_prepare(env);
}

void menu_background_destroy(tenv* env) {
  release_texture(env);
  stbi_image_free(background.pending);
  memset(&background,0,sizeof(background));
}

void menu_background_draw(tenv* env) {
  if(env->usr->gdata.curr_screen==PLAYING || !background.descriptor) return;
  ImGuiViewport* viewport=igGetMainViewport();
  ImVec2 size=viewport->Size, uv0={0,0}, uv1={1,1};
  if(size.x<=0 || size.y<=0) return;
  float image_ratio=(float)background.texture->size[0]/background.texture->size[1];
  float screen_ratio=size.x/size.y;
  if(image_ratio>screen_ratio) { float crop=screen_ratio/image_ratio; uv0.x=(1-crop)*.5f; uv1.x=1-uv0.x; }
  else { float crop=image_ratio/screen_ratio; uv0.y=(1-crop)*.5f; uv1.y=1-uv0.y; }
  ImDrawList_AddImage(igGetWindowDrawList(),(ImTextureRef){NULL,(ImTextureID)background.descriptor},
      viewport->Pos,(ImVec2){viewport->Pos.x+size.x,viewport->Pos.y+size.y},uv0,uv1,0xffffffff);
}

static void choose_background(tenv* env) {
  FILE* file=NULL;
#ifdef _WIN32
  wchar_t path[32768]={0};
  OPENFILENAMEW dialog={0}; dialog.lStructSize=sizeof(dialog);
  dialog.hwndOwner=glfwGetWin32Window(env->wnd->handle);
  dialog.lpstrFilter=L"Images (PNG, JPG, BMP)\0*.png;*.jpg;*.jpeg;*.bmp\0\0";
  dialog.lpstrFile=path; dialog.nMaxFile=32768;
  dialog.lpstrTitle=L"메뉴 배경 이미지 선택";
  dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_EXPLORER;
  if(!GetOpenFileNameW(&dialog)) {
    if(CommDlgExtendedError()) snprintf(background.message,sizeof(background.message),"파일 선택 창을 열지 못했습니다.");
    return;
  }
  file=_wfopen(path,L"rb");
#elif defined(__APPLE__)
  char path[4096];
  if(!mac_choose_image("메뉴 배경 이미지 선택",path,sizeof(path))) return;
  file=fopen(path,"rb");
#else
  snprintf(background.message,sizeof(background.message),"이 환경에서는 이미지 선택 창을 지원하지 않습니다.");
  return;
#endif
  unsigned char* pixels=NULL; int width=0,height=0; size_t length=0;
  unsigned char* bytes=read_image(file,&length,&pixels,&width,&height);
  if(!bytes) {
    snprintf(background.message,sizeof(background.message),"PNG/JPG/BMP, 16MB 이하, 가로·세로 4096px 이하의 이미지를 선택하세요.");
    return;
  }
  if(!save_image(bytes,length)) {
    free(bytes); stbi_image_free(pixels);
    snprintf(background.message,sizeof(background.message),"배경을 저장하지 못했습니다. 기존 배경을 유지합니다.");
    return;
  }
  free(bytes); stbi_image_free(background.pending);
  background.pending=pixels; background.width=width; background.height=height;
  background.change=true; background.reset=false;
  snprintf(background.message,sizeof(background.message),"배경을 저장했습니다. 다음 실행에도 유지됩니다.");
}

void menu_background_settings(tenv* env) {
  igSeparatorText("메뉴 배경");
  if(igButton("배경 이미지 선택",(ImVec2){0})) choose_background(env);
  igSameLine(0,-1);
  if(igButton("기본 배경 복원",(ImVec2){0})) {
    if(remove(BACKGROUND_PATH)==0 || errno==ENOENT) {
      stbi_image_free(background.pending); background.pending=NULL;
      background.change=true; background.reset=true;
      snprintf(background.message,sizeof(background.message),"기본 배경으로 복원했습니다.");
    } else snprintf(background.message,sizeof(background.message),"배경 파일을 삭제하지 못했습니다. 파일 접근 권한을 확인하세요.");
  }
  igTextWrapped("PNG / JPG / BMP · 최대 16MB, 4096 × 4096px\n비율을 유지해 메뉴 화면을 채웁니다. 가장자리는 일부 잘릴 수 있습니다.");
  if(background.message[0]) igTextWrapped("%s",background.message);
  igSpacing();
}
