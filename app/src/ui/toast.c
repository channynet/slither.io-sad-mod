#include "toast.h"
#include "../user.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <string.h>

typedef struct { char text[256]; float color[3]; double until; } toast;
static toast toasts[4];

void toast_show(const char* text, float r, float g, float b, double seconds) {
  int slot = 0;
  for (int i = 0; i < 4; i++) if (toasts[i].until < toasts[slot].until) slot = i;   // replace the oldest
  snprintf(toasts[slot].text, sizeof(toasts[slot].text), "%s", text);
  toasts[slot].color[0] = r; toasts[slot].color[1] = g; toasts[slot].color[2] = b;
  toasts[slot].until = glfwGetTime() + seconds;
}

void toast_draw(tenv* env) {
  double now = glfwGetTime();
  int order[4], n = 0;
  for (int i = 0; i < 4; i++) if (toasts[i].until > now) order[n++] = i;
  if (!n) return;
  for (int i = 1; i < n; i++)            // newest last (lowest)
    for (int j = i; j > 0 && toasts[order[j]].until < toasts[order[j - 1]].until; j--) { int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t; }
  ImFont* font = env->usr->imgui_data.regular_font[FONT_SIZE_SMALL];
  float size = font->LegacySize, y = 54;
  ImDrawList* draw = igGetForegroundDrawList_ViewportPtr(NULL);
  for (int k = 0; k < n; k++) {
    toast* t = toasts + order[k];
    float fade = (float)(t->until - now); if (fade > 1) fade = 1;
    ImVec2 ts; igPushFont(font, size); igCalcTextSize(&ts, t->text, NULL, false, -1); igPopFont();
    float x = env->ctx->size[0] / 2.0f - ts.x / 2;
    ImDrawList_AddRectFilled(draw, (ImVec2){x - 12, y - 6}, (ImVec2){x + ts.x + 12, y + ts.y + 6},
                             igColorConvertFloat4ToU32((ImVec4){0, 0, 0, .62f * fade}), 8, 0);
    ImDrawList_AddText_FontPtr(draw, font, size, (ImVec2){x, y},
                               igColorConvertFloat4ToU32((ImVec4){t->color[0], t->color[1], t->color[2], fade}), t->text, NULL, 0, NULL);
    y += ts.y + 16;
  }
}
