#include "skin_gradient.h"
#include "../user.h"
#include "../game/native_preferences.h"
#include "fire_skin.h"

void skin_gradient_controls(void) {
  igCheckbox("그라데이션 스킨 · Ctrl+F2",&native_prefs.gradient_enabled);
  igTextWrapped("머리에서 꼬리까지 선택한 색이 이어집니다. 내 화면에만 보이며 자동 저장됩니다.");
  igSliderInt("색 개수",&native_prefs.gradient_count,2,4,"%d",ImGuiSliderFlags_AlwaysClamp);
  for(int i=0;i<native_prefs.gradient_count;i++) {
    char label[40];
    if(i==0) snprintf(label,sizeof(label),"머리 색");
    else if(i==native_prefs.gradient_count-1) snprintf(label,sizeof(label),"꼬리 색");
    else snprintf(label,sizeof(label),"중간 색 %d",i);
    igPushID_Int(i); igColorEdit3(label,native_prefs.gradient_colors[i],ImGuiColorEditFlags_DisplayHex); igPopID();
  }
  if(igButton("색 순서 뒤집기",(ImVec2){0})) {
    for(int i=0;i<native_prefs.gradient_count/2;i++) for(int c=0;c<3;c++) {
      int j=native_prefs.gradient_count-1-i;
      float temp=native_prefs.gradient_colors[i][c]; native_prefs.gradient_colors[i][c]=native_prefs.gradient_colors[j][c]; native_prefs.gradient_colors[j][c]=temp;
    }
  }
  ImVec2 start,space; igGetCursorScreenPos(&start); igGetContentRegionAvail(&space);
  float width=fmaxf(20,space.x)/(native_prefs.gradient_count-1);
  for(int i=0;i<native_prefs.gradient_count-1;i++) {
    float* a=native_prefs.gradient_colors[i]; float* b=native_prefs.gradient_colors[i+1];
    ImU32 left=igColorConvertFloat4ToU32((ImVec4){a[0],a[1],a[2],1}),right=igColorConvertFloat4ToU32((ImVec4){b[0],b[1],b[2],1});
    ImDrawList_AddRectFilledMultiColor(igGetWindowDrawList(),(ImVec2){start.x+i*width,start.y},
      (ImVec2){start.x+(i+1)*width,start.y+26},left,right,right,left);
  }
  igDummy((ImVec2){space.x,30});
  igTextWrapped("얼굴·태그·cosmetic과 함께 사용할 수 있습니다. 끄면 기존 몸 색상으로 돌아갑니다.");
}

void fire_skin_controls(void) {
  igCheckbox("불꽃 스킨",&native_prefs.fire_skin);
  igTextWrapped("몸이 불타는 색으로 일렁이고, 아래로 불빛이 번지며 옆구리에서 불꽃이 피어올라 뒤로 흩어집니다. 내 화면의 내 지렁이에만 보이며 자동 저장됩니다.");
  igBeginDisabled(!native_prefs.fire_skin);
  igSetNextItemWidth(180);
  igSliderFloat("불꽃 크기",&native_prefs.fire_strength,.2f,1.5f,"%.1f",ImGuiSliderFlags_AlwaysClamp);
  igEndDisabled();
  // a strip of the fire colours, moving like the body does
  ImVec2 start,space; igGetCursorScreenPos(&start); igGetContentRegionAvail(&space);
  int parts=24; float w=fmaxf(20,space.x)/parts;
  for(int i=0;i<parts;i++) {
    float c[3]; fire_body_color(c,i,parts,(float)igGetTime());
    ImDrawList_AddRectFilled(igGetWindowDrawList(),(ImVec2){start.x+i*w,start.y},(ImVec2){start.x+(i+1)*w,start.y+22},
      igColorConvertFloat4ToU32((ImVec4){c[0],c[1],c[2],1}),0,0);
  }
  igDummy((ImVec2){space.x,26});
  igTextWrapped("켜 두면 그라데이션 스킨보다 우선합니다. 얼굴·태그·cosmetic과 함께 쓸 수 있습니다.");
}
