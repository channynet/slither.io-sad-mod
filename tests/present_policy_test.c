#include "../thermite/src/graphics/present_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  VkPresentModeKHR full[]={VK_PRESENT_MODE_IMMEDIATE_KHR,VK_PRESENT_MODE_FIFO_RELAXED_KHR,VK_PRESENT_MODE_FIFO_KHR,VK_PRESENT_MODE_MAILBOX_KHR};
  assert(tear_free_present_mode(full,4,false)==VK_PRESENT_MODE_MAILBOX_KHR);
  assert(tear_free_present_mode(full,4,true)==VK_PRESENT_MODE_FIFO_KHR);
  assert(tear_free_present_mode(full,3,false)==VK_PRESENT_MODE_FIFO_KHR);
  assert(tear_free_present_mode(NULL,0,false)==VK_PRESENT_MODE_FIFO_KHR);
  assert(swapchain_image_count(2,2,VK_PRESENT_MODE_MAILBOX_KHR)==2);
  assert(swapchain_image_count(2,0,VK_PRESENT_MODE_MAILBOX_KHR)==3);
  assert(swapchain_image_count(2,4,VK_PRESENT_MODE_MAILBOX_KHR)==3);
  assert(swapchain_image_count(2,0,VK_PRESENT_MODE_FIFO_KHR)==3);
  assert(swapchain_image_count(3,4,VK_PRESENT_MODE_FIFO_KHR)==4);
  puts("Presentation: tear-free selection, FIFO fallback, strict sync and image count limits passed");
}
