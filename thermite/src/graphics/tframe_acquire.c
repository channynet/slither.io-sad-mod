#include "tcontext.h"

bool tcontext_begin(tcontext* context) {
  tcontext_frame* fr = context->frames + context->current_frame;
  VkResult ready=vkWaitForFences(context->device, 1, &fr->wait_fence, VK_TRUE, 1000000);
  if(ready==VK_TIMEOUT) return false;
  if(ready!=VK_SUCCESS) { context->swapchain_ok=false; return false; }

  VkResult r = vkAcquireNextImageKHR(context->device, context->swapchain,
                                     1000000, fr->present_complete,
                                     VK_NULL_HANDLE, &context->current_image);

  if(r==VK_TIMEOUT || r==VK_NOT_READY) return false;
  if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
    context->swapchain_ok = false;
    return false;
  }

  // Do not leave an unsignalled fence behind when acquisition fails.
  vkResetFences(context->device, 1, &fr->wait_fence);
  vkResetCommandBuffer(fr->cmd, 0);
  vkBeginCommandBuffer(fr->cmd,
                       &(VkCommandBufferBeginInfo){
                           .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                           .pNext = NULL,
                           .flags = 0,
                           .pInheritanceInfo = NULL});

  return true;
}
