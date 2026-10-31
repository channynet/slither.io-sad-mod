#include "../app/src/network/ntl_protocol.h"
#include "../app/src/network/utf8_text.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
int main(void) {
  ntl_reply* reply=malloc(sizeof(*reply));
  char path[32768],encoded[5401],plain[256];
  ntl_request request={0};
  strcpy(request.auth,"a&b"); strcpy(request.team,"t+/"); strcpy(request.nick,"ABCDEFGH한글");
  assert(ntl_build_path(&request,path,sizeof(path)));
  assert(strstr(path,"auth=a%26b")); assert(strstr(path,"tid=t%2B%2F"));
  assert(strstr(path,"nick=ABCDEFGH%ED%95%9C%EA%B8%80"));
  assert(strstr(path,"srv=_GAME_MENU_")); assert(!strstr(path,"&sid="));
  request.playing=true; request.x=120.5; request.y=800.25; request.sid=65535; strcpy(request.server,"127.0.0.1:444");
  assert(ntl_build_path(&request,path,sizeof(path))); assert(strstr(path,"sid=65535")); assert(strstr(path,"srv=127.0.0.1%3A444"));
  assert(strstr(path,"&cs=-1&tg=0"));                // the NTL tag worn goes to the team (as NTL's tg)
  request.tag=-1; assert(ntl_build_path(&request,path,sizeof(path)) && strstr(path,"&tg=-1"));
  request.tag=203; assert(ntl_build_path(&request,path,sizeof(path)) && strstr(path,"&tg=203"));
  request.x=NAN; assert(!ntl_build_path(&request,path,sizeof(path))); request.x=1;
  assert(!ntl_build_path(&request,path,20));
  assert(ntl_encode_chat("한글 <a> & b",encoded,sizeof(encoded)));
  assert(!strcmp(encoded,"한글&nbsp;&lt;a&gt;&nbsp;&amp;&nbsp;b"));
  ntl_plain(encoded,plain,sizeof(plain)); assert(!strcmp(plain,"한글 <a> & b"));
  assert(!ntl_encode_chat("bad\nline",encoded,sizeof(encoded))); assert(!ntl_encode_chat("",encoded,sizeof(encoded)));
  assert(!ntl_encode_chat("&&",encoded,4));
  ntl_plain("<script>text</script><br>a&nbsp;b",plain,sizeof(plain)); assert(!strcmp(plain,"text\na b"));
  assert(ntl_parse("[]","ABCDEFGHme",reply)==0 && !reply->own_echo);
  const char* korean_json="[{\"nick\":\"ABCDEFGH\\ud55c\\uae00\",\"srv\":\"server\",\"msg\":\"\\uc548\\ub155 \\ud55c\\uae00 \\ud83d\\ude00\"}]";
  assert(ntl_parse(korean_json,"ABCDEFGH한글",reply)==0 && reply->own_echo);
  assert(!strcmp(reply->rows[0].nick,"ABCDEFGH한글"));
  assert(!strcmp(reply->rows[0].message,"안녕 한글 😀"));
  assert(ntl_parse("[{\"nick\":\"ABCDEFGHme\",\"srv\":\"server\",\"msg\":\"literal \\\\ud55c\"}]","",reply)==0);
  assert(!strcmp(reply->rows[0].message,"literal \\ud55c")); // Decode exactly once.
  const char* bad_unicode[]={"\\ud800","\\udc00","\\ud800\\u0041","\\u0000","\\uzzzz"};
  for(int i=0;i<5;i++) {
    char data[256]; snprintf(data,sizeof(data),"[{\"nick\":\"ABCDEFGHme\",\"srv\":\"server\",\"msg\":\"%s\"}]",bad_unicode[i]);
    assert(ntl_parse(data,"",reply)==2);
  }
  ntl_plain("&#54620;&#xAE00; &lt;text&gt;",plain,sizeof(plain)); assert(!strcmp(plain,"한글 <text>"));
  for(size_t cap=1;cap<10;cap++) {
    ntl_plain("한글입",plain,cap); assert(strlen(plain)==((cap-1)/3)*3);
    utf8_copy(plain,cap,"한글입"); assert(strlen(plain)==((cap-1)/3)*3);
  }
  const char* json="[{\"nick\":\"00000000\",\"msg\":\"notice\"},{\"nick\":\"ABCDEFGHme\",\"srv\":\"server\",\"valx\":\"123.4\",\"valy\":42,\"sid\":\"500\",\"score\":300,\"msg\":\"hello\",\"sos\":true}]";
  assert(ntl_parse(json,"ABCDEFGHme",reply)==0); assert(reply->count==1 && reply->own_echo);
  assert(reply->rows[0].position && fabs(reply->rows[0].x-123.4)<0.01 && reply->rows[0].sid==500 && reply->rows[0].sos);
  assert(reply->rows[0].tag==-1);                    // no tg: none
  assert(ntl_parse("[{\"nick\":\"ABCDEFGHme\",\"srv\":\"server\",\"msg\":\"\",\"tg\":\"583\"}]","",reply)==0 && reply->rows[0].tag==583);
  assert(ntl_parse("[{\"nick\":\"ABCDEFGHme\",\"srv\":\"server\",\"msg\":\"\",\"tg\":-1}]","",reply)==0 && reply->rows[0].tag==-1);
  assert(ntl_parse("[{\"nick\":\"ABCDEFGHme\",\"srv\":\"server\",\"valx\":\"NaN\",\"valy\":42}]","",reply)==0 && !reply->rows[0].position);
  assert(ntl_parse("[{\"nick\":\"tiny\"}]","",reply)==2);
  assert(ntl_parse("<html>error</html>","",reply)==2);
  assert(ntl_parse("[{}] garbage","",reply)==2);
  assert(ntl_parse("[{broken}]","",reply)==2);
  assert(ntl_parse("[1]","",reply)==2);
  assert(ntl_snake_id(0xabcd,0x1234563f)==((63u<<10)|(0xabcdu&1023)));
  assert(ntl_snake_id(65535,0)==1023 && ntl_snake_id(7,64)==7);
  const char* telemetry[]={"FPS: 144 @ 45(160) ms @ 2 K/m","FPS: 60 @ 0 ms","FPS: 30","SlitherNative-0.1","FPS: NaN @ 5 ms","FPS: 60 @ -1 ms","FPS: 60 @ 999999999999999999 ms","FPS: 60 @ 25 nope","FPS: 60 @ 25(999999) ms","<b>FPS:</b>&nbsp;120&nbsp;@&nbsp;75(85)&nbsp;ms"};
  int fps[]={144,60,30,-1,-1,60,60,60,60,120},ping[]={45,0,-1,-1,-1,-1,-1,-1,-1,75};
  for(int i=0;i<10;i++) {
    char data[2048]; snprintf(data,sizeof(data),"[{\"nick\":\"ABCDEFGHme\",\"srv\":\"server\",\"dt\":\"%s\"}]",telemetry[i]);
    assert(ntl_parse(data,"",reply)==0 && reply->rows[0].fps==fps[i] && reply->rows[0].ping==ping[i]);
  }
  assert(ntl_parse("[{\"nick\":\"ABCDEFGHme\",\"srv\":\"server\"}]","",reply)==0 && reply->rows[0].fps==-1 && reply->rows[0].ping==-1);
  request.x=1; request.fps=120; request.ping=48;
  assert(ntl_build_path(&request,path,sizeof(path)) && strstr(path,"dt=FPS%3A%20120%20%40%2048%20ms"));
  request.ping_peak=70;                     // NTL's format with the 10 s peak, read back by the parser
  assert(ntl_build_path(&request,path,sizeof(path)) && strstr(path,"dt=FPS%3A%20120%20%40%2048%2870%29%20ms"));
  assert(ntl_parse("[{\"nick\":\"ABCDEFGHme\",\"srv\":\"server\",\"dt\":\"FPS: 120 @ 48(70) ms\"}]","",reply)==0 && reply->rows[0].fps==120 && reply->rows[0].ping==48);
  request.ping_peak=0;
  request.playing=false;
  assert(ntl_build_path(&request,path,sizeof(path)) && strstr(path,"dt=FPS%3A%20120&") && !strstr(path,"2048%20ms"));
  request.playing=true; strcpy(request.target,"9_123_27|2");
  assert(ntl_build_path(&request,path,sizeof(path)) && strstr(path,"&tar=9_123_27%7C2"));
  assert(ntl_parse("[{\"nick\":\"ABCDEFGHally\",\"srv\":\"server\",\"tar\":\"9_123_27|2\"}]","",reply)==0);
  assert(!strcmp(reply->rows[0].target,request.target));
  const char* bad[]={"9_123_27&auth=x","9_999999_27","-1_2_3","1_2_3|7","1_2_3|1junk","1_2","1__2","99999999999_2_3"};
  for(int i=0;i<8;i++) assert(!ntl_target_valid(bad[i]));
  assert(ntl_target_valid("0_0_1") && ntl_target_valid("255_65535_27") && ntl_target_valid("9_4_"));
  strcpy(request.target,bad[0]); assert(!ntl_build_path(&request,path,sizeof(path)));
  request.playing=false; assert(ntl_build_path(&request,path,sizeof(path)) && !strstr(path,"&tar="));
  // NTL flags: SOS (S S) and bot are sent only while playing.
  request.sos=request.bot=true;
  assert(ntl_build_path(&request,path,sizeof(path)) && strstr(path,"&bot=false&sos=false&food=false"));
  request.playing=true; request.target[0]=0;
  assert(ntl_build_path(&request,path,sizeof(path)) && strstr(path,"&bot=true&sos=true&food=false"));
  request.bot=false; assert(ntl_build_path(&request,path,sizeof(path)) && strstr(path,"&bot=false&sos=true&food=false"));
  puts("NTL flags: SOS and bot sent only while playing passed");
  puts("Team telemetry: NTL dt format, HTML entities, missing/invalid metrics, bounds and outbound menu/game telemetry passed");
  free(reply); puts("NTL protocol tests passed: URL encoding, Unicode, validation, parsing, identity, ID mapping.");
}
