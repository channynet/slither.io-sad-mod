#include "../app/src/imgui_setup.c"
#include <assert.h>

// The production font loader is tested with the real ImGui rasterizer.
// No renderer/window backend may run in this CPU-only executable.
bool igImplVulkan_Init(ImGui_ImplVulkan_InitInfo* info) { abort(); }
bool igImplGlfw_InitForVulkan(GLFWwindow* window, bool callbacks) { abort(); }
void igImplVulkan_NewFrame(void) { abort(); }
void igImplGlfw_NewFrame(void) { abort(); }
void igImplVulkan_RenderDrawData(ImDrawData* data, VkCommandBuffer cmd, VkPipeline pipeline) { abort(); }
void igImplVulkan_Shutdown(void) { abort(); }
void igImplGlfw_Shutdown(void) { abort(); }
void text_input_ime_install(void) { abort(); }
void text_input_ime_restore(void) { abort(); }

int main(void) {
  tuser_data* usr = calloc(1, sizeof(*usr));
  assert(usr);
  igCreateContext(NULL);
  ImGuiIO* io = igGetIO_Nil();
  io->IniFilename = NULL;
  io->BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
  io->DisplaySize = (ImVec2){960, 720};
  io->DeltaTime = 1.0f / 60;
  imgui_load_fonts(usr);
  igNewFrame();
  for (int i = 0; i < NUM_FONT_SIZES; i++) {
    ImFont* fonts[] = {usr->imgui_data.regular_font[i], usr->imgui_data.regular_font_bold[i],
                      usr->imgui_data.mono_font[i], usr->imgui_data.mono_font_bold[i]};
    for (int j = 0; j < 4; j++) {
      ImFont* font = fonts[j];
      igPushFont(font, font->LegacySize);
      ImFontBaked* baked = igGetFontBaked();
      const ImWchar characters[] = {0xac00, 0xd55c, 0xae00, 0xc11c, 0xbc84, 'A', '9', 0xeaed};
      for (size_t k = 0; k < sizeof(characters) / sizeof(*characters); k++) {
        ImFontGlyph* glyph = ImFontBaked_FindGlyphNoFallback(baked, characters[k]);
        if (!glyph) fprintf(stderr, "Missing U+%04x in font %d size %d\n", characters[k], j, i);
        assert(glyph && glyph->Visible && glyph->X1 > glyph->X0);
      }
      igTextUnformatted("한글 서버 ABC 123", NULL);
      igPopFont();
    }
  }
  igRender();
  igDestroyContext(NULL);
  free(usr);
  puts("PASS real font rasterization: Korean, English, digits and icons in all four styles and sizes");
}
