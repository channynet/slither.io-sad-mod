#include "viewport.h"

#include "../user.h"
#include "../game/death_review.h"

void ui_viewport_init(tenv* env) {
  tuser_data* usr = env->usr;
  tcontext* ctx = env->ctx;

  renderer_resize(usr->r,ctx,(ivec2){GLM_MAX(1,(int)(ctx->size[0]*usr->render_scale)),GLM_MAX(1,(int)(ctx->size[1]*usr->render_scale))});

  usr->viewport_widget.scene = malloc(ctx->fif * sizeof(VkDescriptorSet));
  for (int i = 0; i < ctx->fif; i++)
    usr->viewport_widget.scene[i] = igImplVulkan_AddTexture(
        usr->r->linear_sampler, usr->r->images[i].color_view_alpha,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void ui_viewport(tenv* env) {
  tuser_data* usr = env->usr;
  tcontext* ctx = env->ctx;
  ImGuiStyle* style = igGetStyle();
  ImVec2 start={0},end={env->wnd->size[0],env->wnd->size[1]};
  if(death_review.frozen) {
    float scale=fminf(end.x/usr->r->size[0],end.y/usr->r->size[1]);
    ImVec2 size={usr->r->size[0]*scale,usr->r->size[1]*scale};
    start=(ImVec2){(end.x-size.x)*.5f,(end.y-size.y)*.5f};
    end=(ImVec2){start.x+size.x,start.y+size.y};
  }

  ImDrawList_AddImage(
      igGetBackgroundDrawList(igGetMainViewport()),
      (ImTextureRef){
          NULL, (ImTextureID)usr->viewport_widget.scene[death_review.frozen ? death_review.frame : ctx->current_frame]},
      start, end,
      (ImVec2){0, 0}, (ImVec2){1, 1}, igGetColorU32_Vec4((ImVec4){1, 1, 1, 1}));
}

void ui_viewport_resize(tenv* env) {
  if(death_review.frozen) return;
  death_review.last_frame=-1;
  tuser_data* usr = env->usr;
  tcontext* ctx = env->ctx;
  renderer_resize(usr->r, ctx, (ivec2){GLM_MAX(1,(int)(ctx->size[0]*usr->render_scale)),GLM_MAX(1,(int)(ctx->size[1]*usr->render_scale))});
  for (int i = 0; i < ctx->fif; i++)
    igImplVulkan_RemoveTexture(usr->viewport_widget.scene[i]);
  for (int i = 0; i < ctx->fif; i++)
    usr->viewport_widget.scene[i] = igImplVulkan_AddTexture(
        usr->r->linear_sampler, usr->r->images[i].color_view_alpha,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void ui_viewport_destroy(tenv* env) {
  tuser_data* usr = env->usr;
  free(usr->viewport_widget.scene);
}
