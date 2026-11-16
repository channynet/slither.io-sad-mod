#include "skin_generator.h"
#include "skin_generator_bundle.h"
#include "../external/quickjs-ng/quickjs.h"
#include <string.h>
#include <time.h>

static int expired(JSContext* ctx,void* start) {
  (void)ctx;
  return (double)(clock()-*(clock_t*)start)/CLOCKS_PER_SEC>3;
}
static bool read_text(JSContext* ctx,JSValue object,const char* key,char* out,size_t capacity) {
  JSValue value=JS_GetPropertyStr(ctx,object,key);
  size_t length=0;const char* text=JS_IsString(value)?JS_ToCStringLen(ctx,&length,value):NULL;
  bool ok=text && length<capacity;
  if(ok) {memcpy(out,text,length);out[length]=0;}
  if(text) JS_FreeCString(ctx,text);
  JS_FreeValue(ctx,value);return ok;
}
bool skin_generator_make(uint32_t seed,generated_skin* result) {
  if(!result) return false;
  generated_skin candidate={0};bool ok=false;
  JSRuntime* runtime=JS_NewRuntime();if(!runtime) return false;
  JS_SetMemoryLimit(runtime,64*1024*1024);
  JS_SetMaxStackSize(runtime,1024*1024);
  clock_t start=clock();JS_SetInterruptHandler(runtime,expired,&start);
  JSContext* ctx=JS_NewContext(runtime);
  if(ctx) {
    JSValue evaluated=JS_Eval(ctx,(const char*)skin_generator_bundle,sizeof(skin_generator_bundle)-1,"skin-generator",JS_EVAL_TYPE_GLOBAL);
    if(!JS_IsException(evaluated)) {
      JSValue global=JS_GetGlobalObject(ctx),function=JS_GetPropertyStr(ctx,global,"nativeRandomSkin");
      JSValue arg=JS_NewUint32(ctx,seed),skin=JS_Call(ctx,function,JS_UNDEFINED,1,&arg);
      if(!JS_IsException(skin)) {
        JSValue mask=JS_GetPropertyStr(ctx,skin,"mask");
        ok=JS_ToUint32(ctx,&candidate.theory_mask,mask)==0 && candidate.theory_mask<16384 &&
          read_text(ctx,skin,"code",candidate.code,sizeof(candidate.code)) && strlen(candidate.code)==250 &&
          strspn(candidate.code,"1234567890-qwertyuiopasdfghjklzxcvbnm,")==250 &&
          read_text(ctx,skin,"topic",candidate.topic,sizeof(candidate.topic)) &&
          read_text(ctx,skin,"theories",candidate.theories,sizeof(candidate.theories));
        JS_FreeValue(ctx,mask);
      }
      JS_FreeValue(ctx,skin);JS_FreeValue(ctx,arg);JS_FreeValue(ctx,function);JS_FreeValue(ctx,global);
    }
    JS_FreeValue(ctx,evaluated);JS_FreeContext(ctx);
  }
  JS_FreeRuntime(runtime);
  if(ok) *result=candidate;
  return ok;
}
