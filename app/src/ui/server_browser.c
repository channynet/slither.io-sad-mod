#include "server_browser.h"
#include "../network/server_catalog.h"
#include "../network/server_status.h"
#include "../network/server.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char* region_name(const char* code) {
  if(!strcmp(code,"AS")) return "아시아";
  if(!strcmp(code,"EU")) return "유럽";
  if(!strcmp(code,"NA")) return "북미";
  if(!strcmp(code,"SA")) return "남미";
  if(!strcmp(code,"OC")) return "오세아니아";
  if(!strcmp(code,"AF")) return "아프리카";
  return "지역 미확인";
}
static const char* country_name(const char* code) {
  static const struct { const char* code; const char* name; } names[]={
    {"JP","일본"},{"KR","한국"},{"SG","싱가포르"},{"HK","홍콩"},{"CN","중국"},
    {"TW","대만"},{"IN","인도"},{"AU","호주"},{"NZ","뉴질랜드"},{"US","미국"},
    {"CA","캐나다"},{"BR","브라질"},{"AR","아르헨티나"},{"CL","칠레"},
    {"GB","영국"},{"DE","독일"},{"FR","프랑스"},{"NL","네덜란드"},{"PL","폴란드"},
    {"IT","이탈리아"},{"ES","스페인"},{"SE","스웨덴"},{"RU","러시아"},{"ZA","남아프리카공화국"}
  };
  for(size_t i=0;i<sizeof(names)/sizeof(*names);i++) if(!strcmp(code,names[i].code)) return names[i].name;
  return *code?code:"국가 미확인";
}
static void age_text(int64_t stamp,char* out,size_t cap) {
  if(stamp<=0) { snprintf(out,cap,"갱신 시각 미확인"); return; }
  int64_t age=(int64_t)time(NULL)-stamp;
  if(age<0) { snprintf(out,cap,"갱신 시각 확인 필요"); return; }
  if(age<60) snprintf(out,cap,"%lld초 전",(long long)age);
  else if(age<3600) snprintf(out,cap,"%lld분 전%s",(long long)(age/60),age>300?" · 오래된 정보":"");
  else snprintf(out,cap,"%lld시간 전 · 오래된 정보",(long long)(age/3600));
}
void ui_server_browser(tenv* env,float width) {
  static server_catalog catalog;
  static char selected[MAX_IPV4_LEN+1],search[80];
  static int sort=0;
  if(igButton("실시간 서버 선택",(ImVec2){width,0})) {
    snprintf(selected,sizeof(selected),"%s",env->usr->usrs.ipv4);
    igOpenPopup_Str("실시간 서버 선택##browser",0);
  }
  ImVec2 size={fminf(1050,env->ctx->size[0]-32),fminf(780,env->ctx->size[1]-32)};
  igSetNextWindowSize(size,ImGuiCond_Appearing);
  igSetNextWindowPos((ImVec2){env->ctx->size[0]/2.0f,env->ctx->size[1]/2.0f},ImGuiCond_Appearing,(ImVec2){.5f,.5f});
  bool open=true;
  if(!igBeginPopupModal("실시간 서버 선택##browser",&open,ImGuiWindowFlags_NoSavedSettings)) return;
  server_catalog_request(); server_catalog_copy(&catalog);
  igTextDisabled("목록을 연 동안 30초마다 갱신합니다. 접속 수에는 봇이 포함될 수 있습니다.");
  if(catalog.failed) {
    if(catalog.http_status) igTextColored((ImVec4){1,.65f,.3f,1},"목록 수신 실패 (HTTP %d). 잠시 후 자동으로 재시도합니다.",catalog.http_status);
    else igTextColored((ImVec4){1,.65f,.3f,1},"목록 서버에 연결하지 못했습니다. 잠시 후 자동으로 재시도합니다.");
    if(catalog.received_at) igTextDisabled("이전에 받은 목록을 표시합니다. 서버별 갱신 시각을 확인하세요.");
  } else if(catalog.pending) igTextDisabled("서버 목록을 받는 중입니다.");
  else { char age[80]; age_text(catalog.received_at,age,sizeof(age)); igTextDisabled("%d개 서버 · 목록 수신 %s",catalog.count,age); }
  igSetNextItemWidth(fminf(280,size.x*.4f));
  igInputTextWithHint("##server_search","주소 / 국가 코드 / 도시 검색",search,sizeof(search),0,NULL,NULL);
  igSameLine(0,-1); igSetNextItemWidth(160);
  igCombo_Str("##server_sort",&sort,"접속 수 많은 순\0핑 낮은 순\0국가 순\0",-1);
  int order[SERVER_CATALOG_LIMIT],n=0;
  server_status statuses[SERVER_CATALOG_LIMIT];
  for(int i=0;i<catalog.count;i++) {
    server_catalog_entry* row=&catalog.entries[i];
    server_status_get(row->address,&statuses[i]);
    if(*search && !strstr(row->address,search) && !strstr(row->country,search) &&
       !strstr(row->location,search) && !strstr(country_name(row->country),search)) continue;
    order[n++]=i;
  }
  for(int i=1;i<n;i++) {
    int k=order[i],j=i-1;
    while(j>=0) {
      int a=order[j]; bool before=false;
      if(sort==0) before=catalog.entries[k].players>catalog.entries[a].players;
      if(sort==1) {
        int ap=statuses[a].failed || statuses[a].ping_ms<0?1000000:statuses[a].ping_ms;
        int kp=statuses[k].failed || statuses[k].ping_ms<0?1000000:statuses[k].ping_ms;
        before=kp<ap;
      }
      if(sort==2) before=strcmp(catalog.entries[k].country,catalog.entries[a].country)<0;
      if(!before) break;
      order[j+1]=order[j]; j--;
    }
    order[j+1]=k;
  }
  ImVec2 avail; igGetContentRegionAvail(&avail);
  float list_height=fmaxf(100,(avail.y-igGetFrameHeightWithSpacing()*5)*.48f);
  if(igBeginTable("##server_table",5,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_ScrollY|ImGuiTableFlags_Resizable,(ImVec2){0,list_height},0)) {
    igTableSetupColumn("국가 · 지역",ImGuiTableColumnFlags_WidthStretch,1.4f,0);
    igTableSetupColumn("서버 주소",ImGuiTableColumnFlags_WidthStretch,1.6f,0);
    igTableSetupColumn("접속 수",ImGuiTableColumnFlags_WidthFixed,75,0);
    igTableSetupColumn("핑",ImGuiTableColumnFlags_WidthFixed,90,0);
    igTableSetupColumn("정보 갱신",ImGuiTableColumnFlags_WidthStretch,1.2f,0);
    igTableSetupScrollFreeze(0,1); igTableHeadersRow();
    for(int k=0;k<n;k++) {
      int i=order[k]; server_catalog_entry* row=&catalog.entries[i]; server_status* status=&statuses[i];
      igPushID_Str(row->address); igTableNextRow(0,0); igTableSetColumnIndex(0);
      char label[180]; snprintf(label,sizeof(label),"%s · %s###pick",country_name(row->country),region_name(row->region));
      if(igSelectable_Bool(label,!strcmp(selected,row->address),ImGuiSelectableFlags_SpanAllColumns,(ImVec2){0})) strcpy(selected,row->address);
      if(igIsItemVisible()) server_status_request(row->address);
      if(igIsItemHovered(0)) igSetTooltip("%s\n%s",row->location[0]?row->location:"도시 미확인",row->address);
      igTableSetColumnIndex(1); igTextUnformatted(row->address,NULL);
      igTableSetColumnIndex(2); if(row->players<0) igTextDisabled("미확인"); else igText("%d",row->players);
      igTableSetColumnIndex(3);
      if(status->pending) igTextDisabled("측정 중");
      else if(status->failed) igTextDisabled("응답 없음");
      else if(status->ping_ms<0) igTextDisabled("미측정");
      else igText("%d ms",status->ping_ms);
      if(igIsItemHovered(0)) igSetTooltip("이 PC에서 측정한 /ptc 응답 시간입니다. 실제 게임 핑과 다를 수 있습니다.");
      igTableSetColumnIndex(4); char age[80]; age_text(row->updated_at,age,sizeof(age)); igTextUnformatted(age,NULL);
      igPopID();
    }
    igEndTable();
  }
  server_catalog_entry* picked=NULL;
  for(int i=0;i<catalog.count;i++) if(!strcmp(selected,catalog.entries[i].address)) picked=&catalog.entries[i];
  igSeparator();
  if(picked) {
    char age[80]; age_text(picked->updated_at,age,sizeof(age));
    igText("%s · %s · %s",picked->address,country_name(picked->country),picked->location);
    igTextDisabled("상위 10명 · %s",age);
    server_status_request(picked->address);
  } else igTextDisabled("서버를 선택하면 상위 10명의 닉네임과 점수가 표시됩니다.");
  igGetContentRegionAvail(&avail);
  float ranking_height=fmaxf(80,avail.y-igGetFrameHeightWithSpacing()*2.5f);
  if(igBeginTable("##server_leaders",3,ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY,(ImVec2){0,ranking_height},0)) {
    igTableSetupColumn("순위",ImGuiTableColumnFlags_WidthFixed,60,0);
    igTableSetupColumn("닉네임",ImGuiTableColumnFlags_WidthStretch,1,0);
    igTableSetupColumn("점수",ImGuiTableColumnFlags_WidthFixed,140,0);
    igTableSetupScrollFreeze(0,1); igTableHeadersRow();
    if(picked) for(int i=0;i<picked->leader_count;i++) {
      server_leader* p=&picked->leaders[i];
      igTableNextRow(0,0); igTableSetColumnIndex(0); igText("%d",p->rank);
      igTableSetColumnIndex(1); igTextUnformatted(p->name[0]?p->name:"(이름 없음)",NULL);
      igTableSetColumnIndex(2); igText("%d",p->score);
    }
    if(picked && !picked->leader_count) { igTableNextRow(0,0); igTableSetColumnIndex(1); igTextDisabled("순위 정보 없음"); }
    igEndTable();
  }
  igBeginDisabled(!picked);
  bool join=igButton("선택한 서버로 접속",(ImVec2){210,0});
  igEndDisabled(); igSameLine(0,-1);
  if(igButton("닫기",(ImVec2){80,0})) igCloseCurrentPopup();
  igTextDisabled("목록 출처: leaderboard.0x0efe.dev (saya) · 핑: 내 PC 측정");
  if(join && picked) {
    strcpy(env->usr->usrs.ipv4,picked->address);
    igCloseCurrentPopup();
    env->usr->gdata.conn=CONNECTING; env->usr->gdata.curr_screen=PLAYING;
    glfwSetTime(0); server_connect(env);
  }
  igEndPopup();
}
