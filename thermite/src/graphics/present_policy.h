#ifndef PRESENT_POLICY_H
#define PRESENT_POLICY_H
#include <stdbool.h>
#include <stdint.h>
#include <vulkan/vulkan.h>
// FIFO is guaranteed. Never fall back to IMMEDIATE or FIFO_RELAXED: both can tear.
static inline VkPresentModeKHR tear_free_present_mode(const VkPresentModeKHR* modes,uint32_t count,bool strict_vsync) {
  if(!strict_vsync) for(uint32_t i=0;i<count;i++) if(modes[i]==VK_PRESENT_MODE_MAILBOX_KHR) return modes[i];
  return VK_PRESENT_MODE_FIFO_KHR;
}
static inline uint32_t swapchain_image_count(uint32_t minimum,uint32_t maximum,VkPresentModeKHR mode) {
  // Keep a spare image: two-image FIFO can fall to half-refresh when a frame
  // misses vblank. Input/network service must not depend on image availability.
  uint32_t count=minimum+1;
  return maximum && count>maximum ? maximum : count;
}
#endif
