#ifndef TCONTEXT_H
#define TCONTEXT_H

#include <vulkan/vulkan.h>

#include "../external/vma/vma.h"
#include "../framework/twindow.h"

typedef struct tcontext_frame {
  VkSemaphore present_complete;
  VkFence wait_fence;
  VkCommandBuffer cmd;
} tcontext_frame;

typedef struct _tcontext_swapchain_frame {
  VkImage image;
  VkImageView image_view;
  VkFramebuffer framebuffer;
} _tcontext_swapchain_frame;

typedef struct tcontext {
  VkInstance instance;
  VkSurfaceKHR surface;
  VkPhysicalDevice ph_device;
  VkDevice device;
  VkQueue queue;
  VkSwapchainKHR old_swapchain;
  VkSwapchainKHR swapchain;
  VkPresentModeKHR present_mode;
  VkDescriptorPool descriptor_pool;

  int fif;
  int current_frame;
  bool swapchain_ok;
  bool frame_submitted;
  double queue_submit_ms,present_ms; // CPU call duration, not GPU execution time.
  VkCommandPool cmd_pool;
  tcontext_frame* frames;
  VkSemaphore* render_completes;
  VkCommandBuffer transfer_cmd;
  VkFence transfer_fence;

  uint32_t image_count;
  uint32_t current_image;
  uint32_t min_image_count;
  _tcontext_swapchain_frame* swapchain_frames;
  VkRenderPass renderpass;

  VmaAllocator allocator;

  uint32_t queue_family;
  VkSurfaceFormatKHR surface_format;
  ivec2 size;

  // Screenshot: set capture_request; the next frame's image is copied on the GPU and, a frame or
  // two later when that frame has finished (never waiting for it), handed over as capture_pixels
  // (capture_w x capture_h x 4 bytes, BGRA if capture_bgra else RGBA, alpha undefined; malloc'd,
  // the caller frees it and clears capture_ready). capture_failed: the image could not be read.
  bool can_capture;
  bool capture_request, capture_ready, capture_failed;
  bool capture_pending, capture_bgra;   // copy recorded, waiting for its frame; pixels are BGRA (caller swaps)
  int capture_frame;
  unsigned char* capture_pixels;
  int capture_w, capture_h;
  VkBuffer capture_buffer;
  VmaAllocation capture_alloc;
  VkDeviceSize capture_size;
} tcontext;

VkShaderModule tcontext_create_shader(tcontext* context, const char* filename);

tcontext* tcontext_create(twindow* window, bool vsync, int fif);
void tcontext_resize(tcontext* context, const ivec2 size, bool vsync);
bool tcontext_begin(tcontext* context);
void tcontext_clear(tcontext* context, const vec4 clear_color);
void tcontext_end(tcontext* context);
void tcontext_wait_idle(tcontext* context);
void tcontext_destroy(tcontext* context);

#endif
