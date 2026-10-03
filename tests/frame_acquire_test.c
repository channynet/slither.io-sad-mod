#include "../thermite/src/graphics/tcontext.h"
#include <assert.h>
#include <stdio.h>
static VkResult wait_result,acquire_result;
static int acquired,resets,commands,begins;
VKAPI_ATTR VkResult VKAPI_CALL vkWaitForFences(VkDevice d,uint32_t n,const VkFence* f,VkBool32 all,uint64_t timeout) {
  assert(timeout>0 && timeout<=1000000); return wait_result;
}
VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImageKHR(VkDevice d,VkSwapchainKHR s,uint64_t timeout,VkSemaphore sem,VkFence f,uint32_t* image) {
  assert(timeout>0 && timeout<=1000000); acquired++; *image=1; return acquire_result;
}
VKAPI_ATTR VkResult VKAPI_CALL vkResetFences(VkDevice d,uint32_t n,const VkFence* f) { resets++; return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL vkResetCommandBuffer(VkCommandBuffer c,VkCommandBufferResetFlags f) { commands++; return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer c,const VkCommandBufferBeginInfo* i) { begins++; return VK_SUCCESS; }
int main(void) {
  tcontext_frame frame={0}; tcontext ctx={.frames=&frame,.swapchain_ok=true};
  wait_result=VK_TIMEOUT;
  for(int i=0;i<5;i++) assert(!tcontext_begin(&ctx));
  assert(ctx.swapchain_ok && !acquired && !resets && !commands);
  wait_result=VK_SUCCESS;
  acquire_result=VK_TIMEOUT; assert(!tcontext_begin(&ctx));
  acquire_result=VK_NOT_READY; assert(!tcontext_begin(&ctx));
  assert(ctx.swapchain_ok && !resets && !commands);
  acquire_result=VK_SUCCESS; assert(tcontext_begin(&ctx));
  assert(resets==1 && commands==1 && begins==1 && ctx.current_image==1);
  acquire_result=VK_ERROR_OUT_OF_DATE_KHR; assert(!tcontext_begin(&ctx));
  assert(!ctx.swapchain_ok && resets==1);
  ctx.swapchain_ok=true; acquire_result=VK_SUBOPTIMAL_KHR;
  assert(tcontext_begin(&ctx)); assert(resets==2 && begins==2);
  wait_result=VK_ERROR_DEVICE_LOST; assert(!tcontext_begin(&ctx));
  assert(!ctx.swapchain_ok && resets==2);
  puts("Frame acquisition: bounded waits, repeated timeout recovery, no premature fence reset, resize/device errors passed");
}
