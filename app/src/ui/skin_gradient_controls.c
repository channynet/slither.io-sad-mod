#include "skin_gradient.h"
#include "../user.h"
#include "../game/native_preferences.h"
#include "fire_skin.h"

void skin_gradient_controls(void) {
  igCheckbox("그라데이션 스킨 · Ctrl+F2",&native_prefs.gradient_enabled);
  igTextWrapped("머리에서 꼬리까지 선택한 색이 이어집니다. 외형 공유를 켜면 같은 서버의 이 클라이언트 사용자에게도 보입니다. 설정은 자동 저장됩니다.");
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
  static const char* descriptions[]={
    "특별한 효과를 끄고 기본 스킨 또는 그라데이션을 표시합니다.",
    "뜨거운 색이 몸을 타고 흐르며 양옆으로 불꽃이 피어오릅니다.",
    "차가운 빛이 몸을 감싸고 반짝이는 얼음 결정이 흩어집니다.",
    "푸른 전류가 몸을 타고 달리며 짧은 번개가 튀어 오릅니다.",
    "초록과 보랏빛의 부드러운 빛 띠가 몸을 따라 일렁입니다.",
    "보랏빛 몸 주위를 작은 금빛 별들이 돌며 반짝입니다.",
    "청록빛 물결이 흐르고 작은 기포와 둥근 파동이 퍼집니다."};
  int selected=special_skin_selected(native_prefs.fire_skin,native_prefs.special_skin);
  ImVec2 available;igGetContentRegionAvail(&available);
  if(igSelectable_Bool("효과 끄기",selected==SPECIAL_NONE,0,(ImVec2){0})) selected=SPECIAL_NONE;
  float width=fmaxf(70,(available.x-igGetStyle()->ItemSpacing.x)*.5f);
  for(int kind=SPECIAL_FIRE;kind<SPECIAL_COUNT;kind++) {
    if((kind-1)%2) igSameLine(0,-1);
    igPushStyleColor_Vec4(ImGuiCol_Button,igGetStyle()->Colors[selected==kind?ImGuiCol_ButtonActive:ImGuiCol_Button]);
    if(igButton(special_skin_name(kind),(ImVec2){width,38})) selected=kind;
    igPopStyleColor(1);
  }
  native_prefs.special_skin=selected;native_prefs.fire_skin=selected==SPECIAL_FIRE;
  igSpacing();igSeparatorText(special_skin_name(selected));
  igTextWrapped("%s",descriptions[selected]);
  igBeginDisabled(selected==SPECIAL_NONE);
  igSetNextItemWidth(fmaxf(70,available.x-75));
  igSliderFloat("효과 강도",&native_prefs.fire_strength,.2f,1.5f,"%.1f",ImGuiSliderFlags_AlwaysClamp);
  igEndDisabled();
  ImVec2 start,space; igGetCursorScreenPos(&start); igGetContentRegionAvail(&space);
  int parts=24; float w=fmaxf(20,space.x)/parts;
  for(int i=0;i<parts;i++) {
    float c[3]; special_body_color(c,selected,i,parts,(float)igGetTime());
    ImDrawList_AddRectFilled(igGetWindowDrawList(),(ImVec2){start.x+i*w,start.y},(ImVec2){start.x+(i+1)*w,start.y+22},
      igColorConvertFloat4ToU32((ImVec4){c[0],c[1],c[2],1}),0,0);
  }
  igDummy((ImVec2){space.x,26});
  igTextWrapped("한 번에 하나의 효과를 사용합니다. 얼굴·태그·cosmetic과 함께 쓸 수 있습니다. OK를 누르면 스킨 목록에도 보관됩니다.");
  igTextWrapped("외형 공유를 켜면 같은 서버의 최신 클라이언트 사용자에게도 보입니다.");
}
