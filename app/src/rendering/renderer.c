#include <stdio.h>
#include <string.h>

#include "../user.h"
#include "../game/perf_trace.h"

void _create_image_data(renderer* r, tcontext* ctx, ivec2 size) {
  glm_ivec2_copy(size, r->size);
  for (int i = 0; i < ctx->fif; i++) {
    vmaCreateImage(
        ctx->allocator,
        &(VkImageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = {.width = size[0], .height = size[1], .depth = 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_SAMPLED_BIT |
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = NULL,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED},
        &(VmaAllocationCreateInfo){
            .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
            .priority = 1.0f},
        &r->images[i].color, &r->images[i].memory, NULL);
    vkCreateImageView(
        ctx->device,
        &(VkImageViewCreateInfo){
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .image = r->images[i].color,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .a = VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .baseMipLevel = 0,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1}},
        NULL, &r->images[i].color_view);

    vkCreateImageView(
        ctx->device,
        &(VkImageViewCreateInfo){
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .image = r->images[i].color,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .components = {.r = VK_COMPONENT_SWIZZLE_R,
                           .g = VK_COMPONENT_SWIZZLE_G,
                           .b = VK_COMPONENT_SWIZZLE_B,
                           .a = VK_COMPONENT_SWIZZLE_ONE},
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .baseMipLevel = 0,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1}},
        NULL, &r->images[i].color_view_alpha);

    vkCreateFramebuffer(ctx->device,
                        &(VkFramebufferCreateInfo){
                            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                            .pNext = NULL,
                            .flags = 0,
                            .renderPass = r->render_pass,
                            .attachmentCount = 1,
                            .pAttachments = &r->images[i].color_view,
                            .width = size[0],
                            .height = size[1],
                            .layers = 1},
                        NULL, &r->images[i].framebuffer);
  }
}

void _destroy_image_data(renderer* r, tcontext* ctx) {
  for (int i = 0; i < ctx->fif; i++) {
    vkDestroyFramebuffer(ctx->device, r->images[i].framebuffer, NULL);
    vkDestroyImageView(ctx->device, r->images[i].color_view_alpha, NULL);
    vkDestroyImageView(ctx->device, r->images[i].color_view, NULL);
    vmaDestroyImage(ctx->allocator, r->images[i].color, r->images[i].memory);
  }
}

renderer* renderer_create(tenv* env) {
  tcontext* ctx = env->ctx;
  tuser_data* usr = env->usr;
  renderer* r = malloc(sizeof(renderer));

  vkCreateSampler(
      ctx->device,
      &(VkSamplerCreateInfo){.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                             .pNext = NULL,
                             .flags = 0,
                             .magFilter = VK_FILTER_LINEAR,
                             .minFilter = VK_FILTER_LINEAR,
                             .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                             .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                             .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                             .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                             .maxLod = VK_LOD_CLAMP_NONE},
      NULL, &r->linear_sampler);

  vkCreateSampler(ctx->device,
                  &(VkSamplerCreateInfo){
                      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                      .pNext = NULL,
                      .flags = 0,
                      .magFilter = VK_FILTER_NEAREST,
                      .minFilter = VK_FILTER_NEAREST,
                      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                      .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
                      .maxLod = VK_LOD_CLAMP_NONE},
                  NULL, &r->nearest_sampler);

  // The gray grid is procedural. Keep a tiny valid descriptor instead of loading
  // an unused 4096x3548 texture and its mip chain (~74 MiB of texels).
  const unsigned char white[]={255,255,255,255};
  r->bg_tex = create_rgba_texture(ctx,white,1,1);
  // The skin atlas at half size (3584x4608): its tiles are 512 px, still larger than a body part
  // ever is on screen, so the GPU never sampled the full-size level anyway. It is 264 MB less
  // memory (texture plus upload buffer) and decodes about 4x faster. The full-size file is used
  // where the half-size one is missing.
  FILE* half = fopen("app/res/textures/tex_atlas_4k.png", "rb");
  if (half) fclose(half);
  r->tex_atlas = create_mipmap_texture(ctx, half ? "app/res/textures/tex_atlas_4k.png" : "app/res/textures/tex_atlas_8k.png");

  vkCreateRenderPass(
      ctx->device,
      &(VkRenderPassCreateInfo){
          .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
          .pNext = NULL,
          .flags = 0,
          .attachmentCount = 1,
          .pAttachments =
              &(VkAttachmentDescription){
                  .flags = 0,
                  .format = VK_FORMAT_R8G8B8A8_UNORM,
                  .samples = VK_SAMPLE_COUNT_1_BIT,
                  .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                  .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                  .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                  .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                  .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                  .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
          .subpassCount = 1,
          .pSubpasses =
              &(VkSubpassDescription){
                  .flags = 0,
                  .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                  .inputAttachmentCount = 0,
                  .pInputAttachments = NULL,
                  .colorAttachmentCount = 1,
                  .pColorAttachments =
                      &(VkAttachmentReference){
                          .attachment = 0,
                          .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}},
          .dependencyCount = 1,
          .pDependencies = &(VkSubpassDependency){
              .srcSubpass=0,.dstSubpass=VK_SUBPASS_EXTERNAL,
              .srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
              .dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
              .srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
              .dstAccessMask=VK_ACCESS_SHADER_READ_BIT}},
      NULL, &r->render_pass);

  r->images = malloc(sizeof(renderer_image_data) * ctx->fif);
  _create_image_data(r, ctx, ctx->size);
  vec2 quad_data[4] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
  r->quad_buffer = tbuffer_create(ctx, quad_data, sizeof(quad_data),
                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

  vkCreateDescriptorSetLayout(
      ctx->device,
      &(VkDescriptorSetLayoutCreateInfo){
          .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
          .pNext = NULL,
          .flags = 0,
          .bindingCount = 4,
          .pBindings =
              (VkDescriptorSetLayoutBinding[]){
                  {.binding = 0,
                   .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                   .descriptorCount = 1,
                   .stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                                 VK_SHADER_STAGE_FRAGMENT_BIT},
                  {.binding = 1,
                   .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   .descriptorCount = 1,
                   .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
                  {.binding = 2,
                   .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   .descriptorCount = 1,
                   .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
                  {.binding = 3,
                   .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   .descriptorCount = 1,
                   .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT}}},
      NULL, &r->global_set_layout);

  vkCreatePipelineLayout(
      ctx->device,
      &(VkPipelineLayoutCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
          .pNext = NULL,
          .flags = 0,
          .setLayoutCount = 1,
          .pSetLayouts = &r->global_set_layout,
          .pushConstantRangeCount = 0,
          .pPushConstantRanges = NULL},
      NULL, &r->pipeline_layout);

  r->mmr = mm_renderer_create(ctx, r->pipeline_layout, r->render_pass);

  r->global_set = malloc(ctx->fif * sizeof(VkDescriptorSet));
  VkDescriptorSetLayout* layouts =
      malloc(ctx->fif * sizeof(VkDescriptorSetLayout));
  for (int i = 0; i < ctx->fif; i++) {
    layouts[i] = r->global_set_layout;
  }

  vkAllocateDescriptorSets(
      ctx->device,
      &(VkDescriptorSetAllocateInfo){
          .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
          .pNext = NULL,
          .descriptorPool = ctx->descriptor_pool,
          .descriptorSetCount = ctx->fif,
          .pSetLayouts = layouts},
      r->global_set);
  free(layouts);

  r->global.viewport[0] = r->size[0];
  r->global.viewport[1] = r->size[1];
  r->global.bg_size[0] = r->bg_tex->size[0];
  r->global.bg_size[1] = r->bg_tex->size[1];
  r->global.bg_color[0] = 1;
  r->global.bg_color[1] = 1;
  r->global.bg_scale = 1;
  r->global.bg_color[2] = 1;
  r->global.bd_color[0] = 1;
  r->global.bd_color[1] = 1;
  r->global.bd_color[2] = 1;
  r->global.bg_opacity = 0;
  r->global.bd_opacity = 0;
  r->global.view[0] = 0;
  r->global.view[1] = 0;
  r->global.zoom = 1;
  r->global.bd_radius = 1;
  r->global.grd = 1;
  r->global.max_minimap_size = MAX_MINIMAP_SIZE;
  r->global.minimap_data_size = 136;
  r->global.minimap_circ[0] = 0;
  r->global.minimap_circ[1] = 0;
  r->global.minimap_circ[2] = 256;
  r->global.minimap_opacity = 1;
  r->global.lview[0] = -1;
  r->global.lview[1] = -1;

  r->global_buffer = tdbuffer_create(ctx, &r->global, sizeof(r->global),
                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

  for (int i = 0; i < ctx->fif; i++)
    vkUpdateDescriptorSets(
        ctx->device, 4,
        (VkWriteDescriptorSet[]){
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = NULL,
                .dstSet = r->global_set[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo =
                    &(VkDescriptorBufferInfo){r->global_buffer[i].handle, 0,
                                              sizeof(r->global)},
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = NULL,
                .dstSet = r->global_set[i],
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo =
                    &(VkDescriptorImageInfo){
                        r->linear_sampler, r->tex_atlas->view,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = NULL,
                .dstSet = r->global_set[i],
                .dstBinding = 2,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo =
                    &(VkDescriptorImageInfo){
                        r->linear_sampler, r->bg_tex->view,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = NULL,
                .dstSet = r->global_set[i],
                .dstBinding = 3,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo =
                    &(VkDescriptorImageInfo){
                        r->nearest_sampler, r->mmr->minimap_tex[i]->view,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            }},
        0, NULL);

  r->bgr = bg_renderer_create(ctx, r->pipeline_layout, r->render_pass);
  r->bstb = bst_renderer_create(ctx, MAX_BOOST_INSTANCES, r->pipeline_layout, r->render_pass);
  r->bpr = bp_renderer_create(ctx, MAX_SPRITE_INSTANCES, r->pipeline_layout, r->render_pass);
  r->bsta = bst_renderer_create(ctx, MAX_BOOST_INSTANCES, r->pipeline_layout, r->render_pass);
  r->fdr = fd_renderer_create(ctx, MAX_FOOD_INSTANCES, MAX_PREYS, r->pipeline_layout, r->render_pass);
  r->bdr = bd_renderer_create(ctx, r->pipeline_layout, r->render_pass);
  r->astr = bp_renderer_create(ctx, 2, r->pipeline_layout, r->render_pass);
  r->cr = spr_renderer_create(ctx, 1, r->pipeline_layout, env->ctx->renderpass);

  // GPU timing for performance.csv (gpu_game_ms). Left off where the queue cannot time.
  r->gpu_queries = VK_NULL_HANDLE; r->gpu_ns_per_tick = 0; r->gpu_ui_ok = false;
  r->gpu_query_written = calloc(ctx->fif, sizeof(bool));
  r->gpu_ui_written = calloc(ctx->fif, sizeof(bool));
  {
    VkPhysicalDeviceProperties props; vkGetPhysicalDeviceProperties(ctx->ph_device, &props);
    uint32_t families = 0; vkGetPhysicalDeviceQueueFamilyProperties(ctx->ph_device, &families, NULL);
    VkQueueFamilyProperties* fam = calloc(families ? families : 1, sizeof(*fam));
    vkGetPhysicalDeviceQueueFamilyProperties(ctx->ph_device, &families, fam);
    bool can = ctx->queue_family < families && fam[ctx->queue_family].timestampValidBits > 0 && props.limits.timestampPeriod > 0;
    free(fam);
    if (can && vkCreateQueryPool(ctx->device, &(VkQueryPoolCreateInfo){.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
          .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = GPU_MARKS * ctx->fif}, NULL, &r->gpu_queries) == VK_SUCCESS)
      r->gpu_ns_per_tick = props.limits.timestampPeriod;
    else r->gpu_queries = VK_NULL_HANDLE;
  }

  return r;
}

void renderer_render(renderer* r, tcontext* ctx, vec4 clear_color) {
  tcontext_frame* fr = ctx->frames + ctx->current_frame;

  // Geometry remains in window coordinates; rasterize into a smaller target.
  r->global.viewport[0] = ctx->size[0];
  r->global.viewport[1] = ctx->size[1];
  memcpy(r->global_buffer[ctx->current_frame].data, &r->global,
         sizeof(r->global));

  // This frame slot's previous timestamps are finished (its fence was waited for): record them.
  int slot = ctx->current_frame;
  // Timestamps between the passes give each one's GPU time (performance.csv gpu_*_ms columns).
  if (r->gpu_queries && r->gpu_query_written[slot]) {
    uint64_t t[GPU_MARKS];
    if (vkGetQueryPoolResults(ctx->device, r->gpu_queries, GPU_MARKS * slot, GPU_MARKS, sizeof(t), t, sizeof(uint64_t),
                              VK_QUERY_RESULT_64_BIT) == VK_SUCCESS) {
      double k = r->gpu_ns_per_tick / 1e6;
      #define SPAN(a, b) (t[b] >= t[a] ? (t[b] - t[a]) * k : 0)
      perf_add_duration(PERF_GPU_GAME, SPAN(GPU_START, GPU_END));
      perf_add_duration(PERF_GPU_BG, SPAN(GPU_START, GPU_BG));
      perf_add_duration(PERF_GPU_FOOD, SPAN(GPU_BG, GPU_FOOD));
      perf_add_duration(PERF_GPU_GLOW, SPAN(GPU_FOOD, GPU_GLOW_UNDER) + SPAN(GPU_BODY, GPU_GLOW_OVER));
      perf_add_duration(PERF_GPU_BODY, SPAN(GPU_GLOW_UNDER, GPU_BODY) + SPAN(GPU_BORDER, GPU_EXTRA));
      perf_add_duration(PERF_GPU_BORDER, SPAN(GPU_GLOW_OVER, GPU_BORDER));
      perf_add_duration(PERF_GPU_MINIMAP, SPAN(GPU_EXTRA, GPU_END));
      if (r->gpu_ui_written[slot]) perf_add_duration(PERF_GPU_UI, SPAN(GPU_UI_START, GPU_UI_END));
      #undef SPAN
    }
    r->gpu_query_written[slot] = false;
  }
  r->gpu_ui_written[slot] = false;
  if (r->gpu_queries) {
    vkCmdResetQueryPool(fr->cmd, r->gpu_queries, GPU_MARKS * slot, GPU_MARKS);
    vkCmdWriteTimestamp(fr->cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, r->gpu_queries, GPU_MARKS * slot + GPU_START);
  }
  #define MARK(i) if (r->gpu_queries) vkCmdWriteTimestamp(fr->cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, r->gpu_queries, GPU_MARKS * slot + (i))

  if(r->global.minimap_opacity>0) {
    double minimap_start=perf_now();
    mm_renderer_prerender(r->mmr, ctx);
    perf_add(PERF_DRAW_MINIMAP,minimap_start);
  }

  vkCmdBeginRenderPass(
      fr->cmd,
      &(VkRenderPassBeginInfo){
          .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
          .pNext = NULL,
          .renderPass = r->render_pass,
          .framebuffer = r->images[ctx->current_frame].framebuffer,
          .renderArea = {.offset = {0, 0}, .extent = {r->size[0], r->size[1]}},
          .clearValueCount = 1,
          .pClearValues =
              &(VkClearValue){
                  .color = {.float32 = {clear_color[0], clear_color[1],
                                        clear_color[2], clear_color[3]}}}},
      VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindVertexBuffers(fr->cmd, 0, 1, &r->quad_buffer->handle,
                         (VkDeviceSize[]){0});

  vkCmdSetViewport(fr->cmd, 0, 1, &(VkViewport){0, 0, r->size[0], r->size[1]});
  vkCmdSetScissor(fr->cmd, 0, 1, &(VkRect2D){{0, 0}, {r->size[0], r->size[1]}});
  vkCmdBindDescriptorSets(fr->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          r->pipeline_layout, 0, 1,
                          &r->global_set[ctx->current_frame], 0, NULL);
  if(r->global.bg_opacity>0) bg_renderer_render(r->bgr, ctx);
  MARK(GPU_BG);
  fd_renderer_render(r->fdr, ctx);
  MARK(GPU_FOOD);
  bst_renderer_render(r->bstb, ctx);
  MARK(GPU_GLOW_UNDER);
  bp_renderer_render(r->bpr, ctx);
  MARK(GPU_BODY);
  bst_renderer_render(r->bsta, ctx);
  MARK(GPU_GLOW_OVER);
  bd_renderer_render(r->bdr, ctx);
  MARK(GPU_BORDER);
  bp_renderer_render(r->astr, ctx);
  MARK(GPU_EXTRA);
  if(r->global.minimap_opacity>0) mm_renderer_render(r->mmr, ctx);
  vkCmdEndRenderPass(ctx->frames[ctx->current_frame].cmd);
  MARK(GPU_END);
  if (r->gpu_queries) { r->gpu_query_written[slot] = true; r->gpu_ui_ok = true; }   // this frame's queries are reset
  #undef MARK
}

// Around the UI pass (the game image scaled to the window, text and windows): its GPU time.
void renderer_gpu_ui(renderer* r, tcontext* ctx, bool end) {
  if (!r->gpu_queries || !r->gpu_ui_ok) return;     // only after this frame's game pass reset the queries
  vkCmdWriteTimestamp(ctx->frames[ctx->current_frame].cmd, end ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      r->gpu_queries, GPU_MARKS * ctx->current_frame + (end ? GPU_UI_END : GPU_UI_START));
  if (end) { r->gpu_ui_written[ctx->current_frame] = true; r->gpu_ui_ok = false; }
}

void renderer_render_cursor(renderer* r, tcontext* ctx) {
  tcontext_frame* fr = ctx->frames + ctx->current_frame;
  vkCmdBindVertexBuffers(fr->cmd, 0, 1, &r->quad_buffer->handle,
                         (VkDeviceSize[]){0});

  vkCmdSetViewport(fr->cmd, 0, 1, &(VkViewport){0, 0, ctx->size[0], ctx->size[1]});
  vkCmdSetScissor(fr->cmd, 0, 1, &(VkRect2D){{0, 0}, {ctx->size[0], ctx->size[1]}});
  vkCmdBindDescriptorSets(fr->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          r->pipeline_layout, 0, 1,
                          &r->global_set[ctx->current_frame], 0, NULL);

  spr_renderer_render(r->cr, ctx);
}

void renderer_destroy(renderer* r, tcontext* ctx) {
  if (r->gpu_queries) vkDestroyQueryPool(ctx->device, r->gpu_queries, NULL);
  free(r->gpu_query_written);
  free(r->gpu_ui_written);
  spr_renderer_destroy(r->cr, ctx);
  bd_renderer_destroy(r->bdr, ctx);
  bp_renderer_destroy(r->astr, ctx);
  bst_renderer_destroy(r->bsta, ctx);
  bp_renderer_destroy(r->bpr, ctx);
  bst_renderer_destroy(r->bstb, ctx);
  fd_renderer_destroy(r->fdr, ctx);
  bg_renderer_destroy(r->bgr, ctx);
  tdbuffer_destroy(ctx, r->global_buffer);
  free(r->global_set);
  mm_renderer_destroy(r->mmr, ctx);
  vkDestroyPipelineLayout(ctx->device, r->pipeline_layout, NULL);
  vkDestroyDescriptorSetLayout(ctx->device, r->global_set_layout, NULL);
  tbuffer_destroy(ctx, r->quad_buffer);
  _destroy_image_data(r, ctx);
  vkDestroyRenderPass(ctx->device, r->render_pass, NULL);
  free(r->images);
  destroy_texture(ctx, r->tex_atlas);
  destroy_texture(ctx, r->bg_tex);
  vkDestroySampler(ctx->device, r->nearest_sampler, NULL);
  vkDestroySampler(ctx->device, r->linear_sampler, NULL);
  free(r);
}

void renderer_resize(renderer* r, tcontext* ctx, ivec2 size) {
  vkQueueWaitIdle(ctx->queue);
  _destroy_image_data(r, ctx);
  _create_image_data(r, ctx, size);
}

void renderer_clear_instances(renderer* r) {
  r->fdr->num_instances = 0;
  r->fdr->num_p_instances = 0;
  r->bsta->num_instances = 0;
  r->bpr->num_instances = 0;
  r->bstb->num_instances = 0;
  r->astr->num_instances = 0;
  r->cr->num_instances = 0;
}
