#include "../app/src/network/ntl_protocol.h"
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
  request.x=NAN; assert(!ntl_build_path(&request,path,sizeof(path))); request.x=1;
  assert(!ntl_build_path(&request,path,20));
  assert(ntl_encode_chat("한글 <a> & b",encoded,sizeof(encoded)));
  assert(!strcmp(encoded,"한글&nbsp;&lt;a&gt;&nbsp;&amp;&nbsp;b"));
  ntl_plain(encoded,plain,sizeof(plain)); assert(!strcmp(plain,"한글 <a> & b"));
  assert(!ntl_encode_chat("bad\nline",encoded,sizeof(encoded))); assert(!ntl_encode_chat("",encoded,sizeof(encoded)));
  assert(!ntl_encode_chat("&&",encoded,4));
  ntl_plain("<script>text</script><br>a&nbsp;b",plain,sizeof(plain)); assert(!strcmp(plain,"text\na b"));
  assert(ntl_parse("[]","ABCDEFGHme",reply)==0 && !reply->own_echo);
  const char* json="[{\"nick\":\"00000000\",\"msg\":\"notice\"},{\"nick\":\"ABCDEFGHme\",\"srv\":\"server\",\"valx\":\"123.4\",\"valy\":42,\"sid\":\"500\",\"score\":300,\"msg\":\"hello\",\"sos\":true}]";
  assert(ntl_parse(json,"ABCDEFGHme",reply)==0); assert(reply->count==1 && reply->own_echo);
  assert(reply->rows[0].position && fabs(reply->rows[0].x-123.4)<0.01 && reply->rows[0].sid==500 && reply->rows[0].sos);
  assert(ntl_parse("[{\"nick\":\"ABCDEFGHme\",\"srv\":\"server\",\"valx\":\"NaN\",\"valy\":42}]","",reply)==0 && !reply->rows[0].position);
  assert(ntl_parse("[{\"nick\":\"tiny\"}]","",reply)==2);
  assert(ntl_parse("<html>error</html>","",reply)==2);
  assert(ntl_parse("[{}] garbage","",reply)==2);
  assert(ntl_parse("[{broken}]","",reply)==2);
  assert(ntl_parse("[1]","",reply)==2);
  assert(ntl_snake_id(0xabcd,0x1234563f)==((63u<<10)|(0xabcdu&1023)));
  assert(ntl_snake_id(65535,0)==1023 && ntl_snake_id(7,64)==7);
  free(reply); puts("NTL protocol tests passed: URL encoding, Unicode, validation, parsing, identity, ID mapping.");
}
