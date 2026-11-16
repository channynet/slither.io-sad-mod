#include "skin_library.h"
#include "../external/mongoose.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#endif

static char* encode(const skin_design* d) {
  return mg_mprintf("{\"code\":%m,\"base\":[%d,%d,%d],\"effects\":[%d,%.6f,%d,%d,%d,%d],"
    "\"colors\":[%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f],"
    "\"tag\":[%d,%d,%d,%.6f,%.6f,%.6f,%.6f],"
    "\"images\":[[%d,%d,%.6f,%.6f,%.6f,%d,%d],[%d,%d,%.6f,%.6f,%.6f,%d,%d]]}",
    MG_ESC(d->code),d->custom,d->default_skin,d->accessory,d->fire,d->fire_strength,d->gradient,d->gradient_count,d->smile,d->claws,
    d->colors[0][0],d->colors[0][1],d->colors[0][2],d->colors[1][0],d->colors[1][1],d->colors[1][2],
    d->colors[2][0],d->colors[2][1],d->colors[2][2],d->colors[3][0],d->colors[3][1],d->colors[3][2],
    d->tag_mode,d->tag_id,d->local_tag,d->look[0],d->look[1],d->look[2],d->look[3],
    d->images[0].id,d->images[0].enabled,d->images[0].size,d->images[0].gap,d->images[0].angle,d->images[0].rotate,d->images[0].sway,
    d->images[1].id,d->images[1].enabled,d->images[1].size,d->images[1].gap,d->images[1].angle,d->images[1].rotate,d->images[1].sway);
}
static bool numbers(struct mg_str row,const char* key,float* out,int count) {
  for(int i=0;i<count;i++) {
    char path[80];double v;snprintf(path,sizeof(path),"$.%s[%d]",key,i);
    if(!mg_json_get_num(row,path,&v) || !isfinite(v) || fabs(v)>1000000) return false;
    out[i]=(float)v;
  }
  return true;
}
static bool decode(struct mg_str row,skin_design* d) {
  memset(d,0,sizeof(*d));char* code=mg_json_get_str(row,"$.code");
  if(!code || strlen(code)>256) {free(code);return false;}
  for(const char* p=code;*p;p++) if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p==','||*p=='-')) {free(code);return false;}
  strcpy(d->code,code);free(code);
  float base[3],effects[6],tag[7],colors[12];
  if(!numbers(row,"base",base,3)||!numbers(row,"effects",effects,6)||!numbers(row,"colors",colors,12)||!numbers(row,"tag",tag,7)) return false;
  memcpy(d->colors,colors,sizeof(colors));
  if(base[1]<0||base[1]>255||base[2]<0||base[2]>255||effects[1]<.2f||effects[1]>1.5f||effects[3]<2||effects[3]>4||tag[0]<0||tag[0]>1||tag[1]<-1||tag[1]>=700) return false;
  d->custom=base[0]!=0;d->default_skin=(int)base[1];d->accessory=(int)base[2];
  d->fire=effects[0]!=0;d->fire_strength=effects[1];d->gradient=effects[2]!=0;d->gradient_count=(int)effects[3];d->smile=effects[4]!=0;d->claws=effects[5]!=0;
  d->tag_mode=(int)tag[0];d->tag_id=(int)tag[1];d->local_tag=tag[2]!=0;memcpy(d->look,tag+3,sizeof(d->look));
  for(int i=0;i<12;i++) if(d->colors[i/3][i%3]<0||d->colors[i/3][i%3]>1) return false;
  for(int i=0;i<2;i++) {
    char key[24];float v[7];snprintf(key,sizeof(key),"images[%d]",i);
    if(!numbers(row,key,v,7)||v[0]<0||v[2]<24||v[2]>320||v[3]<-100||v[3]>400||fabsf(v[4])>180) return false;
    d->images[i]=(skin_image_state){(int)v[0],v[1]!=0,v[5]!=0,v[6]!=0,v[2],v[3],v[4]};
  }
  return true;
}
bool skin_library_load(skin_library* l,const char* path) {
  memset(l,0,sizeof(*l));FILE* f=fopen(path,"rb");
  if(!f) {l->read_only=errno!=ENOENT;return !l->read_only;}
  fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
  char* data=n>0&&n<=512*1024?malloc(n+1):NULL;
  bool ok=data&&fread(data,1,n,f)==(size_t)n;fclose(f);
  if(ok) {
    data[n]=0;struct mg_str all=mg_str_n(data,n),items,item;int len,at=mg_json_get(all,"$.items",&len);double version;
    ok=mg_json_get_num(all,"$.version",&version)&&version==1&&at>=0&&len>=2&&data[at]=='[';
    if(ok) {
      items=mg_str_n(data+at,len);size_t offset=0;
      while((offset=mg_json_next(items,offset,NULL,&item))!=0) {
        if(l->count==SKIN_LIBRARY_MAX||!decode(item,&l->items[l->count])) {ok=false;break;}
        l->count++;
      }
    }
  }
  free(data);l->read_only=!ok;if(!ok) l->count=0;return ok;
}
int skin_library_remember(skin_library* l,const char* path,const skin_design* d) {
  if(l->read_only) return -1;
  char* value=encode(d);if(!value) return -1;
  skin_design checked;if(!decode(mg_str(value),&checked)) {free(value);return -1;}
  for(int i=0;i<l->count;i++) {
    char* old=encode(&l->items[i]);bool same=old&&!strcmp(old,value);free(old);
    if(same) {free(value);return i;}
  }
  if(l->count==SKIN_LIBRARY_MAX) {free(value);return -1;}
  char temp[512];if(snprintf(temp,sizeof(temp),"%s.tmp",path)>=(int)sizeof(temp)) {free(value);return -1;}
  FILE* f=fopen(temp,"wb");if(!f) {free(value);return -1;}
  bool ok=fputs("{\"version\":1,\"items\":[",f)>=0;
  for(int i=0;i<l->count&&ok;i++) {
    char* old=encode(&l->items[i]);ok=old&&fprintf(f,"%s%s",i?",":"",old)>0;free(old);
  }
  if(fprintf(f,"%s%s]}\n",l->count?",":"",value)<0) ok=false;
  if(fclose(f)) ok=false;
#ifdef _WIN32
  if(ok) ok=MoveFileExA(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
  if(ok) ok=rename(temp,path)==0;
#endif
  free(value);if(!ok) return -1;
  l->items[l->count]=checked;return l->count++;
}
