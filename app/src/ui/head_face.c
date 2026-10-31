#include "head_face.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "../cimgui/cimgui.h"
#include <math.h>

static ImVec2 point(float x,float y,float scale,float c,float s,float ahead,float side) {
  return (ImVec2){x+scale*(c*ahead-s*side),y+scale*(s*ahead+c*side)};
}
bool head_face_available(bool smile,bool claws) {
  return (smile || claws) && (!smile || head_face_texture(0)) && (!claws || head_face_texture(1));
}
static void sprite(ImDrawList* draw,int index,ImVec2 center,float scale,float angle,
                   float left,float top,float width,float height,ImU32 color) {
  float c=cosf(angle),s=sinf(angle);
  ImDrawList_AddImageQuad(draw,(ImTextureRef){NULL,(ImTextureID)head_face_texture(index)},
    point(center.x,center.y,scale,c,s,left,top),point(center.x,center.y,scale,c,s,left+width,top),
    point(center.x,center.y,scale,c,s,left+width,top+height),point(center.x,center.y,scale,c,s,left,top+height),
    (ImVec2){0,0},(ImVec2){1,0},(ImVec2){1,1},(ImVec2){0,1},color);
}
void head_face_draw(float x,float y,float scale,float unit,float angle,float alpha,bool smile,bool claws,
                    float gaze_x,float gaze_y,const float head_color[3]) {
  if(!head_face_available(smile,claws) || scale<=0 || alpha<=0) return;
  ImDrawList* draw=igGetWindowDrawList();
  float c=cosf(angle),s=sinf(angle);
  // The original adds +.5 in world units to some offsets; express it in head-size units.
  float half=.5f*unit/scale;
  ImU32 black=igColorConvertFloat4ToU32((ImVec4){0,0,0,alpha});
  ImU32 white=igColorConvertFloat4ToU32((ImVec4){1,1,1,alpha});
  if(claws) {
    // Original skin 41 (slg): ed=34, esp=14, er=8, eo=3, pr=4.5, pma=3, sest5 stalks.
    // Stalks first so the eyes sit on top of them, as in the WebGL game.
    ImU32 stalk=igColorConvertFloat4ToU32((ImVec4){head_color?head_color[0]:1,head_color?head_color[1]:1,
                                                   head_color?head_color[2]:1,alpha});
    for(int side=-1;side<=1;side+=2)
      sprite(draw,1,point(x,y,scale,c,s,13,side*(6+half)),scale*.25f,angle+side*.4f,-28,-44,105,88,stalk);
    for(int side=-1;side<=1;side+=2) {
      ImVec2 eye=point(x,y,scale,c,s,34,side*(14+half));
      ImDrawList_AddCircleFilled(draw,eye,8*scale+1.5f*unit,black,28);
      ImDrawList_AddCircleFilled(draw,eye,8*scale,white,24);
      ImVec2 pupil=point(x,y,scale,c,s,34+half,side*14);
      // The game moves the pupil up to pma=3 for this eye; gaze_* is computed for the default 2.3.
      pupil.x+=gaze_x*scale*(3/2.3f); pupil.y+=gaze_y*scale*(3/2.3f);
      ImDrawList_AddCircleFilled(draw,pupil,4.5f*scale,black,20);
    }
  } else {
    for(int side=-1;side<=1;side+=2) {
      // Original skin 40 (jyt): ed=-3, esp=7, ecmc canvas path (48x48) at .25, sampled quadratics.
      ImVec2 center=point(x,y,scale,c,s,-3+half,side*7),path[26];
      center.x+=gaze_x*scale; center.y+=gaze_y*scale;
      path[0]=point(center.x,center.y,scale*.25f,c,s,12,-18);
      for(int j=0;j<=12;j++) {
        float t=j/12.0f,u=1-t;
        path[1+j]=point(center.x,center.y,scale*.25f,c,s,30*(u*u+t*t)-24,6*u*u+48*u*t+42*t*t-24);
      }
      for(int j=0;j<12;j++) {
        float t=j/12.0f,u=1-t;
        path[14+j]=point(center.x,center.y,scale*.25f,c,s,36*(u*u+t*t)+28*u*t-24,42*u*u+48*u*t+6*t*t-24);
      }
      ImDrawList_AddConcavePolyFilled(draw,path,26,black);
    }
  }
  if(smile) {
    // Original: ed=5, jmou3 at .16, follows the gaze like the eyes.
    ImVec2 mouth=point(x,y,scale,c,s,5+half,0);
    mouth.x+=gaze_x*scale; mouth.y+=gaze_y*scale;
    sprite(draw,0,mouth,scale*.16f,angle,-40,-65,79,130,white);
  }
}
