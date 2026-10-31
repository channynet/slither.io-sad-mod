#include "head_face_assets.h"
#include "head_face.h"
#include "../user.h"
#include <external/stb/stb_image.h>
static texture* images[2];
static VkDescriptorSet descriptors[2];
uint64_t head_face_texture(int index) {
  return index>=0 && index<2?(uint64_t)descriptors[index]:0;
}
void head_face_init(tenv* env) {
  const char* paths[]={"app/res/faces/jmou3.png","app/res/faces/sest5.png"};
  // Decode/upload once at startup, never during gameplay or option toggling.
  for(int i=0;i<2;i++) {
    int w,h,c; unsigned char* pixels=stbi_load(paths[i],&w,&h,&c,4);
    if(!pixels) continue;
    if(i==1) {
      // The stalk art is red. Keep only its shading so it can be tinted with the head colour.
      int peak=1;
      for(int p=0;p<w*h;p++) if(pixels[p*4+3]) for(int k=0;k<3;k++) if(pixels[p*4+k]>peak) peak=pixels[p*4+k];
      for(int p=0;p<w*h;p++) {
        int value=pixels[p*4]; if(pixels[p*4+1]>value) value=pixels[p*4+1]; if(pixels[p*4+2]>value) value=pixels[p*4+2];
        value=value*255/peak; if(value>255) value=255;
        pixels[p*4]=pixels[p*4+1]=pixels[p*4+2]=(unsigned char)value;
      }
    }
    images[i]=create_rgba_texture(env->ctx,pixels,w,h); stbi_image_free(pixels);
    if(images[i]) descriptors[i]=igImplVulkan_AddTexture(env->usr->r->linear_sampler,
        images[i]->view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }
}
void head_face_destroy(tenv* env) {
  for(int i=0;i<2;i++) {
    if(descriptors[i]) igImplVulkan_RemoveTexture(descriptors[i]);
    if(images[i]) destroy_texture(env->ctx,images[i]);
    descriptors[i]=0; images[i]=NULL;
  }
}
