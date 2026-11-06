#include "../app/src/network/server_catalog.c"
#include <assert.h>
static volatile LONG calls, fail_reply;
static const char fixture[]=
  "{\"type\":\"success\",\"dataList\":["
  "{\"ipv4\":\"181.41.140.146\",\"po\":444,\"snakeCount\":397,\"cc\":\"JP\",\"contc\":\"AS\",\"tz\":\"Asia/Tokyo\",\"updatedAt\":1793929613340,"
  "\"leaderboard\":[{\"place\":2,\"nk\":\"second\",\"len\":400},{\"place\":1,\"nk\":\"first %s\\nplayer\",\"len\":500},"
  "{\"place\":1,\"nk\":\"duplicate\",\"len\":600},{\"place\":11,\"nk\":\"invalid\",\"len\":5}]},"
  "{\"ipv4\":\"181.41.140.146\",\"po\":445,\"snakeCount\":0,\"leaderboard\":[]},"
  "{\"ipv4\":\"181.41.140.146\",\"po\":444},"
  "{\"ipv4\":\"bad.host\",\"po\":444},{\"ipv4\":\"1.2.3.4\",\"po\":0},"
  "{\"ipv4\":\"1.2.3.4\",\"po\":444,\"snakeCount\":-1}]}";
int http_request_referer(const char* method,const char* url,const char* ref,const char* type,const void* body,size_t body_len,
                        unsigned char** out,size_t* len,size_t max,int timeout) {
  assert(!strcmp(method,"GET") && !strcmp(url,SERVER_CATALOG_URL) && !strcmp(ref,SERVER_CATALOG_SOURCE));
  assert(!body && !body_len && max==2*1024*1024 && timeout==4000);
  InterlockedIncrement(&calls);
  if(InterlockedCompareExchange(&fail_reply,0,0)) { *out=NULL; *len=0; return 503; }
  *len=strlen(fixture); *out=malloc(*len+1); memcpy(*out,fixture,*len+1); return 200;
}
static void finish(server_catalog* c) {
  ULONGLONG start=GetTickCount64();
  do { Sleep(1); server_catalog_copy(c); } while(c->pending && GetTickCount64()-start<3000);
  assert(!c->pending);
}
int main(void) {
  server_catalog* c=calloc(1,sizeof(*c)); assert(c);
  assert(server_catalog_parse(fixture,strlen(fixture),c));
  assert(c->count==3 && c->entries[0].players==397 && c->entries[1].players==0 && c->entries[2].players==-1);
  assert(!strcmp(c->entries[0].address,"181.41.140.146:444"));
  assert(!strcmp(c->entries[1].address,"181.41.140.146:445"));
  assert(!strcmp(c->entries[0].location,"Asia/Tokyo") && c->entries[0].updated_at==1793929613);
  assert(c->entries[0].leader_count==2 && c->entries[0].leaders[0].rank==1);
  assert(!strcmp(c->entries[0].leaders[0].name,"first %s player"));
  assert(c->entries[0].leaders[0].score==500);
  assert(!server_catalog_parse(fixture,strlen(fixture)-3,c) && c->count==3);
  const char* invalid[]={"{}","{\"type\":\"failed\",\"dataList\":[]}","{\"type\":\"success\",\"dataList\":{}}",
    "{\"type\":\"success\",\"dataList\":[{\"ipv4\":\"garbage\",\"po\":1}]}","{\"type\":\"success\",\"dataList\":[]} trailing"};
  for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) assert(!server_catalog_parse(invalid[i],strlen(invalid[i]),c) && c->count==3);
  char* many=calloc(1,60000); assert(many); strcpy(many,"{\"type\":\"success\",\"dataList\":[");
  for(int i=0;i<300;i++) { char row[128]; snprintf(row,sizeof(row),"%s{\"ipv4\":\"1.2.3.4\",\"po\":%d}",i?",":"",i+1); strcat(many,row); }
  strcat(many,"]}"); assert(server_catalog_parse(many,strlen(many),c) && c->count==SERVER_CATALOG_LIMIT); free(many);
  server_catalog_init(); Sleep(20); assert(calls==0); // no background polling before opening the picker
  server_catalog_request(); finish(c);
  assert(calls==1 && c->count==3 && !c->failed && c->received_at>0);
  for(int i=0;i<100;i++) server_catalog_request();
  Sleep(20); assert(calls==1); // frame-rate independent 30-second throttle
  EnterCriticalSection(&lock); attempted=GetTickCount64()-30001; LeaveCriticalSection(&lock);
  InterlockedExchange(&fail_reply,1); server_catalog_request(); finish(c);
  assert(calls==2 && c->failed && c->http_status==503 && c->count==3); // last successful snapshot retained
  server_catalog_destroy(); free(c);
  puts("Server catalog: schema validation, names, ranks, exact ports, bounds, request throttle and failure retention passed");
}
