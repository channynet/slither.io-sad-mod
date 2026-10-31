#include "../game/frame_pacer.h"
#include "ntl.h"
#include "../ui/skin_gradient.h"
#include "server.h"
#include "../game/perf_trace.h"
#include "ntl_protocol.h"
#include "team_membership.h"
#include "credential_store.h"
#include "../user.h"
#include "../ui/viewport.h"
#include "../ui/personal_tag.h"
#include "../game/native_preferences.h"
#include "../game/native_shortcuts.h"
#include "../game/death_review.h"
#include "../game/target_marker.h"
#include "../game/ntl_tags.h"
#include "../ui/team_minimap.h"
#include "../ui/team_status.h"
#include "../ui/chat_input.h"
#include "utf8_text.h"
#include "server_address.h"
#include "callback.h"
#include "../ui/toast.h"
#include "../ui/screenshot.h"
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <mmsystem.h>
#else
#include "../platform/compat.h"
#include <curl/curl.h>
#include <stdlib.h>
#ifndef __APPLE__
#include <sys/random.h>
#endif
typedef CURL* HINTERNET;
#endif
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
static team_status_view roster;
static team_membership membership;
static unsigned revision;
static char status[256]="팀 연결 안 됨", auth[513], team[513], prefix[9], draft[901];
static wchar_t credentials_path[32768];
#ifdef _WIN32
static HANDLE pace_timer;               // precise frame-cap naps (see pace_nap)
#endif
static team_profiles profiles;
static tuser_data* team_key_user;          // saved key pairs (NTL "Saved keys"), protected on disk
static char key_name[65];               // name of the pair in the boxes ("team name / key owner")
static char key_status[192];
typedef struct { char nick[257],text[2048],time[9]; } chat_line;
static chat_line history[160];
static char visible_server[161],own_identity[257];
static bool options_open,chat_at_bottom=true;
static int history_count, history_start;
static bool capture_mouse, chat_typing, active, scroll_chat, demo;
static bool sos_on;                     // NTL SOS flag (S S)
static chat_edit chat_editor;
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
  utf8_copy(history[slot].nick,sizeof(history[slot].nick),nick);
  utf8_copy(history[slot].text,sizeof(history[slot].text),text);
  SYSTEMTIME time; GetLocalTime(&time);
  snprintf(history[slot].time,sizeof(history[slot].time),"%02u:%02u:%02u",time.wHour,time.wMinute,time.wSecond);
  scroll_chat=true;
}

#ifdef _WIN32
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
static HINTERNET http_connect(HINTERNET* session) {
  *session=WinHttpOpen(L"SlitherNative/0.1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
  if (*session) WinHttpSetTimeouts(*session,2000,2000,3000,3000);
  return *session ? WinHttpConnect(*session,L"ntl-slither.com",443,0) : NULL;
}
static void http_close(HINTERNET session,HINTERNET connection) {
  if(connection) WinHttpCloseHandle(connection);
  if(session) WinHttpCloseHandle(session);
}
#else
// The same request through libcurl (part of macOS). Redirects and cookies stay off, https only.
typedef struct { char* body; size_t total; DWORD error; } http_sink;
static size_t http_store(char* data,size_t size,size_t count,void* user) {
  http_sink* sink=user; size_t length=size*count;
  if (!sink->body) sink->body=calloc(1048577,1);
  if (!sink->body) { sink->error=ERROR_NOT_ENOUGH_MEMORY; return 0; }
  if (sink->total+length>=1048576) { sink->error=ERROR_INSUFFICIENT_BUFFER; return 0; }
  memcpy(sink->body+sink->total,data,length); sink->total+=length; return length;
}
static int http_progress(void* user,curl_off_t a,curl_off_t b,curl_off_t c,curl_off_t d) {
  (void)user; (void)a; (void)b; (void)c; (void)d;
  return WaitForSingleObject(shared.stop,0)==WAIT_OBJECT_0;           // stop at once when closing
}
static char* fetch(HINTERNET curl, const ntl_request* input, DWORD* error) {
  char* path=calloc(32768,1); char* url=calloc(32800,1);
  http_sink sink={0}; struct curl_slist* headers=NULL;
  *error=ERROR_INVALID_DATA;
  if (!path || !url || !ntl_build_path(input,path,32768)) goto done;
  snprintf(url,32800,"https://ntl-slither.com%s",path);
  headers=curl_slist_append(NULL,"Cache-Control: no-store");
  curl_easy_reset(curl);
  curl_easy_setopt(curl,CURLOPT_URL,url);
#if LIBCURL_VERSION_NUM >= 0x075500
  curl_easy_setopt(curl,CURLOPT_PROTOCOLS_STR,"https");
#else
  curl_easy_setopt(curl,CURLOPT_PROTOCOLS,(long)CURLPROTO_HTTPS);
#endif
  curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
  curl_easy_setopt(curl,CURLOPT_USERAGENT,"SlitherNative/0.1");
  curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
  curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT_MS,2000L);
  curl_easy_setopt(curl,CURLOPT_TIMEOUT_MS,6000L);
  curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,http_store);
  curl_easy_setopt(curl,CURLOPT_WRITEDATA,&sink);
  curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);
  curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,http_progress);
  CURLcode code=curl_easy_perform(curl);
  long status_code=0; curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status_code);
  if (code!=CURLE_OK) {
    *error=sink.error ? sink.error : code==CURLE_OPERATION_TIMEDOUT || code==CURLE_ABORTED_BY_CALLBACK ? ERROR_TIMEOUT : 12000ul+code;
    goto bad_body;
  }
  if (status_code!=200) { *error=(DWORD)status_code; goto bad_body; }
  if (!sink.body) sink.body=calloc(1,1);
  if (!sink.body) { *error=ERROR_NOT_ENOUGH_MEMORY; goto done; }
  if (memchr(sink.body,0,sink.total)) { *error=ERROR_INVALID_DATA; goto bad_body; }
  *error=0; goto done;
bad_body: if (sink.body) { SecureZeroMemory(sink.body,sink.total); free(sink.body); } sink.body=NULL;
done:
  curl_slist_free_all(headers);
  if (path) { SecureZeroMemory(path,32768); free(path); }
  if (url) { SecureZeroMemory(url,32800); free(url); }
  return sink.body;
}
static HINTERNET http_connect(HINTERNET* session) { *session=NULL; return curl_easy_init(); }
static void http_close(HINTERNET session,HINTERNET connection) { (void)session; if (connection) curl_easy_cleanup(connection); }
#endif
static DWORD WINAPI worker(void* unused) {
  (void)unused;
  HINTERNET session=NULL, connection=http_connect(&session);
  ntl_reply* reply=calloc(1,sizeof(*reply));
  HANDLE events[]={shared.stop,shared.wake}; DWORD delay=INFINITE; unsigned failures=0;
  while (WaitForMultipleObjects(2,events,FALSE,delay)!=WAIT_OBJECT_0) {
    ntl_request request; unsigned generation; bool enabled;
    EnterCriticalSection(&shared.lock);
    enabled=shared.enabled; generation=shared.generation; request=shared.request;
    // Consume once, even if the response is lost: never replay a chat automatically.
    SecureZeroMemory(shared.request.message,sizeof(shared.request.message));
    shared.request.target[0]=0;
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
  http_close(session,connection);
  return 0;
}
void ntl_init(void) {
  if(credential_store_path(credentials_path,32768) && credential_store_load_profiles(credentials_path,&profiles) && profiles.count) {
    const team_profile* p=&profiles.items[profiles.selected>=0 ? profiles.selected : 0];
    copy(auth,sizeof(auth),p->auth); copy(team,sizeof(team),p->team); copy(key_name,sizeof(key_name),p->name);
    snprintf(key_status,sizeof(key_status),"저장된 팀키 %d개 · '%s' 선택됨",profiles.count,p->name);
  }
  InitializeCriticalSection(&shared.lock);
  shared.stop=CreateEventW(NULL,TRUE,FALSE,NULL);
  shared.wake=CreateEventW(NULL,FALSE,FALSE,NULL);
  unsigned char bytes[8];
#ifdef _WIN32
  bool random_ok=BCryptGenRandom(NULL,bytes,8,BCRYPT_USE_SYSTEM_PREFERRED_RNG)>=0;
#elif defined(__APPLE__)
  arc4random_buf(bytes,8); bool random_ok=true;
#else
  bool random_ok=getrandom(bytes,8,0)==8;
#endif
  if (random_ok) {
    const char alphabet[]="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    for(int i=0;i<8;i++) prefix[i]=alphabet[bytes[i]%62];
  }
#ifndef _WIN32
  curl_global_init(CURL_GLOBAL_DEFAULT);              // before the worker thread uses curl
#endif
  if(shared.stop && shared.wake && prefix[0]) shared.thread=CreateThread(NULL,0,worker,NULL,0,NULL);
  if(!shared.thread) copy(status,sizeof(status),"NTL 통신 작업을 시작하지 못했습니다");
  QueryPerformanceFrequency(&frequency);
#ifdef _WIN32
  timeBeginPeriod(1);
  pace_timer=CreateWaitableTimerExW(NULL,NULL,0x00000002 /* CREATE_WAITABLE_TIMER_HIGH_RESOLUTION */,TIMER_ALL_ACCESS);
#endif
}
void ntl_destroy(void) {
  if(shared.stop) SetEvent(shared.stop);
  if(shared.thread) { WaitForSingleObject(shared.thread,INFINITE); CloseHandle(shared.thread); }
  if(shared.stop) CloseHandle(shared.stop);
  if(shared.wake) CloseHandle(shared.wake);
  DeleteCriticalSection(&shared.lock);
  SecureZeroMemory(&shared,sizeof(shared)); SecureZeroMemory(auth,sizeof(auth)); SecureZeroMemory(team,sizeof(team));
  SecureZeroMemory(&profiles,sizeof(profiles));
#ifdef _WIN32
  if(pace_timer) { CloseHandle(pace_timer); pace_timer=NULL; }
  timeEndPeriod(1);
#else
  curl_global_cleanup();
#endif
}
bool ntl_mouse_captured(void) { return capture_mouse || native_settings_open; }
bool ntl_chat_typing(void) { return chat_typing && panel && !native_settings_open && !options_open; }
bool ntl_is_teammate(tenv* env,int sid) {
  if(sid<0 || sid>65535 || !active || !updated || GetTickCount64()-updated>10000 || strcmp(visible_server,env->usr->usrs.ipv4)) return false;
  return team_membership_has(&membership,sid);
}
int ntl_teammate_tag(tenv* env,int sid) {
  if(!ntl_is_teammate(env,sid)) return -2;
  for(int i=0;i<visible.count;i++) {
    const ntl_member* m=&visible.rows[i];
    if(m->sid==sid && !strcmp(m->server,env->usr->usrs.ipv4) && strcmp(m->nick,own_identity)) return m->tag;
  }
  return -2;
}
const char* ntl_own_identity(tenv* env) {
  static char identity[257];
  snprintf(identity,sizeof(identity),"%s%s",prefix,env->usr->usrs.nickname);
  return identity;
}
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
static void read_snapshot(tenv* env) {
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
  team_membership_build(&membership,&visible,own_identity,visible_server);
  team_status_build(&roster,&visible,own_identity,visible_server);
  if(active && updated && GetTickCount64()-updated<=10000 && !strcmp(visible_server,env->usr->usrs.ipv4)) {
    const char* current=visible_server;
    for(int i=0;i<visible.count;i++) if(!strcmp(visible.rows[i].nick,own_identity)) current=visible.rows[i].server;
    for(int i=0;i<visible.count;i++)
      if(!strcmp(visible.rows[i].server,current)) target_marker_receive(env,visible.rows[i].target);
  }
  for(int i=0;i<visible.count;i++) {          // a teammate turned SOS on: tell the player
    const ntl_member* m=&visible.rows[i];
    if(!m->sos || !strcmp(m->nick,own_identity) || strlen(m->nick)<=8) continue;
    bool before=false;
    for(int j=0;j<previous.count;j++) if(!strcmp(m->nick,previous.rows[j].nick)) { before=previous.rows[j].sos; break; }
    if(!before) {
      char name[257],text[400]; ntl_plain(m->nick+8,name,sizeof(name));
      bool here=!strcmp(m->server,env->usr->usrs.ipv4);
      snprintf(text,sizeof(text),"SOS · %s%s",name,here ? " (같은 서버)" : m->server[0] ? " (다른 서버)" : "");
      toast_show(text,1,.4f,.35f,5);
    }
  }
  // This copy is performed only when an NTL snapshot changed (see caller).
  previous=visible;
}
static void strip_spaces(char* text) {
  char* out=text; for(char* c=text;*c;c++) if(*c!=' ' && *c!='\t' && *c!='\r' && *c!='\n') *out++=*c; *out=0;
}
static void trim(char* text) {
  size_t n=strlen(text); while(n && text[n-1]==' ') text[--n]=0;
  size_t lead=strspn(text," "); if(lead) memmove(text,text+lead,strlen(text+lead)+1);
}
static void fill_from_profile(int index) {
  if(index<0 || index>=profiles.count) { auth[0]=team[0]=key_name[0]=0; return; }
  copy(auth,sizeof(auth),profiles.items[index].auth); copy(team,sizeof(team),profiles.items[index].team);
  copy(key_name,sizeof(key_name),profiles.items[index].name);
}
static bool write_profiles(void) {
  return credentials_path[0] && credential_store_save_profiles(credentials_path,&profiles);
}
// NTL "Save": the boxes become a saved pair under their name (a pair with that name is replaced).
static bool save_team_keys(void) {
  strip_spaces(auth); strip_spaces(team); trim(key_name);
  if(strlen(auth)<16 || strlen(team)<16) {
    copy(key_status,sizeof(key_status),"저장하려면 인증키와 팀 ID를 모두 입력해 주세요 (붙여넣기 권장)");
    return false;
  }
  if(!strchr(team,'-') && strchr(auth,'-')) {      // pasted the other way round (the team ID has '-')
    char swap[513]; copy(swap,sizeof(swap),auth); copy(auth,sizeof(auth),team); copy(team,sizeof(team),swap);
    SecureZeroMemory(swap,sizeof(swap));
  }
  int index=-1;
  for(int i=0;i<profiles.count;i++) if(key_name[0] && !strcmp(profiles.items[i].name,key_name)) index=i;
  if(index<0) {
    if(profiles.count>=TEAM_PROFILE_MAX) { snprintf(key_status,sizeof(key_status),"팀키는 %d개까지 저장됩니다. 하나를 삭제해 주세요",TEAM_PROFILE_MAX); return false; }
    index=profiles.count++;
    if(!key_name[0]) snprintf(key_name,sizeof(key_name),"팀키 %d",index+1);
  }
  team_profile* p=&profiles.items[index];
  copy(p->name,sizeof(p->name),key_name); copy(p->auth,sizeof(p->auth),auth); copy(p->team,sizeof(p->team),team);
  profiles.selected=index;
  if(write_profiles()) snprintf(key_status,sizeof(key_status),"'%s' 저장됨 · 저장된 팀키 %d개",key_name,profiles.count);
  else copy(key_status,sizeof(key_status),"팀키 저장 실패 · 입력한 키는 저장되지 않았습니다");
  return true;
}
static void delete_team_keys(void) {
  int index=-1;
  for(int i=0;i<profiles.count;i++) if(!strcmp(profiles.items[i].name,key_name)) index=i;
  if(index<0) { copy(key_status,sizeof(key_status),"삭제할 팀키를 목록에서 골라 주세요"); return; }
  char removed[65]; copy(removed,sizeof(removed),profiles.items[index].name);
  memmove(&profiles.items[index],&profiles.items[index+1],(size_t)(profiles.count-index-1)*sizeof(team_profile));
  profiles.count--; SecureZeroMemory(&profiles.items[profiles.count],sizeof(team_profile));
  profiles.selected=profiles.count ? (index<profiles.count ? index : profiles.count-1) : -1;
  fill_from_profile(profiles.selected);
  if(write_profiles()) snprintf(key_status,sizeof(key_status),"'%s' 삭제됨 · 남은 팀키 %d개",removed,profiles.count);
  else copy(key_status,sizeof(key_status),"팀키 파일을 고치지 못했습니다");
}
static void choose_profile(int index) {
  fill_from_profile(index);
  if(index>=0 && index<profiles.count && profiles.selected!=index) { profiles.selected=index; write_profiles(); }
  if(index>=0 && index<profiles.count) snprintf(key_status,sizeof(key_status),"'%s' 선택됨",profiles.items[index].name);
}
static void connect_team(tuser_data* usr) {
  if(!shared.thread) return;
  strip_spaces(auth); strip_spaces(team);
  if(strlen(auth)<16 || strlen(team)<16 || !usr->usrs.nickname[0]) { copy(status,sizeof(status),"인증키 · 팀 ID · 게임 닉네임이 필요합니다"); return; }
  save_team_keys();
  EnterCriticalSection(&shared.lock);
  copy(shared.request.auth,sizeof(shared.request.auth),auth); copy(shared.request.team,sizeof(shared.request.team),team);
  snprintf(own_identity,sizeof(own_identity),"%s%s",prefix,usr->usrs.nickname);
  copy(shared.request.nick,sizeof(shared.request.nick),own_identity);
  shared.enabled=true; shared.generation++; shared.updated=0; memset(&shared.reply,0,sizeof(shared.reply));
  LeaveCriticalSection(&shared.lock);
  active=true; memset(&visible,0,sizeof(visible)); memset(&previous,0,sizeof(previous)); memset(&roster,0,sizeof(roster)); updated=0; history_count=history_start=0;
  SecureZeroMemory(auth,sizeof(auth)); SecureZeroMemory(team,sizeof(team)); copy(status,sizeof(status),"연결 확인 중"); SetEvent(shared.wake);
}
static void disconnect_team(void) {
  EnterCriticalSection(&shared.lock); shared.enabled=false; shared.generation++; SecureZeroMemory(&shared.request,sizeof(shared.request)); LeaveCriticalSection(&shared.lock);
  target_marker_clear_team(); active=demo=sos_on=false; visible.count=previous.count=0; memset(&roster,0,sizeof(roster)); updated=0; history_count=history_start=0; copy(status,sizeof(status),"팀 연결 안 됨"); SetEvent(shared.wake);
  fill_from_profile(profiles.selected);
}
static void team_key_list(const char* label) {
  const char* preview=profiles.selected>=0 && profiles.selected<profiles.count ? profiles.items[profiles.selected].name : "저장된 팀키 없음";
  igSetNextItemWidth(260);
  if(igBeginCombo(label,preview,0)) {
    for(int i=0;i<profiles.count;i++) {
      igPushID_Int(i);
      if(igSelectable_Bool(profiles.items[i].name,i==profiles.selected,0,(ImVec2){0})) {
        if(active && !demo) { disconnect_team(); choose_profile(i); connect_team(team_key_user); }
        else choose_profile(i);
      }
      igPopID();
    }
    if(!active && igSelectable_Bool("+ 새 팀키 입력",false,0,(ImVec2){0})) {
      auth[0]=team[0]=key_name[0]=0; copy(key_status,sizeof(key_status),"이름 · 인증키 · 팀 ID를 넣고 저장하세요");
    }
    if(!profiles.count) igTextDisabled("저장하면 여기서 고를 수 있습니다");
    igEndCombo();
  }
}
static void send_chat(void) {
  char encoded[5401];
  if(!ntl_encode_chat(draft,encoded,sizeof(encoded))) { copy(status,sizeof(status),"채팅 내용을 확인해 주세요 (최대 900바이트)"); return; }
  if(demo) { log_line("나 (시험)",draft); draft[0]=0; return; }
  EnterCriticalSection(&shared.lock);
  if(shared.enabled) {
    size_t pending=strlen(shared.request.message),length=strlen(encoded);
    if(pending+length+(pending?4:0)<sizeof(shared.request.message)) {
      if(pending) strcat(shared.request.message,"<br>");
      strcat(shared.request.message,encoded); draft[0]=0;
      copy(status,sizeof(status),"채팅 전송 중"); SetEvent(shared.wake);
    } else copy(status,sizeof(status),"이전 메시지 전송을 기다려 주세요.");
  }
  LeaveCriticalSection(&shared.lock);
}
static void queue_message(const char* text) {
  char encoded[5401]; if(!ntl_encode_chat(text,encoded,sizeof(encoded))) return;
  EnterCriticalSection(&shared.lock);
  if(shared.enabled) {
    size_t pending=strlen(shared.request.message),length=strlen(encoded);
    if(pending+length+(pending?4:0)<sizeof(shared.request.message)) {
      if(pending) strcat(shared.request.message,"<br>");
      strcat(shared.request.message,encoded); SetEvent(shared.wake);
    }
  }
  LeaveCriticalSection(&shared.lock);
}
bool ntl_sos(void) { return sos_on; }
void ntl_clear_chat(void) { history_count=history_start=0; chat_at_bottom=true; }
void ntl_toggle_sos(void) {
  if(!active && !demo) { toast_show("SOS는 팀에 연결된 뒤 보낼 수 있습니다 (팀 / 화면 설정)",1,.75f,.4f,3); return; }
  sos_on=!sos_on;
  if(sos_on && native_prefs.sos_chat) queue_message("Help me!");     // NTL sends the same line
  toast_show(sos_on ? "SOS 켜짐 · 팀원에게 빨간색으로 보입니다 (S S로 끄기)" : "SOS 꺼짐",sos_on ? 1 : .8f,sos_on ? .4f : .8f,sos_on ? .35f : .8f,2.5);
  EnterCriticalSection(&shared.lock); SetEvent(shared.wake); LeaveCriticalSection(&shared.lock);
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
static bool fresh_roster(void) {
  return !demo && active && updated && GetTickCount64()-updated<=10000;
}
// A teammate's server as an address the client can connect to (NTL sends "IP:port" or a name).
static bool joinable(const char* server,char out[MAX_IPV4_LEN+1]) {
  if(!server[0] || !strcmp(server,"_GAME_MENU_") || strlen(server)>MAX_IPV4_LEN) return false;
  for(const char* c=server;*c;c++) if((unsigned char)*c<=' ') return false;
  if(server_address_clean(server,out)) return true;
  if(!strchr(server,'.')) return false;
  snprintf(out,MAX_IPV4_LEN+1,"%s",server); return true;
}
bool ntl_join_server(tenv* env,const char* address) {
  game_data* game=&env->usr->gdata; user_settings* settings=&env->usr->usrs;
  char clean[MAX_IPV4_LEN+1];
  if(!joinable(address,clean) || game->conn==CONNECTING) return false;
  if(game->conn==CONNECTED && game->connection) {
    // Same path as the restart key: the game closes, then connects again to the new address.
    server_note_end("join teammate's server %s",clean);
    snprintf(settings->ipv4,sizeof(settings->ipv4),"%s",clean);
    game->connection->is_closing=true;
    game->restart_req=true;
  } else {
    snprintf(settings->ipv4,sizeof(settings->ipv4),"%s",clean);
    game->conn=CONNECTING; game->curr_screen=PLAYING;
    glfwSetTime(0);
    server_connect(env);
  }
  return true;
}
int ntl_team_servers(ntl_team_server* out,int capacity) {
  if(!fresh_roster() || capacity<=0) return 0;
  int n=0;
  for(int i=0;i<roster.count;i++) {
    const team_status_row* row=&roster.rows[i];
    char address[MAX_IPV4_LEN+1];
    if(row->self || row->menu || !joinable(row->server,address)) continue;
    int k=0; while(k<n && strcmp(out[k].address,address)) k++;
    if(k==n) { if(n==capacity) continue; memset(&out[n],0,sizeof(out[n])); snprintf(out[n].address,sizeof(out[n].address),"%s",address); n++; }
    size_t used=strlen(out[k].names);
    if(used+strlen(row->name)+3<sizeof(out[k].names)) snprintf(out[k].names+used,sizeof(out[k].names)-used,"%s%s",used?", ":"",row->name);
    out[k].count++;
  }
  for(int i=1;i<n;i++) for(int j=i;j>0 && out[j].count>out[j-1].count;j--) { ntl_team_server t=out[j]; out[j]=out[j-1]; out[j-1]=t; }
  return n;
}
// FPS and ping at the bottom centre, above the safe-wall label while playing.
static __attribute__((unused)) void draw_net_stats(tenv* env) {
  if(!native_prefs.net_stats) return;
  game_data* game=&env->usr->gdata;
  bool playing=game->curr_screen==PLAYING && game->conn==CONNECTED;
  int fps=average>0 ? (int)lroundf(1000/average) : 0;
  int ping=playing && game->data.ping>0 ? game->data.ping : -1;
  char fps_text[24],ping_text[32];
  snprintf(fps_text,sizeof(fps_text),"%d FPS",fps);
  if(ping>=0) snprintf(ping_text,sizeof(ping_text),"핑 %d ms",ping); else snprintf(ping_text,sizeof(ping_text),"핑 —");
  ImVec4 fps_color=fps>=55?(ImVec4){.55f,.95f,.6f,.95f}:fps>=30?(ImVec4){1,.85f,.35f,.95f}:(ImVec4){1,.45f,.4f,.95f};
  ImVec4 ping_color=ping<0?(ImVec4){1,1,1,.45f}:ping<80?(ImVec4){.55f,.95f,.6f,.95f}:ping<150?(ImVec4){1,.85f,.35f,.95f}:(ImVec4){1,.45f,.4f,.95f};
  ImFont* font=env->usr->imgui_data.regular_font[FONT_SIZE_SMALL];
  float size=font->LegacySize; ImVec2 a,b; const char* gap="   ";
  igPushFont(font,size);
  igCalcTextSize(&a,fps_text,NULL,false,-1); igCalcTextSize(&b,ping_text,NULL,false,-1);
  ImVec2 g; igCalcTextSize(&g,gap,NULL,false,-1);
  igPopFont();
  float width=a.x+g.x+b.x, x=env->ctx->size[0]/2.0f-width/2;
  float y=env->ctx->size[1]-10-a.y-(playing ? a.y+6 : 0);
  ImDrawList* draw=igGetForegroundDrawList_ViewportPtr(NULL);
  ImU32 shadow=igColorConvertFloat4ToU32((ImVec4){0,0,0,.7f});
  ImDrawList_AddText_FontPtr(draw,font,size,(ImVec2){x+1,y+1},shadow,fps_text,NULL,0,NULL);
  ImDrawList_AddText_FontPtr(draw,font,size,(ImVec2){x,y},igColorConvertFloat4ToU32(fps_color),fps_text,NULL,0,NULL);
  float px=x+a.x+g.x;
  ImDrawList_AddText_FontPtr(draw,font,size,(ImVec2){px+1,y+1},shadow,ping_text,NULL,0,NULL);
  ImDrawList_AddText_FontPtr(draw,font,size,(ImVec2){px,y},igColorConvertFloat4ToU32(ping_color),ping_text,NULL,0,NULL);
}
static void draw_team_status(tenv* env) {
  if(!native_prefs.team_status) return;
  igSetNextWindowPos((ImVec2){12,100},ImGuiCond_FirstUseEver,(ImVec2){0});
  igSetNextWindowSize((ImVec2){fminf(650,env->ctx->size[0]-24),250},ImGuiCond_FirstUseEver);
  igSetNextWindowSizeConstraints((ImVec2){320,120},(ImVec2){fmaxf(320,env->ctx->size[0]-24),fmaxf(120,env->ctx->size[1]-48)},NULL,NULL);
  igSetNextWindowBgAlpha(.76f);
  bool open=igBegin("팀원 현황",&native_prefs.team_status,ImGuiWindowFlags_NoDocking);
  if(open) {
    bool stale=!demo && (!active || !updated || GetTickCount64()-updated>10000);
    if(demo) igTextColored((ImVec4){1,.8f,.35f,1},"오프라인 예시 · 실제 팀원/수치가 아닙니다");
    else if(!active) igTextDisabled("팀 연결 안 됨 · F8 → 설정에서 연결하세요.");
    else if(!updated) igTextDisabled("팀원 정보를 기다리는 중입니다.");
    else igTextColored(stale?(ImVec4){1,.65f,.3f,1}:(ImVec4){.6f,.9f,.75f,1},
      "%d명 · 게임 중 %d명 · 서버 %d개 · %llu초 전 수신%s",roster.count,roster.playing,roster.servers,
      (unsigned long long)((GetTickCount64()-updated)/1000),stale?" · 오래된 정보":"");
    if(!active && !demo && igSmallButton("팀 연결 설정")) options_open=true;
    if(igBeginTable("team-status-table",5,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollX|ImGuiTableFlags_ScrollY,
        (ImVec2){0,-igGetTextLineHeightWithSpacing()},0)) {
      igTableSetupColumn("팀원",ImGuiTableColumnFlags_WidthFixed,130,0);
      igTableSetupColumn("점수",ImGuiTableColumnFlags_WidthFixed,65,0);
      igTableSetupColumn("FPS",ImGuiTableColumnFlags_WidthFixed,50,0);
      igTableSetupColumn("핑 (ms)",ImGuiTableColumnFlags_WidthFixed,70,0);
      igTableSetupColumn("참가 서버 / 상태",ImGuiTableColumnFlags_WidthStretch,1,0);
      igTableSetupScrollFreeze(0,1); igTableHeadersRow();
      int count=(active||demo)?roster.count:0;
      bool same_context=demo || (env->usr->gdata.conn==CONNECTED && !strcmp(visible_server,env->usr->usrs.ipv4));
      for(int i=0;i<count;i++) {
        const team_status_row* row=&roster.rows[i];
        igTableNextRow(0,0); igTableSetColumnIndex(0);
        ImVec4 color=stale?(ImVec4){.6f,.6f,.6f,1}:row->sos?(ImVec4){1,.45f,.4f,1}:row->same_server&&same_context?(ImVec4){.55f,.94f,.73f,1}:(ImVec4){.9f,.9f,.9f,1};
        igTextColored(color,"%s%s%s",row->name,row->self?" (나)":"",row->sos?" · SOS":"");
        if(igIsItemHovered(0)) igSetTooltip("%s%s",row->name,row->same_server&&same_context?" · 같은 게임 서버":"");
        igTableSetColumnIndex(1); igText("%d",row->score);
        igTableSetColumnIndex(2); if(!stale && row->fps>=0) igText("%d",row->fps); else igTextDisabled("—");
        igTableSetColumnIndex(3); if(!stale && row->ping>=0) igText("%d",row->ping); else igTextDisabled("—");
        igTableSetColumnIndex(4);
        char address[MAX_IPV4_LEN+1];
        bool here=env->usr->gdata.conn==CONNECTED && joinable(row->server,address) && !strcmp(address,env->usr->usrs.ipv4);
        if(!demo && !stale && !row->self && !row->menu && !here && joinable(row->server,address)) {
          igPushID_Int(i);
          igBeginDisabled(env->usr->gdata.conn==CONNECTING);
          if(igSmallButton("참가")) ntl_join_server(env,row->server);
          igEndDisabled();
          if(igIsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            igSetTooltip(env->usr->gdata.conn==CONNECTED ? "지금 게임을 나가고 %s 서버로 들어갑니다" : "%s 서버로 들어갑니다",address);
          igPopID();
          igSameLine(0,-1);
        }
        igTextColored(color,"%s%s",row->menu?"메뉴 / 대기":row->server,here&&!row->self?" · 같은 서버":"");
        if(igIsItemHovered(0)) igSetTooltip("%s",row->menu?"게임 참가 전 또는 메뉴 화면":row->server);
      }
      igEndTable();
    }
    igTextDisabled("— 정보 없음/오래됨 · 초록: 같은 서버 · 참가: 그 팀원의 서버로 이동 · 창 이동/크기 조절 가능");
  }
  capture_mouse|=igIsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
  igEnd();
}
// Colored spans share a line and wrap back to the full left edge, as in NTL.
static void draw_chat_line(ImFont* font,const chat_line* line) {
  ImVec2 origin,space; igGetCursorScreenPos(&origin); igGetContentRegionAvail(&space);
  float width=fmaxf(30,space.x),x=0,y=0,size=igGetFontSize(),line_height=size+3;
  const char* spans[]={line->time," ",line->nick," ",line->text};
  const ImU32 colors[]={0xffff901e,0xffffffff,0xe60000ff,0xffffffff,0xe6ffff00};
  ImDrawList* draw=igGetWindowDrawList();
  for(int i=0;i<5;i++) {
    const char* begin=spans[i]; const char* end=begin+strlen(begin);
    while(begin<end) {
      if(*begin=='\n') { x=0; y+=line_height; begin++; continue; }
      if(width-x<1) { x=0; y+=line_height; }
      const char* stop=ImFont_CalcWordWrapPosition(font,size,begin,end,width-x);
      const char* newline=memchr(begin,'\n',end-begin);
      if(newline && stop>newline) stop=newline;
      if(stop<=begin) {
        if(x>0) { x=0; y+=line_height; continue; }
        stop=begin+1; while(stop<end && ((unsigned char)*stop&0xc0)==0x80) stop++;
      }
      ImVec2 measured; igCalcTextSize(&measured,begin,stop,false,-1);
      if(x>0 && measured.x>width-x) { x=0; y+=line_height; continue; }
      ImDrawList_AddText_FontPtr(draw,font,size,(ImVec2){origin.x+x,origin.y+y},colors[i],begin,stop,0,NULL);
      x+=measured.x; begin=stop;
      if(begin<end && *begin!='\n') { x=0; y+=line_height; while(begin<end && *begin==' ') begin++; }
    }
  }
  igDummy((ImVec2){width,y+line_height});
  if(igBeginPopupContextItem("copy-message",ImGuiPopupFlags_MouseButtonRight)) {
    if(igMenuItem_Bool("메시지 복사",NULL,false,true)) igSetClipboardText(line->text);
    if(igMenuItem_Bool("닉네임과 메시지 복사",NULL,false,true)) {
      char text[2400]; snprintf(text,sizeof(text),"%s %s %s",line->time,line->nick,line->text); igSetClipboardText(text);
    }
    igEndPopup();
  }
}
static void draw_chat(tenv* env) {
  float width=fminf(280,env->ctx->size[0]-16),height=fminf(230,env->ctx->size[1]*.4f);
  igSetNextWindowPos((ImVec2){4,fminf(220,env->ctx->size[1]-height-40)},ImGuiCond_FirstUseEver,(ImVec2){0});
  igSetNextWindowSize((ImVec2){width,height},ImGuiCond_FirstUseEver);
  igSetNextWindowSizeConstraints((ImVec2){220,100},(ImVec2){fmaxf(220,env->ctx->size[0]-8),fmaxf(100,env->ctx->size[1]-40)},NULL,NULL);
  igSetNextWindowBgAlpha(.12f);
  ImFont* font=env->usr->imgui_data.regular_font[FONT_SIZE_SMALL]; igPushFont(font,18);
  igPushStyleVar_Float(ImGuiStyleVar_WindowRounding,0);
  igPushStyleVar_Vec2(ImGuiStyleVar_WindowPadding,(ImVec2){2,2});
  igPushStyleVar_Vec2(ImGuiStyleVar_ItemSpacing,(ImVec2){2,0});
  igBegin("##ntl_chat",NULL,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoDocking);
  igBeginChild_Str("##team_messages",(ImVec2){0,-igGetFrameHeight()},0,0);
  if(!history_count) {
    igTextColored((ImVec4){.12f,.56f,1,1},"# %s",active?"팀 메시지를 기다리는 중입니다.":"팀 연결 안 됨 · 아래 …에서 설정");
    igTextDisabled("입력창 클릭 · Enter 전송 · Esc 복귀");
  }
  for(int i=0;i<history_count;i++) {
    int slot=(history_start+i)%160;
    igPushID_Int(slot); draw_chat_line(font,&history[slot]); igPopID();
  }
  if(scroll_chat && chat_at_bottom) igSetScrollHereY(1);
  scroll_chat=false; chat_at_bottom=igGetScrollY()>=igGetScrollMaxY()-8;
  igEndChild();
  igBeginDisabled(!active && !demo);
  igSetNextItemWidth(-28);
  bool typing=false;
  bool send=chat_input(&chat_editor,draft,sizeof(draft),&typing);
  chat_typing=typing;
  if(send) { send_chat(); if(!draft[0]) igSetWindowFocus_Str(NULL); }
  if(typing && igIsKeyPressed_Bool(ImGuiKey_Escape,false)) igSetWindowFocus_Str(NULL);
  igEndDisabled();
  igSameLine(0,2);
  if(igButton("…",(ImVec2){26,0})) igOpenPopup_Str("chat-options",0);
  if(igIsItemHovered(0)) igSetTooltip("%s",status);
  if(igBeginPopup("chat-options",0)) {
    igTextWrapped("%s",status);
    if(igMenuItem_Bool("팀 연결 / 설정",NULL,false,true)) options_open=true;
    if(igMenuItem_Bool("팀원 현황",NULL,native_prefs.team_status,true)) native_prefs.team_status=!native_prefs.team_status;
    if(igMenuItem_Bool("채팅 숨기기","F8 / H H",false,true)) panel=false;
    igEndPopup();
  }
  capture_mouse|=typing || igIsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
  igEnd(); igPopStyleVar(3); igPopFont();
}

void ntl_frame(tenv* env) {
  tuser_data* usr=env->usr; game_data* game=&usr->gdata;
  update_stats(); capture_mouse=false; chat_typing=false;
  if(TryEnterCriticalSection(&shared.lock)) {
    bool changed=revision!=shared.revision;
    ntl_request* r=&shared.request;
    snake* me=game->conn==CONNECTED && game->data.follow_view ? get_snake(game,game->data.snake_id) : NULL;
    r->playing=me && !me->dead;
    r->fps=average>0 ? (int)fminf(10000,roundf(1000/average)) : -1;
    r->ping=r->playing && game->data.ping>0 ? game->data.ping : -1;
    {                                                // NTL: highest ping of the last 10 s
      static ULONGLONG peak_since; static int peak;
      ULONGLONG now=GetTickCount64();
      if(r->ping<0) peak=-1;
      else if(peak<0 || now-peak_since>10000) { peak=r->ping; peak_since=now; }
      else if(r->ping>peak) peak=r->ping;
      r->ping_peak=peak;
    }
    r->sos=sos_on; r->bot=usr->usrs.hotkeys[HOTKEY_BOT].active;
    if(r->playing) {
      r->x=me->xx; r->y=me->yy; r->sid=me->ntl_id; r->score=game->data.score; r->rank=game->data.rank; r->tag=ntl_tag_own();
      copy(r->server,sizeof(r->server),usr->usrs.ipv4);
      if(shared.enabled && !r->target[0]) target_marker_report(env,r->target,sizeof(r->target));
    }
    else r->target[0]=0;
    LeaveCriticalSection(&shared.lock);
    if(changed) read_snapshot(env);
  }
  if(!native_settings_open) draw_team_minimap(env);
  if(sos_on && game->curr_screen==PLAYING) {      // own SOS: a red badge at the top
    ImFont* f=usr->imgui_data.regular_font[FONT_SIZE_SMALL]; const char* t="SOS 보내는 중 · S S로 끄기";
    ImVec2 ts; igPushFont(f,f->LegacySize); igCalcTextSize(&ts,t,NULL,false,-1); igPopFont();
    float x=env->ctx->size[0]/2.0f-ts.x/2, y=14;
    ImDrawList* d=igGetForegroundDrawList_ViewportPtr(NULL);
    float pulse=.75f+.25f*sinf((float)glfwGetTime()*5);
    ImDrawList_AddRectFilled(d,(ImVec2){x-10,y-4},(ImVec2){x+ts.x+10,y+ts.y+4},igColorConvertFloat4ToU32((ImVec4){.8f,.1f,.1f,.85f*pulse}),6,0);
    ImDrawList_AddText_FontPtr(d,f,f->LegacySize,(ImVec2){x,y},0xffffffff,t,NULL,0,NULL);
  }
  toast_draw(env);
  screenshot_frame(env);
  if(native_settings_open) return;
  ImFont* font=usr->imgui_data.regular_font[FONT_SIZE_SMALL];
  igPushFont(font,font->LegacySize);
  draw_team_status(env);
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
  if(options_open) {
    igSetNextWindowPos((ImVec2){12,90},ImGuiCond_FirstUseEver,(ImVec2){0});
    igSetNextWindowSize((ImVec2){470,min(760,env->ctx->size[1]-110)},ImGuiCond_Always);
    if(igBegin("팀 / 화면 설정",&options_open,ImGuiWindowFlags_NoDocking)) {
      igTextWrapped("%s",status);
      igCheckbox("팀원 현황 표시",&native_prefs.team_status);
      if(updated) igText("마지막 응답: %llu초 전%s",(GetTickCount64()-updated)/1000,GetTickCount64()-updated>10000 ? " · 위치가 오래됨" : "");
      team_key_user=usr;
      if(!active && !demo) {
        igSeparatorText("팀키 (NTL처럼 여러 개 저장)");
        team_key_list("저장된 팀키");
        igSetNextItemWidth(260);
        igInputTextWithHint("이름","예: 팀 이름 / 키 주인",key_name,sizeof(key_name),0,NULL,NULL);
        igSetNextItemWidth(260);
        igInputText("인증키",auth,sizeof(auth),ImGuiInputTextFlags_Password,NULL,NULL);
        igSetNextItemWidth(260);
        igInputText("팀 ID",team,sizeof(team),ImGuiInputTextFlags_Password,NULL,NULL);
#ifdef _WIN32
        igTextWrapped("이 Windows 계정에 암호화해 보관합니다. 같은 이름으로 저장하면 그 팀키를 바꿉니다.");
#else
        igTextWrapped("이 사용자 계정만 읽을 수 있는 파일에 보관합니다. 같은 이름으로 저장하면 그 팀키를 바꿉니다.");
#endif
        if(igButton("저장",(ImVec2){0})) save_team_keys();
        igSameLine(0,-1);
        igBeginDisabled(!profiles.count);
        if(igButton("삭제",(ImVec2){0})) delete_team_keys();
        igEndDisabled();
        igSameLine(0,-1);
        if(igButton("팀 연결",(ImVec2){0})) connect_team(usr);
        igSameLine(0,-1);
        if(igButton("오프라인 화면 시험",(ImVec2){0})) {
          demo=true; memset(&visible,0,sizeof(visible)); memset(&previous,0,sizeof(previous));
          ntl_parse("[{\"nick\":\"TEST0001팀원\",\"srv\":\"demo\",\"valx\":18000,\"valy\":19000,\"dt\":\"FPS: 60 @ 42(70) ms\",\"msg\":\"안녕하세요&nbsp;위치&nbsp;시험\"}]","",&visible);
          float c=game->data.grd>0 ? game->data.grd : 21600;
          float r=game->data.flux_grd>0 ? game->data.flux_grd : c;
          visible.rows[0].x=c+r*.3f; visible.rows[0].y=c-r*.2f;
          visible.rows[1]=visible.rows[0]; strcpy(visible.rows[1].nick,"TEST0002도움 요청");
          visible.rows[1].x=c-r*.35f; visible.rows[1].y=c+r*.25f; visible.rows[1].sos=true;
          visible.rows[1].fps=120; visible.rows[1].ping=85;
          visible.rows[2]=visible.rows[0]; strcpy(visible.rows[2].nick,"TEST0003다른 서버"); strcpy(visible.rows[2].server,"example.invalid:444");
          visible.rows[2].fps=75; visible.rows[2].ping=160;
          visible.rows[3]=visible.rows[0]; strcpy(visible.rows[3].nick,"TEST0004대기 중"); strcpy(visible.rows[3].server,"_GAME_MENU_");
          visible.rows[3].fps=visible.rows[3].ping=-1;
          visible.count=4; team_status_build(&roster,&visible,"","demo"); native_prefs.team_status=true;
          updated=GetTickCount64(); log_line("팀원","안녕하세요 · 네트워크 전송 없는 화면 시험입니다"); copy(status,sizeof(status),"오프라인 시험 · 실제 팀에 연결하지 않음");
        }
      } else {
        if(!demo && profiles.selected>=0 && profiles.selected<profiles.count) {
          igText("연결된 팀키: %s",profiles.items[profiles.selected].name);
          if(profiles.count>1) { team_key_list("다른 팀키로 바꾸기"); igSetItemTooltip("고르면 지금 팀 연결을 끊고 그 팀키로 다시 연결합니다."); }
        }
        if(igButton(demo ? "시험 종료" : "팀 연결 끊기",(ImVec2){0})) disconnect_team();
      }
      if(key_status[0]) igTextWrapped("%s",key_status);
      igSeparator();
      if(igCollapsingHeader_TreeNodeFlags("이미지 태그 / cosmetic",0)) personal_tag_controls(env);
      if(igCollapsingHeader_TreeNodeFlags("불꽃 스킨",0)) fire_skin_controls();
      igSeparator();
      if(igCollapsingHeader_TreeNodeFlags("이름 색상",0)) {
        igCheckbox("팀원 닉네임 강조(팀 색 + 반짝임) · Ctrl+F3",&native_prefs.team_name_emphasis);
        igSetItemTooltip("NTL 팀원 이름에 어두운 배경과 팀 색 테두리·표식을 표시합니다. 점수와 상대 이름은 그대로입니다.");
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
      igSeparatorText("안전 벽 (Safewall)");
      igCheckbox("안전 벽",&native_prefs.safe_wall);
      igSetItemTooltip("NTL의 Safewall과 같은 방식입니다. 벽 가까이에서 마우스가 맵 밖을 가리키면\n벽 안쪽의 안전한 원 위의 점으로 방향을 돌려 벽을 따라 미끄러지게 합니다.\n마우스를 맵 안쪽으로 두거나, 정확히 바깥쪽(±9도)을 가리키면 마우스대로 움직입니다.\n봇(T)이 켜져 있으면 봇이 벽을 피합니다. 벽을 따라 돌리는 동안 아래 표시가 주황색이 됩니다.");
      igBeginDisabled(!native_prefs.safe_wall);
      igText("방식"); igSameLine(0,-1);
      if(igRadioButton_Bool("1 · 마우스 위치",native_prefs.safe_wall_script==1)) native_prefs.safe_wall_script=1;
      igSetItemTooltip("NTL 방식 1: 머리에서 마우스 쪽 0.78 거리의 점이 벽 밖이면, 벽 원 위의 가장 가까운 점으로 갑니다.\n벽 50 안쪽에서만 동작합니다.");
      igSameLine(0,-1);
      if(igRadioButton_Bool("2 · 속도·각도·굵기 (NTL 기본)",native_prefs.safe_wall_script==2)) native_prefs.safe_wall_script=2;
      igSetItemTooltip("NTL 방식 2 (기본): 속도·회전 반경·벽을 향한 각도로 미리 돌기 시작하고,\n앞쪽 일정 거리에서 안전 원과 만나는 점 중 마우스에 가까운 쪽으로 갑니다. 벽 500 안쪽에서 동작합니다.");
      igCheckbox("벽과 거리 직접 정하기",&native_prefs.safe_wall_tight);
      igSetItemTooltip("켜면 아래 거리만큼 벽 안쪽으로 붙여 탑니다(두 방식 모두). 끄면 NTL의 벽 오프셋을 씁니다.");
      igBeginDisabled(!native_prefs.safe_wall_tight);
      igSetNextItemWidth(200);
      igSliderFloat("벽과 거리",&native_prefs.safe_wall_distance,0,20,"%.1f",ImGuiSliderFlags_AlwaysClamp);
      igSetItemTooltip("머리 중심이 벽선에서 이만큼 안쪽을 따라갑니다 (기본 1 = 완전 밀착, 몸이 벽선에 겹쳐 보임).\n0에 가까울수록 붙고, 핑이 높아 벽에 죽으면 올리세요. NTL 오프셋 -10은 3, 0은 7, 10은 11과 비슷합니다.\nCtrl+클릭하면 숫자를 직접 입력할 수 있습니다.");
      igEndDisabled();
      igBeginDisabled(native_prefs.safe_wall_tight);
      igSetNextItemWidth(160);
      igSliderInt("벽 오프셋",&native_prefs.safe_wall_offset,-10,10,"%d",ImGuiSliderFlags_AlwaysClamp);
      igEndDisabled();
      igSetItemTooltip("NTL의 Border assist offset과 같습니다 (기본 0, 방식 2에 적용).\n벽에 자주 죽으면 올려서 조금 더 떨어지게, 핑이 아주 낮으면 내려서 더 붙게 하세요.\n-10이면 벽에서 3, 0이면 7, 10이면 11만큼 안쪽을 지나갑니다.");
      igEndDisabled();
      igSeparatorText("몸 붙기 (E 누르는 동안)");
      igSetNextItemWidth(200);
      igSliderFloat("히트박스까지 거리",&native_prefs.hug_gap,0,30,"%.1f",ImGuiSliderFlags_AlwaysClamp);
      igSetItemTooltip("E를 누르고 있는 동안 가장 가까운 적(팀원 제외)의 몸을 따라, 지금 가는 방향으로 히트박스에 붙어서 갑니다.\n죽는 판정은 내 머리 앞쪽 끝(코)이 상대 몸 반지름 안에 들어갈 때뿐이라, 옆으로 나란히 갈 때는\n머리 중심을 상대 몸 반지름 바로 바깥에 두어 화면에서는 몸이 겹쳐 보입니다.\n이 값은 코와 상대 히트박스 사이 거리입니다 (기본 1). 꼬리 끝에서는 돌아서 반대쪽으로 붙고,\n머리 쪽 끝에서는 머리 옆을 따라 계속 갑니다. 벽 가까이에서는 안전 벽이 우선합니다. 봇(T)이 켜져 있으면 쓰지 않습니다.\nCtrl+클릭으로 숫자를 직접 넣을 수 있습니다.");
      igSeparatorText("벽 상태");
      igCheckbox("벽 색으로 상태 표시 (줄어듦 빨강 · 멈춤 주황 · 넓어짐 초록)",&native_prefs.wall_semaphore);
      igSetItemTooltip("NTL의 Dynamic border semaphore와 같습니다. 벽이 움직이는 서버에서 벽 선 색으로\n줄어드는지(빨강), 멈췄는지(주황), 넓어지는지(초록) 보여줍니다.\n벽이 움직이지 않는 서버에서는 벽 색이 그대로입니다.");
      igCheckbox("유리 벽 (흐리게)",&native_prefs.glass_border);
      igSetItemTooltip("NTL의 Glass Border: 벽 선과 바깥을 흐리게 그립니다.");
      igSeparatorText("SOS (플레이 중 S 두 번)");
      igText("지금: %s",sos_on ? "SOS 보내는 중" : "꺼짐"); igSameLine(0,-1);
      igBeginDisabled(!active && !demo);
      if(igButton(sos_on ? "SOS 끄기" : "SOS 보내기",(ImVec2){0})) ntl_toggle_sos();
      igEndDisabled();
      igCheckbox("SOS를 켤 때 채팅으로 \"Help me!\" 보내기",&native_prefs.sos_chat);
      igSetItemTooltip("NTL의 Send chat alert when SOS와 같습니다. 팀원 화면에서 나는 빨간색으로 보이고,\n팀원이 SOS를 켜면 화면 위에 알림이 뜨고 지도에 빨간 원으로 표시됩니다.");
      igSeparatorText("줌 · 스크린샷");
      igSetNextItemWidth(200);
      igSliderFloat("시작 줌",&native_prefs.start_zoom,.2f,3,"%.2f",ImGuiSliderFlags_AlwaysClamp);
      igSetItemTooltip("게임을 시작할 때마다 이 줌으로 시작합니다 (1.00 = 기본, 작을수록 멀리 봄).\n플레이 중 X를 누르면 이 줌으로 돌아갑니다. 휠과 M/N으로 바꾸는 것은 그대로입니다.");
      igTextWrapped("Z: 스크린샷 (게임 폴더의 screenshots에 PNG로 저장)");
      igSeparatorText("그래픽 (7 두 번으로 바꾸기)");
      { static const char* modes[]={"기본 스킨","스킨 없음 (단색)","고대비 스킨"};
        igSetNextItemWidth(200); igCombo_Str_arr("지렁이 그래픽",&native_prefs.skin_mode,modes,3,-1); }
      igSetItemTooltip("NTL Graphics 와 High visibility skins:\n스킨 없음 · 지렁이마다 스킨 대신 한 가지 색으로 그려 몸이 잘 구별됩니다.\n고대비 · 스킨 무늬를 더 밝고 선명하게 그립니다.");
      igCheckbox("내 지렁이는 원래 스킨 유지",&native_prefs.keep_own_skin);
      igSetNextItemWidth(200);
      igSliderFloat("다른 지렁이 불투명도",&native_prefs.others_opacity,.15f,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
      igSetItemTooltip("1.00이면 그대로, 낮추면 다른 지렁이 몸이 반투명해져 뒤의 먹이와 몸이 보입니다.");
      igSeparatorText("봇 (T 두 번으로 켜고 끔)");
      igCheckbox("스마트 봇 · B 두 번",&native_prefs.bot_smart);
      igSetItemTooltip("켜면 NTL처럼 먹이를 먹으며 자라다가, 아래 길이가 되면 몸을 말아 자기 몸을 따라 돕니다.\n끄면 길이와 상관없이 바로 자기 몸을 따라 돕니다.");
      igSetNextItemWidth(140);
      if(igInputInt("봇 길이",&native_prefs.bot_length,100,1000,0)) native_prefs.bot_length=native_prefs.bot_length<100?100:native_prefs.bot_length>50000?50000:native_prefs.bot_length;
      igSetItemTooltip("스마트 봇이 자라기를 멈추고 돌기 시작하는 길이입니다 (NTL 기본값 2000).");
      igCheckbox("느린 경로면 다시 접속",&native_prefs.path_retry);
      if(igIsItemHovered(0)) igSetTooltip("접속 직후 왕복 시간이 그 서버의 가장 빠른 값보다 12ms 넘게 느리면 끊고 다시 연결합니다 (최대 3번).\n접속 횟수가 늘어나므로 서버가 연속 접속을 거부할 수 있습니다. 기본은 꺼짐입니다.");
      igCheckbox("플레이 중 Wi-Fi 주변 검색 중지",&native_prefs.wifi_low_latency);
      if(igIsItemHovered(0)) igSetTooltip("Wi-Fi가 주변 채널을 훑는 동안 생기는 주기적인 핑 급등을 줄입니다.\n플레이 화면에서만 적용되고 메뉴로 나가거나 종료하면 원래대로 돌아갑니다.\n유선 연결이나 지원하지 않는 어댑터에서는 아무 일도 하지 않습니다.");
      igSetItemTooltip("켜면 사망 알림 직전의 게임 화면을 유지합니다. 자동 재시작보다 우선하며 R로 다시 시작, Esc로 메뉴에 돌아갑니다.");
      igTextWrapped("타겟: Backspace 두 번 → 상대 머리 위 숫자/문자. 같은 서버의 NTL 팀에 공유됩니다(약 4초 갱신). Delete는 내 표시/재전송을 해제하며, 팀 표적은 마지막 목격 후 5분에 만료됩니다.");
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

// Waits about `ms` (fractions too). Windows: a high-resolution waitable timer (Windows 10 1803+),
// else Sleep at the 1 ms timer period set in ntl_init. Elsewhere: nanosleep.
#define PACE_SPIN_MS 0.35
static void pace_nap(double ms) {
  if(ms<=0) return;
#ifdef _WIN32
  if(pace_timer) {
    LARGE_INTEGER due; due.QuadPart=-(LONGLONG)(ms*10000.0);     // relative, 100 ns units
    if(SetWaitableTimer(pace_timer,&due,0,NULL,NULL,FALSE) && WaitForSingleObject(pace_timer,50)==WAIT_OBJECT_0) return;
  }
  Sleep(ms>=1 ? (DWORD)ms : 1);
#else
  struct timespec t={(time_t)(ms/1000),(long)(fmod(ms,1000)*1e6)};
  nanosleep(&t,NULL);
#endif
}
void ntl_pace(tenv* env) {
  double pace_start=perf_now();
  int limit=caps[cap_choice]; double now=now_ms();
  if(!limit) { next_frame=now; perf_mark(); return; }
  double remaining=frame_pacer_next(&next_frame,now,1000.0/limit);
  // Browser sockets are serviced independently of requestAnimationFrame.
  // Keep our socket and input responsive while maintaining the selected cap.
  // Short naps on a precise timer, then a short spin for the last fraction of a millisecond:
  // the spin used to be up to 1 ms every frame (about 6% of a CPU core at 60 FPS).
  while(remaining>PACE_SPIN_MS) {
    double nap=fmin(remaining-PACE_SPIN_MS*.8,3), before=now_ms();
    pace_nap(nap);
    perf_add_duration(PERF_OVERSLEEP,fmax(0,now_ms()-before-nap));   // the system woke us later than asked
    // Window events are handled once per frame by the main loop right after this wait (input is
    // read at the start of each frame anyway); polling them on every nap here cost about 0.6 ms
    // of CPU per frame. The game socket is still serviced on every nap.
    server_service(env);
    remaining=next_frame-now_ms();
  }
  while(now_ms()<next_frame) SwitchToThread();
  perf_add(PERF_PACING,pace_start);
  perf_mark();
}
