#include "../app/src/rendering/mm_renderer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static int copies,barriers;
static VkBufferImageCopy last_copy;
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
VKAPI_ATTR void VKAPI_CALL vkCmdCopyBufferToImage(VkCommandBuffer a,VkBuffer b,VkImage c,VkImageLayout d,uint32_t e,const VkBufferImageCopy* f) { copies++; last_copy=*f; }
VkResult vmaFlushAllocation(VmaAllocator a,VmaAllocation b,VkDeviceSize c,VkDeviceSize d) { return VK_SUCCESS; }
int main(void) {
  mm_renderer r={0}; tcontext ctx={0}; tcontext_frame frames[2]={0}; ctx.frames=frames;
  bool uploaded[2]={false,false}; r.uploaded=uploaded;
  texture images[2]={0}; texture* textures[]={images,images+1}; r.minimap_tex=textures;
  tdbuffer buffers[2]={0}; r.minimap_buffer=buffers;
  r.shadow=calloc(2*MAX_MINIMAP_SIZE*MAX_MINIMAP_SIZE,1);
  for(int i=0;i<2;i++) buffers[i].data=calloc(MAX_MINIMAP_SIZE*MAX_MINIMAP_SIZE,1);
  mm_renderer_prerender(&r,&ctx); assert(copies==1 && barriers==2);
  // The first upload defines the whole texture.
  assert(last_copy.bufferOffset==0 && last_copy.imageExtent.width==MAX_MINIMAP_SIZE && last_copy.imageExtent.height==MAX_MINIMAP_SIZE);
  mm_renderer_prerender(&r,&ctx); assert(copies==1);
  ctx.current_frame=1; mm_renderer_prerender(&r,&ctx); assert(copies==2);
  r.minimap[8]=123; ctx.current_frame=0; mm_renderer_prerender(&r,&ctx); assert(copies==3);
  assert(((unsigned char*)buffers[0].data)[8]==123 && ((unsigned char*)buffers[1].data)[8]==0);
  ctx.current_frame=1; mm_renderer_prerender(&r,&ctx); assert(copies==4);
  assert(((unsigned char*)buffers[1].data)[8]==123);
  for(int i=0;i<100;i++) { ctx.current_frame=i%2; mm_renderer_prerender(&r,&ctx); }
  assert(copies==4 && barriers==8);
  // With a known map size only changed rows inside that square are sent.
  r.size=80; r.minimap[100*MAX_MINIMAP_SIZE+3]=9; r.minimap[5*MAX_MINIMAP_SIZE+200]=9;
  ctx.current_frame=0; mm_renderer_prerender(&r,&ctx); assert(copies==4);
  r.minimap[5*MAX_MINIMAP_SIZE+3]=77; r.minimap[7*MAX_MINIMAP_SIZE+79]=78;
  mm_renderer_prerender(&r,&ctx); assert(copies==5 && barriers==10);
  assert(last_copy.bufferOffset==5*MAX_MINIMAP_SIZE && last_copy.bufferRowLength==MAX_MINIMAP_SIZE);
  assert(last_copy.imageOffset.x==0 && last_copy.imageOffset.y==5 && last_copy.imageExtent.width==80 && last_copy.imageExtent.height==3);
  unsigned char* sent=buffers[0].data;
  assert(sent[5*MAX_MINIMAP_SIZE+3]==77 && sent[7*MAX_MINIMAP_SIZE+79]==78);
  assert(sent[5*MAX_MINIMAP_SIZE+200]==0 && sent[100*MAX_MINIMAP_SIZE+3]==0);
  mm_renderer_prerender(&r,&ctx); assert(copies==5);
  // The other in-flight buffer catches up independently, and a larger map picks up what was skipped.
  ctx.current_frame=1; mm_renderer_prerender(&r,&ctx); assert(copies==6 && ((unsigned char*)buffers[1].data)[7*MAX_MINIMAP_SIZE+79]==78);
  r.size=0; ctx.current_frame=0; mm_renderer_prerender(&r,&ctx); assert(copies==7);
  assert(sent[5*MAX_MINIMAP_SIZE+200]==9 && sent[100*MAX_MINIMAP_SIZE+3]==9);
  assert(last_copy.imageOffset.y==5 && last_copy.imageExtent.height==96 && last_copy.imageExtent.width==MAX_MINIMAP_SIZE);
  free(r.shadow);
  for(int i=0;i<2;i++) free(buffers[i].data);
  puts("Minimap: first upload, unchanged-frame skip, used-region rows and independent in-flight refresh passed");
}
