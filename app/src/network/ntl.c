#include "ntl.h"
#include "server.h"
#include "../game/perf_trace.h"
#include "ntl_protocol.h"
#include "credential_store.h"
#include "../user.h"
#include "../ui/viewport.h"
#include "../ui/personal_tag.h"
#include "../game/native_preferences.h"
#include "../game/native_shortcuts.h"
#include "../game/death_review.h"
#include "../game/target_marker.h"
#include "../ui/team_minimap.h"
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <mmsystem.h>
#include <math.h>

static struct {
  CRITICAL_SECTION lock;
  HANDLE stop, wake, thread;
  bool enabled;
  unsigned generation, revision;
  ntl_request request;
  ntl_reply reply;
  ULONGLONG updated;
  char status[256];
  char reply_server[161];
} shared;
static ntl_reply visible, previous;
static unsigned revision;
static char status[256]="팀 연결 안 됨", auth[513], team[513], prefix[9], draft[901];
static wchar_t credentials_path[32768];
static bool keys_saved;
static char key_status[192];
typedef struct { char nick[257],text[2048],time[6]; } chat_line;
static chat_line history[160];
static char visible_server[161],own_identity[257];
static bool options_open,chat_at_bottom=true;
static int history_count, history_start;
static bool capture_mouse, active, scroll_chat, demo, focus_chat;
#define panel native_prefs.team_panel
#define cap_choice native_prefs.frame_cap
#define scale_choice native_prefs.render_scale
static ULONGLONG updated;
static LARGE_INTEGER frequency;
static double next_frame, last_frame, stats_at;
static float samples[600], average, p99;
static int sample_count, sample_next, slow_frames;
static const int caps[]={60,120,0};
static double now_ms(void) { LARGE_INTEGER q; QueryPerformanceCounter(&q); return q.QuadPart*1000.0/frequency.QuadPart; }
static void copy(char* dst, size_t cap, const char* src) { snprintf(dst,cap,"%s",src); }
static void log_line(const char* nick, const char* text) {
  if (!*text) return;
  int slot=(history_start+history_count)%160;
  if (history_count==160) { history_start=(history_start+1)%160; slot=(history_start+159)%160; }
  else history_count++;
  copy(history[slot].nick,sizeof(history[slot].nick),nick);
  copy(history[slot].text,sizeof(history[slot].text),text);
  SYSTEMTIME time; GetLocalTime(&time);
  snprintf(history[slot].time,sizeof(history[slot].time),"%02u:%02u",time.wHour,time.wMinute);
  scroll_chat=true;
}

// Synchronous HTTP is confined to this worker. The renderer never waits for it.
static char* fetch(HINTERNET connection, const ntl_request* input, DWORD* error) {
  char* path=calloc(32768,1); wchar_t* wide=calloc(32768,sizeof(wchar_t));
  char* body=NULL; HINTERNET request=NULL;
  *error=ERROR_INVALID_DATA;
  if (!path || !wide || !ntl_build_path(input,path,32768)) goto done;
  if (!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,32768)) goto done;
  request=WinHttpOpenRequest(connection,L"GET",wide,NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
  if (!request) goto failed;
  DWORD disabled=WINHTTP_DISABLE_REDIRECTS|WINHTTP_DISABLE_COOKIES;
  if (!WinHttpSetOption(request,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled))) goto failed;
  if (!WinHttpSendRequest(request,L"Cache-Control: no-store\r\n",-1,NULL,0,0,0) || !WinHttpReceiveResponse(request,NULL)) goto failed;
  DWORD status_code=0,size=sizeof(status_code);
  if (!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,NULL,&status_code,&size,NULL)) goto failed;
  if (status_code!=200) { *error=status_code; goto done; }
  body=calloc(1048577,1);
  if (!body) { *error=ERROR_NOT_ENOUGH_MEMORY; goto done; }
  DWORD total=0,received=0;
  ULONGLONG deadline=GetTickCount64()+6000;
  for (;;) {
    if (WaitForSingleObject(shared.stop,0)==WAIT_OBJECT_0 || GetTickCount64()>deadline) { *error=ERROR_TIMEOUT; goto bad_body; }
    if (!WinHttpReadData(request,body+total,min(16384,1048576-total),&received)) { *error=GetLastError(); goto bad_body; }
    if (!received) break;
    total+=received;
    if (total>=1048576) { *error=ERROR_INSUFFICIENT_BUFFER; goto bad_body; }
  }
  if (memchr(body,0,total)) { *error=ERROR_INVALID_DATA; goto bad_body; }
  *error=0; goto done;
failed: *error=GetLastError(); goto done;
bad_body: free(body); body=NULL;
done:
  if (request) WinHttpCloseHandle(request);
  if (path) { SecureZeroMemory(path,32768); free(path); }
  if (wide) { SecureZeroMemory(wide,32768*sizeof(wchar_t)); free(wide); }
  return body;
}
static DWORD WINAPI worker(void* unused) {
  (void)unused;
  HINTERNET session=WinHttpOpen(L"SlitherNative/0.1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
  if (session) WinHttpSetTimeouts(session,2000,2000,3000,3000);
  HINTERNET connection=session ? WinHttpConnect(session,L"ntl-slither.com",443,0) : NULL;
  ntl_reply* reply=calloc(1,sizeof(*reply));
  HANDLE events[]={shared.stop,shared.wake}; DWORD delay=INFINITE; unsigned failures=0;
  while (WaitForMultipleObjects(2,events,FALSE,delay)!=WAIT_OBJECT_0) {
    ntl_request request; unsigned generation; bool enabled;
    EnterCriticalSection(&shared.lock);
    enabled=shared.enabled; generation=shared.generation; request=shared.request;
    // Consume once, even if the response is lost: never replay a chat automatically.
    SecureZeroMemory(shared.request.message,sizeof(shared.request.message));
    LeaveCriticalSection(&shared.lock);
    if (!enabled) { SecureZeroMemory(&request,sizeof(request)); delay=INFINITE; failures=0; continue; }
    DWORD error=ERROR_NOT_READY;
    char* body=connection && reply ? fetch(connection,&request,&error) : NULL;
    int parsed=body ? ntl_parse(body,request.nick,reply) : 2;
    EnterCriticalSection(&shared.lock);
    if (shared.enabled && generation==shared.generation) {
      if (body && parsed==0) {
        shared.reply=*reply; shared.updated=GetTickCount64(); failures=0;
        copy(shared.reply_server,sizeof(shared.reply_server),request.playing ? request.server : "");
        copy(shared.status,sizeof(shared.status),reply->own_echo ? "NTL 연결 확인 · 4초마다 갱신" : "응답 수신 · 내 접속 확인 대기");
        if (request.message[0]) {
          bool echo=false;
          for(int i=0;i<reply->count;i++) if (!strcmp(reply->rows[i].nick,request.nick) && strstr(reply->rows[i].message,request.message)) echo=true;
          copy(shared.status,sizeof(shared.status),echo ? "내 채팅의 서버 반영 확인" : "채팅 요청 완료 · 반영 여부 미확인");
        }
      } else if (body && parsed==1) {
        shared.enabled=false;
        SecureZeroMemory(&shared.request,sizeof(shared.request));
        copy(shared.status,sizeof(shared.status),"팀키 거절됨 · 두 키를 확인해 주세요");
      } else {
        failures++;
        snprintf(shared.status,sizeof(shared.status),request.message[0] ? "전송 결과 불명확 (%lu) · 자동 재전송 안 함" : "NTL 응답 오류 (%lu) · 다시 연결 중",error);
      }
      shared.revision++;
    }
    bool still_enabled=shared.enabled;
    LeaveCriticalSection(&shared.lock);
    if (body) { SecureZeroMemory(body,strlen(body)); free(body); }
    SecureZeroMemory(&request,sizeof(request));
    delay=still_enabled ? min(30000,4000*(1+failures)) : INFINITE;
  }
  free(reply);
  if(connection) WinHttpCloseHandle(connection);
  if(session) WinHttpCloseHandle(session);
  return 0;
}
void ntl_init(void) {
  if(credential_store_path(credentials_path,32768)) {
    keys_saved=credential_store_load(credentials_path,auth,team);
    if(keys_saved) copy(key_status,sizeof(key_status),"저장된 팀키를 불러왔습니다");
  }
  InitializeCriticalSection(&shared.lock);
  shared.stop=CreateEventW(NULL,TRUE,FALSE,NULL);
  shared.wake=CreateEventW(NULL,FALSE,FALSE,NULL);
  unsigned char bytes[8];
  if (BCryptGenRandom(NULL,bytes,8,BCRYPT_USE_SYSTEM_PREFERRED_RNG)>=0) {
    const char alphabet[]="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    for(int i=0;i<8;i++) prefix[i]=alphabet[bytes[i]%62];
  }
  if(shared.stop && shared.wake && prefix[0]) shared.thread=CreateThread(NULL,0,worker,NULL,0,NULL);
  if(!shared.thread) copy(status,sizeof(status),"NTL 통신 작업을 시작하지 못했습니다");
  QueryPerformanceFrequency(&frequency); timeBeginPeriod(1);
}
void ntl_destroy(void) {
  if(shared.stop) SetEvent(shared.stop);
  if(shared.thread) { WaitForSingleObject(shared.thread,INFINITE); CloseHandle(shared.thread); }
  if(shared.stop) CloseHandle(shared.stop);
  if(shared.wake) CloseHandle(shared.wake);
  DeleteCriticalSection(&shared.lock);
  SecureZeroMemory(&shared,sizeof(shared)); SecureZeroMemory(auth,sizeof(auth)); SecureZeroMemory(team,sizeof(team));
  timeEndPeriod(1);
}
bool ntl_mouse_captured(void) { return capture_mouse || native_settings_open || focus_chat; }
bool ntl_is_teammate(tenv* env,int sid) {
  if(sid<0 || sid>65535 || !active || !updated || GetTickCount64()-updated>10000 || strcmp(visible_server,env->usr->usrs.ipv4)) return false;
  const char* server=env->usr->usrs.ipv4;
  for(int i=0;i<visible.count;i++) if(!strcmp(visible.rows[i].nick,own_identity)) server=visible.rows[i].server;
  for(int i=0;i<visible.count;i++) if(visible.rows[i].sid==sid && !strcmp(visible.rows[i].server,server)) return true;
  return false;
}
void ntl_focus_chat(void) { panel=true; focus_chat=true; }
void ntl_lightweight_settings(user_settings* settings) {
  for(int i=0;i<2;i++) {
    gameplay_mode* m=&settings->modes[i];
    m->food_float=false; m->food_flicker=false; m->show_shadows=false;
    m->show_boost=false; m->show_accessories=false; m->death_effect=false;
  }
}
static int float_compare(const void* a,const void* b) { float x=*(const float*)a,y=*(const float*)b; return (x>y)-(x<y); }
static void update_stats(void) {
  double t=now_ms();
  if(last_frame && t-last_frame<2000) { samples[sample_next++]=(float)(t-last_frame); sample_next%=600; if(sample_count<600) sample_count++; }
  last_frame=t;
  if(t-stats_at>1000 && sample_count) {
    float sorted[600],sum=0; slow_frames=0;
    memcpy(sorted,samples,sample_count*sizeof(float));
    for(int i=0;i<sample_count;i++) { sum+=sorted[i]; if(sorted[i]>33.34f) slow_frames++; }
    qsort(sorted,sample_count,sizeof(float),float_compare);
    average=sum/sample_count; p99=sorted[(sample_count-1)*99/100]; stats_at=t;
  }
}
static void read_snapshot(void) {
  if(!TryEnterCriticalSection(&shared.lock)) return;
  active=shared.enabled;
  if(revision!=shared.revision) {
    revision=shared.revision;
    if(!demo) { visible=shared.reply; updated=shared.updated; copy(visible_server,sizeof(visible_server),shared.reply_server); }
    copy(status,sizeof(status),shared.status);
  }
  LeaveCriticalSection(&shared.lock);
  for(int i=0;i<visible.count;i++) {
    ntl_member* m=&visible.rows[i]; const char* old="";
    for(int j=0;j<previous.count;j++) if(!strcmp(m->nick,previous.rows[j].nick)) { old=previous.rows[j].message; break; }
    if(strcmp(m->message,old) && m->message[0]) {
      const char* delta=!strncmp(m->message,old,strlen(old)) ? m->message+strlen(old) : m->message;
      char plain[2048],name[257]; ntl_plain(delta,plain,sizeof(plain)); ntl_plain(m->nick+8,name,sizeof(name)); log_line(name,plain);
    }
  }
  // This copy is performed only when an NTL snapshot changed (see caller).
  previous=visible;
}
static void save_team_keys(void) {
  if(strlen(auth)<16 || strlen(team)<16) {
    copy(key_status,sizeof(key_status),"저장하려면 인증키와 팀 ID를 모두 입력해 주세요");
    return;
  }
  if(credentials_path[0] && credential_store_save(credentials_path,auth,team)) {
    keys_saved=true;
    copy(key_status,sizeof(key_status),"팀키 저장됨 · 다음 실행 때 자동으로 불러옵니다");
  } else {
    copy(key_status,sizeof(key_status),"팀키 저장 실패 · 입력한 키는 저장되지 않았습니다");
  }
}
static void send_chat(void) {
  char encoded[5401];
  if(!ntl_encode_chat(draft,encoded,sizeof(encoded))) { copy(status,sizeof(status),"채팅 내용을 확인해 주세요 (최대 900바이트)"); return; }
  if(demo) { log_line("나 (시험)",draft); draft[0]=0; return; }
  EnterCriticalSection(&shared.lock);
  if(shared.enabled && !shared.request.message[0]) {
    copy(shared.request.message,sizeof(shared.request.message),encoded);
    draft[0]=0; copy(status,sizeof(status),"다음 갱신 때 전송 대기 중");
  }
  LeaveCriticalSection(&shared.lock);
}
static void draw_team_minimap(tenv* env) {
  if(death_review.frozen) return;
  tuser_data* usr=env->usr; game_data* game=&usr->gdata;
  if(game->curr_screen!=PLAYING || game->conn!=CONNECTED ||
     usr->r->global.minimap_opacity<=0 || (!active && !demo)) return;
  if(!demo && (!updated || strcmp(visible_server,usr->usrs.ipv4))) return;
  uint64_t age=demo ? 0 : GetTickCount64()-updated;
  const char* server=demo ? "demo" : usr->usrs.ipv4;
  for(int i=0;i<visible.count;i++)
    if(!strcmp(visible.rows[i].nick,own_identity)) { server=visible.rows[i].server; break; }
  const float* circle=usr->r->global.minimap_circ;
  ImDrawList* draw=igGetBackgroundDrawList(igGetMainViewport());
  const ntl_member* hovered=NULL;
  ImVec2 mouse=igGetIO_Nil()->MousePos;
  for(int i=0;i<visible.count;i++) {
    ntl_member* member=&visible.rows[i]; float point[2];
    if(!team_minimap_point(member,server,own_identity,age,circle[0],circle[1],circle[2],
        game->data.grd,game->data.flux_grd,point)) continue;
    ImVec2 p={point[0],point[1]};
    ImDrawList_AddCircleFilled(draw,p,4.7f,0xff101010,12);
    ImDrawList_AddCircleFilled(draw,p,3.2f,member->sos ? 0xff6363ff : 0xff9ce978,12);
    if(member->sos) ImDrawList_AddCircle(draw,p,6.5f,0xff6363ff,16,1.2f);
    if((mouse.x-p.x)*(mouse.x-p.x)+(mouse.y-p.y)*(mouse.y-p.y)<81) hovered=member;
  }
  if(hovered) {
    char name[257]; ntl_plain(hovered->nick+8,name,sizeof(name));
    ImFont* font=usr->imgui_data.regular_font[FONT_SIZE_SMALL]; igPushFont(font,font->LegacySize);
    igSetTooltip("%s%s\n점수 %d · %llu초 전%s",name,hovered->sos ? " · SOS" : "",hovered->score,
      (unsigned long long)(age/1000),demo ? " · 시험 위치" : "");
    igPopFont();
  }
}
static void draw_chat(tenv* env) {
  float width=fminf(480,env->ctx->size[0]-24),height=fminf(270,env->ctx->size[1]*.38f);
  float bottom=env->usr->imgui_data.mono_font[env->usr->usrs.stats_font_size]->LegacySize*4+24;
  igSetNextWindowPos((ImVec2){12,env->ctx->size[1]-bottom},ImGuiCond_Always,(ImVec2){0,1});
  igSetNextWindowSize((ImVec2){width,height},ImGuiCond_Always);
  igSetNextWindowBgAlpha(.62f);
  if(focus_chat) igSetNextWindowFocus();
  igPushStyleVar_Float(ImGuiStyleVar_WindowRounding,5);
  igPushStyleVar_Vec2(ImGuiStyleVar_WindowPadding,(ImVec2){10,8});
  igBegin("##team_chat",NULL,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoSavedSettings);
  igTextColored((ImVec4){.55f,.94f,.73f,1},"TEAM");
  if(igIsItemHovered(0)) igSetTooltip("%s",status);
  igSameLine(0,10);
  igTextDisabled(demo ? "오프라인 시험" : active ? "팀원 %d명" : "연결 안 됨",visible.count);
  igSameLine(width-120,0);
  if(igSmallButton("설정")) options_open=!options_open;
  igSameLine(0,8); if(igSmallButton("닫기")) panel=false;
  if(igIsItemHovered(0)) igSetTooltip("F8 / H H · 채팅 표시 전환");
  igSeparator();
  bool request_focus=focus_chat;
  igBeginChild_Str("##team_messages",(ImVec2){0,-igGetFrameHeightWithSpacing()},0,0);
  if(!history_count) {
    igTextDisabled(active ? "팀 메시지를 기다리는 중입니다." : "설정에서 팀에 연결할 수 있습니다.");
    igTextDisabled("Enter · 입력 / 전송     Esc · 게임으로 돌아가기");
  }
  for(int i=0;i<history_count;i++) {
    chat_line* line=&history[(history_start+i)%160];
    igTextDisabled("%s",line->time); igSameLine(0,7);
    igTextColored((ImVec4){.55f,.94f,.73f,1},"%s",line->nick);
    igTextWrapped("%s",line->text);
    igDummy((ImVec2){0,3});
  }
  if((scroll_chat && chat_at_bottom) || request_focus) igSetScrollHereY(1);
  scroll_chat=false; chat_at_bottom=igGetScrollY()>=igGetScrollMaxY()-8;
  igEndChild();
  igBeginDisabled(!active && !demo);
  igSetNextItemWidth(-62);
  if(request_focus && (active || demo)) igSetKeyboardFocusHere(0);
  focus_chat=false;
  bool send=igInputTextWithHint("##team_message","팀 채팅 · Enter 전송",draft,sizeof(draft),ImGuiInputTextFlags_EnterReturnsTrue,NULL,NULL);
  bool typing=igIsItemActive();
  igSameLine(0,6); if(igButton("전송",(ImVec2){0})) send=true;
  if(send) { send_chat(); igSetWindowFocus_Str(NULL); }
  if(typing && igIsKeyPressed_Bool(ImGuiKey_Escape,false)) igSetWindowFocus_Str(NULL);
  igEndDisabled();
  capture_mouse|=igIsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
  igEnd(); igPopStyleVar(2);
}
void ntl_frame(tenv* env) {
  tuser_data* usr=env->usr; game_data* game=&usr->gdata;
  update_stats(); capture_mouse=false;
  if(TryEnterCriticalSection(&shared.lock)) {
    bool changed=revision!=shared.revision;
    ntl_request* r=&shared.request;
    snake* me=game->conn==CONNECTED && game->data.follow_view ? get_snake(game,game->data.snake_id) : NULL;
    r->playing=me && !me->dead;
    if(r->playing) {
      r->x=me->xx; r->y=me->yy; r->sid=me->ntl_id; r->score=game->data.score; r->rank=game->data.rank;
      copy(r->server,sizeof(r->server),usr->usrs.ipv4);
    }
    LeaveCriticalSection(&shared.lock);
    if(changed) read_snapshot();
  }
  if(!native_settings_open) draw_team_minimap(env);
  if(native_settings_open) { focus_chat=false; return; }
  ImFont* font=usr->imgui_data.regular_font[FONT_SIZE_SMALL];
  igPushFont(font,font->LegacySize);
  if(usr->usrs.hotkeys[HOTKEY_HUD].active) {
    igSetNextWindowPos((ImVec2){12,12},ImGuiCond_Always,(ImVec2){0});
    igSetNextWindowBgAlpha(0.65f);
    igBegin("성능",NULL,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoSavedSettings);
    igText("%.0f FPS  |  느린 1%% %.1f ms  |  핑 %d ms",average>0 ? 1000/average : 0,p99,game->conn==CONNECTED ? game->data.ping : 0);
    if(igButton(panel ? "F8 · 채팅 닫기" : "F8 · 팀 채팅",(ImVec2){0})) panel=!panel;
    capture_mouse|=igIsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
    igEnd();
  }
  if(panel) draw_chat(env);
  else focus_chat=false;
  if(options_open) {
    igSetNextWindowPos((ImVec2){12,90},ImGuiCond_FirstUseEver,(ImVec2){0});
    igSetNextWindowSize((ImVec2){470,min(760,env->ctx->size[1]-110)},ImGuiCond_Always);
    if(igBegin("팀 / 화면 설정",&options_open,ImGuiWindowFlags_NoDocking)) {
      igTextWrapped("%s",status);
      if(updated) igText("마지막 응답: %llu초 전%s",(GetTickCount64()-updated)/1000,GetTickCount64()-updated>10000 ? " · 위치가 오래됨" : "");
      if(!active && !demo) {
        igInputText("인증키",auth,sizeof(auth),ImGuiInputTextFlags_Password,NULL,NULL);
        igInputText("팀 ID",team,sizeof(team),ImGuiInputTextFlags_Password,NULL,NULL);
        igTextWrapped("저장하면 다음 실행 때 자동으로 불러옵니다. 이 Windows 계정에 암호화해 보관합니다.");
        if(igButton("팀키 저장",(ImVec2){0})) save_team_keys();
        igSameLine(0,-1);
        if(igButton("팀 연결",(ImVec2){0}) && shared.thread) {
          if(strlen(auth)<16 || strlen(team)<16 || !usr->usrs.nickname[0]) copy(status,sizeof(status),"인증키 · 팀 ID · 게임 닉네임이 필요합니다");
          else {
            save_team_keys();
            EnterCriticalSection(&shared.lock);
            copy(shared.request.auth,sizeof(shared.request.auth),auth); copy(shared.request.team,sizeof(shared.request.team),team);
            snprintf(own_identity,sizeof(own_identity),"%s%s",prefix,usr->usrs.nickname);
            copy(shared.request.nick,sizeof(shared.request.nick),own_identity);
            shared.enabled=true; shared.generation++; shared.updated=0; memset(&shared.reply,0,sizeof(shared.reply));
            LeaveCriticalSection(&shared.lock);
            active=true; memset(&visible,0,sizeof(visible)); memset(&previous,0,sizeof(previous)); updated=0; history_count=history_start=0;
            SecureZeroMemory(auth,sizeof(auth)); SecureZeroMemory(team,sizeof(team)); copy(status,sizeof(status),"연결 확인 중"); SetEvent(shared.wake);
          }
        }
        igSameLine(0,-1);
        if(igButton("오프라인 화면 시험",(ImVec2){0})) {
          demo=true; memset(&visible,0,sizeof(visible)); memset(&previous,0,sizeof(previous));
          ntl_parse("[{\"nick\":\"TEST0001팀원\",\"srv\":\"demo\",\"valx\":18000,\"valy\":19000,\"msg\":\"안녕하세요&nbsp;위치&nbsp;시험\"}]","",&visible);
          float c=game->data.grd>0 ? game->data.grd : 21600;
          float r=game->data.flux_grd>0 ? game->data.flux_grd : c;
          visible.rows[0].x=c+r*.3f; visible.rows[0].y=c-r*.2f;
          visible.rows[1]=visible.rows[0]; strcpy(visible.rows[1].nick,"TEST0002도움 요청");
          visible.rows[1].x=c-r*.35f; visible.rows[1].y=c+r*.25f; visible.rows[1].sos=true;
          visible.count=2;
          updated=GetTickCount64(); log_line("팀원","안녕하세요 · 네트워크 전송 없는 화면 시험입니다"); copy(status,sizeof(status),"오프라인 시험 · 실제 팀에 연결하지 않음");
        }
      } else if(igButton(demo ? "시험 종료" : "팀 연결 끊기",(ImVec2){0})) {
        EnterCriticalSection(&shared.lock); shared.enabled=false; shared.generation++; SecureZeroMemory(&shared.request,sizeof(shared.request)); LeaveCriticalSection(&shared.lock);
        active=demo=false; visible.count=previous.count=0; updated=0; history_count=history_start=0; copy(status,sizeof(status),"팀 연결 안 됨"); SetEvent(shared.wake);
        if(keys_saved) credential_store_load(credentials_path,auth,team);
      }
      if(key_status[0]) igTextWrapped("%s",key_status);
      if(keys_saved && igButton("저장된 팀키 삭제",(ImVec2){0})) {
        if(credential_store_delete(credentials_path)) {
          keys_saved=false; SecureZeroMemory(auth,sizeof(auth)); SecureZeroMemory(team,sizeof(team));
          copy(key_status,sizeof(key_status),"저장된 팀키 삭제됨 · 현재 연결은 유지됩니다");
        } else copy(key_status,sizeof(key_status),"팀키 파일을 삭제하지 못했습니다");
      }
      igSeparator();
      if(igCollapsingHeader_TreeNodeFlags("이미지 태그 / cosmetic",0)) personal_tag_controls(env);
      igSeparator();
      if(igCollapsingHeader_TreeNodeFlags("이름 색상",0)) {
        igCheckbox("이름 단일 색 · Ctrl+F11",&native_prefs.uniform_names);
        igBeginDisabled(!native_prefs.uniform_names);
        igSetNextItemWidth(180);
        igColorEdit3("기본 이름 색",native_prefs.name_color,ImGuiColorEditFlags_NoInputs);
        igSetNextItemWidth(180);
        igColorEdit3("팀원 이름 색",native_prefs.team_name_color,ImGuiColorEditFlags_NoInputs);
        igTextWrapped("일반 이름은 기본 색, NTL 팀원의 이름 전체만 팀원 색으로 표시합니다. 팀 연결이 필요하며 설정은 자동 저장됩니다.");
        float* base=native_prefs.name_color; float* ally=native_prefs.team_name_color;
        igTextColored((ImVec4){base[0],base[1],base[2],1},"일반 플레이어 10.0K");
        igTextColored((ImVec4){ally[0],ally[1],ally[2],1},"팀원 닉네임"); igSameLine(0,0);
        igTextColored((ImVec4){base[0],base[1],base[2],1}," 10.0K");
        igEndDisabled();
      }
      igCheckbox("지렁이 외곽선 강조 · Ctrl+F10",&native_prefs.snake_outline);
      igCheckbox("내 머리 앞 기준점 · Ctrl+F12",&native_prefs.head_point);
      igSetItemTooltip("내 머리 앞 중앙을 표시합니다. 서버의 정확한 사망 판정 범위를 나타내지는 않습니다.");
      igCheckbox("사망 순간 화면 정지 · Ctrl+F4",&native_prefs.death_freeze);
      igSetItemTooltip("켜면 사망 알림 직전의 게임 화면을 유지합니다. 자동 재시작보다 우선하며 R로 다시 시작, Esc로 메뉴에 돌아갑니다.");
      igTextWrapped("타겟: Backspace 두 번 → 상대 머리 위 숫자/문자. 선택 중 Delete로 해제합니다. 표적은 내 화면에 표시됩니다.");
      if(!death_review.frozen && game->conn==CONNECTED && game->data.follow_view && igButton("타겟 선택",(ImVec2){0})) {
        target_marker_input(env,true); options_open=false;
      }
      igSeparator();
      const char* choices[]={"60 FPS (기본)","120 FPS","제한 없음"};
      igTextUnformatted(env->ctx->present_mode==VK_PRESENT_MODE_MAILBOX_KHR ? "화면 찢어짐 방지 · 저지연" : "화면 찢어짐 방지 · 동기화",NULL);
      igSetNextItemWidth(200); igCombo_Str_arr("FPS · Ctrl+F5",&cap_choice,choices,3,-1);
      const char* scales[]={"100% · 선명하게","75% · 기본","50% · 가볍게"};
      igSetNextItemWidth(200);
      if(igCombo_Str_arr("해상도 · Ctrl+F6",&scale_choice,scales,3,-1)) {
        const float factors[]={1,0.75f,0.5f}; usr->render_scale=factors[scale_choice];
      }
      if(igButton("가벼운 그래픽 · Ctrl+F7",(ImVec2){0})) ntl_lightweight_settings(&usr->usrs);
      igText("최근 %d프레임 중 33ms 초과: %d",sample_count,slow_frames);
      igPlotLines_FloatPtr("##frametimes",samples,sample_count,sample_count==600 ? sample_next : 0,"프레임 간격 (ms)",0,70,(ImVec2){-1,60},sizeof(float));

      if(igCollapsingHeader_TreeNodeFlags("팀원 목록",0)) for(int i=0;i<visible.count;i++) {
        ntl_member* m=&visible.rows[i]; char name[257]; ntl_plain(m->nick+8,name,sizeof(name));
        igTextWrapped("%s · %d · %s",name,m->score,m->server);
      }
      capture_mouse|=igIsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
    }
    igEnd();
  }
  igPopFont();
}

void ntl_pace(tenv* env) {
  double pace_start=perf_now();
  int limit=caps[cap_choice]; double now=now_ms();
  if(!limit) { next_frame=now; return; }
  double interval=1000.0/limit;
  if(!next_frame || now>next_frame+interval) next_frame=now;
  next_frame+=interval;
  double remaining=next_frame-now;
  // Browser sockets are serviced independently of requestAnimationFrame.
  // Keep our socket and input responsive while maintaining the selected cap.
  while(remaining>1) {
    Sleep((DWORD)fmin(remaining-1,3));
    glfwPollEvents();
    server_service(env);
    remaining=next_frame-now_ms();
  }
  while(now_ms()<next_frame) SwitchToThread();
  perf_add(PERF_PACING,pace_start);
}
