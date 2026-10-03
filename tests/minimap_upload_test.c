#include "../app/src/rendering/mm_renderer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static int copies,barriers;
// This test exercises only the upload path; creation/drawing must never run.
tdbuffer* tdbuffer_create(tcontext* a,const void* b,VkDeviceSize c,VkBufferUsageFlags d) { assert(0); return NULL; }
void tdbuffer_destroy(tcontext* a,tdbuffer* b) { assert(0); }
texture* create_minimap_texture(tcontext* a,int b) { assert(0); return NULL; }
void destroy_texture(tcontext* a,texture* b) { assert(0); }
VkShaderModule tcontext_create_shader(tcontext* a,const char* b) { assert(0); return VK_NULL_HANDLE; }
VKAPI_ATTR VkResult VKAPI_CALL vkCreateGraphicsPipelines(VkDevice a,VkPipelineCache b,uint32_t c,const VkGraphicsPipelineCreateInfo* d,const VkAllocationCallbacks* e,VkPipeline* f) { assert(0); return VK_ERROR_UNKNOWN; }
VKAPI_ATTR void VKAPI_CALL vkDestroyShaderModule(VkDevice a,VkShaderModule b,const VkAllocationCallbacks* c) { assert(0); }
VKAPI_ATTR void VKAPI_CALL vkDestroyPipeline(VkDevice a,VkPipeline b,const VkAllocationCallbacks* c) { assert(0); }
VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer a,VkPipelineBindPoint b,VkPipeline c) { assert(0); }
VKAPI_ATTR void VKAPI_CALL vkCmdDraw(VkCommandBuffer a,uint32_t b,uint32_t c,uint32_t d,uint32_t e) { assert(0); }
VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier(VkCommandBuffer a,VkPipelineStageFlags b,VkPipelineStageFlags c,VkDependencyFlags d,uint32_t e,const VkMemoryBarrier* f,uint32_t g,const VkBufferMemoryBarrier* h,uint32_t i,const VkImageMemoryBarrier* j) { barriers++; }
VKAPI_ATTR void VKAPI_CALL vkCmdCopyBufferToImage(VkCommandBuffer a,VkBuffer b,VkImage c,VkImageLayout d,uint32_t e,const VkBufferImageCopy* f) { copies++; }
VkResult vmaFlushAllocation(VmaAllocator a,VmaAllocation b,VkDeviceSize c,VkDeviceSize d) { return VK_SUCCESS; }
int main(void) {
  mm_renderer r={0}; tcontext ctx={0}; tcontext_frame frames[2]={0}; ctx.frames=frames;
  bool uploaded[2]={false,false}; r.uploaded=uploaded;
  texture images[2]={0}; texture* textures[]={images,images+1}; r.minimap_tex=textures;
  tdbuffer buffers[2]={0}; r.minimap_buffer=buffers;
  for(int i=0;i<2;i++) buffers[i].data=calloc(MAX_MINIMAP_SIZE*MAX_MINIMAP_SIZE,1);
  mm_renderer_prerender(&r,&ctx); assert(copies==1 && barriers==2);
  mm_renderer_prerender(&r,&ctx); assert(copies==1);
  ctx.current_frame=1; mm_renderer_prerender(&r,&ctx); assert(copies==2);
  r.minimap[8]=123; ctx.current_frame=0; mm_renderer_prerender(&r,&ctx); assert(copies==3);
  assert(((unsigned char*)buffers[0].data)[8]==123 && ((unsigned char*)buffers[1].data)[8]==0);
  ctx.current_frame=1; mm_renderer_prerender(&r,&ctx); assert(copies==4);
  assert(((unsigned char*)buffers[1].data)[8]==123);
  for(int i=0;i<100;i++) { ctx.current_frame=i%2; mm_renderer_prerender(&r,&ctx); }
  assert(copies==4 && barriers==8);
  for(int i=0;i<2;i++) free(buffers[i].data);
  puts("Minimap: first upload, unchanged-frame skip and independent in-flight refresh passed");
}
