#include "imgui_setup.h"
#include "ui/text_input_ime.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "cimgui/cimgui.h"
#include "cimgui/cimgui_impl.h"

#include "user.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

void imgui_init(tenv* env) {
  tuser_data* usr = env->usr;

  igCreateContext(NULL);
  igImplVulkan_Init(&(ImGui_ImplVulkan_InitInfo){
      .ApiVersion = VK_API_VERSION_1_0,
      .Instance = env->ctx->instance,
      .PhysicalDevice = env->ctx->ph_device,
      .Device = env->ctx->device,
      .QueueFamily = env->ctx->queue_family,
      .Queue = env->ctx->queue,
      .DescriptorPool = env->ctx->descriptor_pool,
      .DescriptorPoolSize = 0,
      .MinImageCount = env->ctx->min_image_count,
      .ImageCount = env->ctx->fif,
      .PipelineCache = NULL,
      .PipelineInfoMain = {.RenderPass = env->ctx->renderpass,
                           .Subpass = 0,
                           .MSAASamples = VK_SAMPLE_COUNT_1_BIT},
      .UseDynamicRendering = false});
  igImplGlfw_InitForVulkan(env->wnd->handle, true);
  text_input_ime_install();
  ImGuiIO* io = igGetIO_Nil();
  char korean_font[260]="";
  bool korean_available = false;
#ifdef _WIN32
  GetWindowsDirectoryA(korean_font,MAX_PATH);
  strcat(korean_font,"/Fonts/malgun.ttf");
  korean_available = _access(korean_font,0)==0;
#else
  // The system's Korean font: Apple SD Gothic Neo on every Mac since 10.11, Noto/Nanum on Linux.
  static const char* candidates[]={
    "/System/Library/Fonts/AppleSDGothicNeo.ttc",
    "/System/Library/Fonts/Supplemental/AppleGothic.ttf",
    "/Library/Fonts/NanumGothic.ttf",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/nanum/NanumGothic.ttf",
  };
  for (size_t i = 0; i < sizeof(candidates)/sizeof(candidates[0]) && !korean_available; i++)
    if (access(candidates[i], R_OK) == 0) { strcpy(korean_font, candidates[i]); korean_available = true; }
#endif
  // io->MouseDrawCursor = true;

  // The Korean font file is read once (ImGui keeps its own copy per size) and freed below.
  void* korean_data = NULL; long korean_size = 0;
  if (korean_available) {
    FILE* f = fopen(korean_font, "rb");
    if (f) {
      fseek(f, 0, SEEK_END); korean_size = ftell(f); rewind(f);
      korean_data = korean_size > 0 && korean_size < 256L * 1024 * 1024 ? malloc(korean_size) : NULL;
      if (korean_data && fread(korean_data, 1, korean_size, f) != (size_t)korean_size) { free(korean_data); korean_data = NULL; }
      fclose(f);
    }
    if (!korean_data) korean_available = false;
  }
  ImFontConfig* korean_config = ImFontConfig_ImFontConfig();
  korean_config->FontDataOwnedByAtlas = false;     // ImGui copies the data for each size

  for (int i = 0; i < NUM_FONT_SIZES; i++) {
    ImFontConfig icons_config = {.FontDataOwnedByAtlas = true,
                                 .OversampleH = 0,
                                 .OversampleV = 0,
                                 .GlyphMaxAdvanceX = FLT_MAX,
                                 .RasterizerDensity = 1,
                                 .RasterizerMultiply = 1,
                                 .EllipsisChar = 0,
                                 .MergeMode = true,
                                 .GlyphOffset = (ImVec2){0, 2 + i},
                                 .GlyphMinAdvanceX = 26.0f + i * 6};

    usr->imgui_data.mono_font[i] = ImFontAtlas_AddFontFromFileTTF(
        io->Fonts, "app/res/fonts/mono_regular.ttf", 20 + i * 4, NULL, NULL);

    ImFontAtlas_AddFontFromFileTTF(
        io->Fonts, "app/res/fonts/iconfont.ttf", 20 + i * 4, &icons_config,
        (const ImWchar[]){0xe900, 0xeaea, 0});

    usr->imgui_data.regular_font[i] = korean_available
        ? ImFontAtlas_AddFontFromMemoryTTF(io->Fonts, korean_data, (int)korean_size, 20 + i * 4, korean_config, NULL)
        : ImFontAtlas_AddFontFromFileTTF(io->Fonts, "app/res/fonts/regular_regular.ttf", 20 + i * 4, NULL, NULL);

    ImFontAtlas_AddFontFromFileTTF(
        io->Fonts, "app/res/fonts/iconfont.ttf", 20 + i * 4, &icons_config,
        (const ImWchar[]){0xe900, 0xeaea, 0});

    usr->imgui_data.mono_font_bold[i] = ImFontAtlas_AddFontFromFileTTF(
        io->Fonts, "app/res/fonts/mono_bold.ttf", 20 + i * 4, NULL, NULL);

    ImFontAtlas_AddFontFromFileTTF(io->Fonts, "app/res/fonts/iconfont.ttf",
                                   20 + i * 4, &icons_config,
                                   (const ImWchar[]){0xe900, 0xeaea, 0});

    usr->imgui_data.regular_font_bold[i] = ImFontAtlas_AddFontFromFileTTF(
        io->Fonts, "app/res/fonts/regular_bold.ttf", 20 + i * 4, NULL, NULL);

    ImFontAtlas_AddFontFromFileTTF(io->Fonts, "app/res/fonts/iconfont.ttf",
                                   20 + i * 4, &icons_config,
                                   (const ImWchar[]){0xe900, 0xeaea, 0});
  }
  ImFontConfig_destroy(korean_config);
  free(korean_data);

  io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io->IniFilename = "native-layout.ini";

  ImGuiStyle* style = igGetStyle();
  style->DockingNodeHasCloseButton = false;
  style->WindowMenuButtonPosition = ImGuiDir_None;
  style->TabCloseButtonMinWidthUnselected = -1;
  style->WindowBorderSize = style->FrameBorderSize = style->ChildBorderSize =
      style->PopupBorderSize = style->TabBorderSize = 1;
  style->FramePadding = (ImVec2){8, 8};
  style->ItemSpacing = (ImVec2){4, 4};
  style->ItemInnerSpacing = (ImVec2){4, 4};
  style->WindowPadding = (ImVec2){4, 4};
  style->GrabMinSize = 18;
  style->FrameRounding = style->TabRounding = style->ChildRounding =
      style->GrabRounding = style->PopupRounding = style->ScrollbarRounding =
          style->WindowRounding = style->TreeLinesRounding = 3;
  style->ScrollbarSize = 10;
  style->DockingSeparatorSize = 1;
  style->ScrollbarPadding = 1;
  style->CellPadding.x = 2;

  igStyleColorsDark(style);
  style->Colors[ImGuiCol_Text] = (ImVec4){0.89f, 0.89f, 0.89f, 1.00f};
  style->Colors[ImGuiCol_WindowBg] = style->Colors[ImGuiCol_PopupBg] =
      (ImVec4){0.20f, 0.20f, 0.20f, 1.00f};
  style->Colors[ImGuiCol_Border] = (ImVec4){0.00f, 0.00f, 0.00f, 1.00f};
  style->Colors[ImGuiCol_BorderShadow] = (ImVec4){1.00f, 1.00f, 1.00f, 0.18f};
  style->Colors[ImGuiCol_FrameBg] = (ImVec4){0.16f, 0.16f, 0.16f, 1.00f};
  style->Colors[ImGuiCol_TitleBgActive] = (ImVec4){0.12f, 0.12f, 0.12f, 1.00f};
  style->Colors[ImGuiCol_Button] = (ImVec4){0.25f, 0.25f, 0.25f, 1.00f};
  style->Colors[ImGuiCol_ButtonHovered] = (ImVec4){0.31f, 0.31f, 0.31f, 1.00f};
  style->Colors[ImGuiCol_Header] = (ImVec4){0.25f, 0.25f, 0.25f, 1.00f};
  style->Colors[ImGuiCol_HeaderHovered] = (ImVec4){0.31f, 0.31f, 0.31f, 1.00f};
  style->Colors[ImGuiCol_TabHovered] = (ImVec4){0.31f, 0.31f, 0.31f, 1.00f};
  style->Colors[ImGuiCol_Tab] = (ImVec4){0.25f, 0.25f, 0.25f, 1.00f};
  style->Colors[ImGuiCol_TabSelected] = (ImVec4){0.31f, 0.31f, 0.31f, 1.00f};
  style->Colors[ImGuiCol_TabSelectedOverline] =
      (ImVec4){0.26f, 0.59f, 0.98f, 0.00f};
  style->Colors[ImGuiCol_TabDimmed] = (ImVec4){0.17f, 0.17f, 0.17f, 1.00f};
  style->Colors[ImGuiCol_TabDimmedSelected] =
      (ImVec4){0.23f, 0.23f, 0.23f, 1.00f};
  style->Colors[ImGuiCol_ModalWindowDimBg] = (ImVec4){0, 0, 0, 0.7f};
  style->Colors[ImGuiCol_FrameBgHovered] = (ImVec4){0.23f, 0.23f, 0.23f, 1.00f};
  style->Colors[ImGuiCol_FrameBgActive] = (ImVec4){0.12f, 0.12f, 0.12f, 1.00f};
  style->Colors[ImGuiCol_SliderGrab] = (ImVec4){0.31f, 0.31f, 0.31f, 1.00f};
  style->Colors[ImGuiCol_SliderGrabActive] =
      (ImVec4){0.31f, 0.31f, 0.31f, 1.00f};
  style->Colors[ImGuiCol_ButtonActive] = (ImVec4){0.14f, 0.14f, 0.14f, 1.00f};
  style->Colors[ImGuiCol_HeaderActive] = (ImVec4){0.14f, 0.14f, 0.14f, 1.00f};
}

void imgui_prerender() {
  igImplVulkan_NewFrame();
  igImplGlfw_NewFrame();
  igNewFrame();
}

void imgui_render(VkCommandBuffer cmd) {
  igImplVulkan_RenderDrawData(igGetDrawData(), cmd, NULL);
}

void imgui_destroy() {
  text_input_ime_restore();
  igImplGlfw_Shutdown();
  igImplVulkan_Shutdown();
  igDestroyContext(NULL);
}
