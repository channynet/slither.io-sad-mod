#include "../app/src/network/http_simple.c"
#include <assert.h>

static void request(const char* base,const char* path,size_t limit,int expected,const char* text) {
  char url[256]; snprintf(url,sizeof(url),"%s%s",base,path);
  unsigned char* out=(unsigned char*)1; size_t length=999;
  int status=http_request("GET",url,NULL,NULL,0,&out,&length,limit,1000);
  if(status!=expected) fprintf(stderr,"%s: status %d (expected %d), length %zu\n",path,status,expected,length);
  assert(status==expected);
  if(text) { assert(out && length==strlen(text) && !strcmp((char*)out,text)); }
  else { assert(out==NULL && length==0); }
  free(out);
}
int main(int argc,char** argv) {
  assert(argc==2);
  reply_buffer b={NULL,0,0,4};
  assert(append(&b,"abc",3) && b.length==3 && b.cap<=5);
  assert(!append(&b,"de",2) && b.length==3 && !strcmp((char*)b.data,"abc"));
  assert(append(&b,"d",1) && !strcmp((char*)b.data,"abcd"));
  assert(!append(&b,"x",SIZE_MAX)); free(b.data);
  b=(reply_buffer){NULL,0,0,SIZE_MAX}; assert(!append(&b,"x",SIZE_MAX));
  request(argv[1],"/exact",4,200,"abcd");
  request(argv[1],"/over",4,0,NULL);
  request(argv[1],"/chunked",4,0,NULL);
  request(argv[1],"/partial",64,0,NULL);
  request(argv[1],"/empty",4,204,NULL);
  request(argv[1],"/over",0,200,NULL);
  request(argv[1],"/denied",64,403,"denied");
  puts("HTTP: exact limit, oversized/chunked/incomplete rejection, status-only and overflow checks passed");
}
