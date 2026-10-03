#include "image_library.h"
#include "../external/mongoose.h"
#include <windows.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char* slugs[2][10]={
  {"ruby-heart","crescent-moon","gold-star","blue-flame","emerald-leaf","ice-crystal","purple-gem","lucky-clover","sun-medallion","silver-feather"},
  {"raven-wings","angel-wings","bat-wings","gold-crown","devil-horns","ice-horns","silver-antlers","laurel-wreath","cyber-fins","flame-wings"}
};
static const char* names[2][10]={
  {"루비 하트","초승달","황금 별","푸른 불꽃","에메랄드 잎","얼음 결정","보라 보석","네잎클로버","태양 문양","은빛 깃털"},
  {"검은 날개","천사 날개","박쥐 날개","황금 왕관","붉은 뿔","얼음 뿔","은빛 사슴뿔","월계관","네온 지느러미","불꽃 날개"}
};
static const char* kind(const image_library* l) { return l->cosmetic?"cosmetic":"tag"; }
static void catalog_path(const image_library* l,char* out,size_t n) { snprintf(out,n,"personal-%s-library.json",kind(l)); }
static image_item defaults(bool cos) { return (image_item){.size=cos?160:96,.gap=cos?-18:24,.rotate=cos}; }
void image_library_seed(image_library* l,bool cos) {
  memset(l,0,sizeof(*l)); l->cosmetic=cos; l->enabled=true; l->count=10;
  for(int i=0;i<10;i++) {
    l->items[i]=defaults(cos); l->items[i].id=i+1;
    snprintf(l->items[i].name,sizeof(l->items[i].name),"%s",names[cos][i]);
    if(cos && i>=3 && i<=7) { l->items[i].size=80; l->items[i].gap=4; }
  }
}
void image_library_path(const image_library* l,int index,char* out,size_t n) {
  if(index<0 || index>=l->count) { if(n) out[0]=0; return; }
  int id=l->items[index].id;
  if(id>=1 && id<=10) snprintf(out,n,"app/res/personal/library/%s-%s.png",kind(l),slugs[l->cosmetic][id-1]);
  else snprintf(out,n,"personal-%s-item-%d.rgba",kind(l),id);
}
image_item* image_library_add(image_library* l,const char* name) {
  if(l->read_only || l->count>=IMAGE_LIBRARY_MAX || !name || !*name || strlen(name)>=sizeof(l->items[0].name)) return NULL;
  int id=100;
  for(int i=0;i<l->count;i++) if(l->items[i].id>=id) id=l->items[i].id+1;
  if(id>1000000) return NULL;
  image_item* item=&l->items[l->count++]; *item=defaults(l->cosmetic); item->id=id;
  strcpy(item->name,name); return item;
}
static bool number(struct mg_str json,const char* path,double lo,double hi,double* out) {
  return mg_json_get_num(json,path,out) && isfinite(*out) && *out>=lo && *out<=hi;
}
int image_library_load(image_library* l) {
  char path[96]; catalog_path(l,path,sizeof(path));
  FILE* f=fopen(path,"rb"); if(!f) { if(errno==ENOENT) return 0; l->read_only=true; return -1; }
  char data[32768]; size_t n=fread(data,1,sizeof(data),f); bool io=ferror(f); fclose(f);
  image_library next={.cosmetic=l->cosmetic};
  struct mg_str json=mg_str_n(data,n); double v; int selected_id=0;
  bool ok=!io && n<sizeof(data) && number(json,"$.version",1,1,&v) &&
      number(json,"$.count",1,IMAGE_LIBRARY_MAX,&v) && floor(v)==v;
  if(ok) next.count=(int)v;
  ok=ok && number(json,"$.selected",1,1000000,&v) && floor(v)==v;
  if(ok) selected_id=(int)v;
  ok=ok && mg_json_get_bool(json,"$.enabled",&next.enabled);
  bool found=false;
  for(int i=0;ok && i<next.count;i++) {
    char key[48]; snprintf(key,sizeof(key),"$.items[%d]",i);
    struct mg_str entry=mg_json_get_tok(json,key); image_item* item=&next.items[i];
    ok=number(entry,"$.id",1,1000000,&v) && floor(v)==v && (v<=10 || v>=100);
    if(ok) item->id=(int)v;
    for(int j=0;j<i;j++) if(next.items[j].id==item->id) ok=false;
    char* name=mg_json_get_str(entry,"$.name");
    ok=ok && name && *name && strlen(name)<sizeof(item->name);
    if(ok) strcpy(item->name,name); mg_free(name);
    ok=ok && number(entry,"$.size",24,l->cosmetic?320:192,&v); if(ok) item->size=(float)v;
    ok=ok && number(entry,"$.gap",l->cosmetic?-100:0,l->cosmetic?100:120,&v); if(ok) item->gap=(float)v;
    ok=ok && number(entry,"$.angle",-180,180,&v); if(ok) item->angle=(float)v;
    ok=ok && mg_json_get_bool(entry,"$.rotate",&item->rotate);
    if(item->id==selected_id) { next.selected=i; found=true; }
  }
  if(!ok || !found) { l->read_only=true; return -1; }
  *l=next; return 1;
}
bool image_library_save(const image_library* l) {
  if(l->read_only || l->count<1 || l->count>IMAGE_LIBRARY_MAX || l->selected<0 || l->selected>=l->count) return false;
  char path[96],temp[104]; catalog_path(l,path,sizeof(path)); snprintf(temp,sizeof(temp),"%s.tmp",path);
  FILE* f=fopen(temp,"wb"); if(!f) return false;
  bool ok=fprintf(f,"{\"version\":1,\"count\":%d,\"selected\":%d,\"enabled\":%s,\"items\":[\n",l->count,l->items[l->selected].id,l->enabled?"true":"false")>0;
  for(int i=0;ok && i<l->count;i++) {
    const image_item* p=&l->items[i];
    char* row=mg_mprintf("%s{\"id\":%d,\"name\":%m,\"size\":%.3f,\"gap\":%.3f,\"angle\":%.3f,\"rotate\":%s}",
        i?",\n":"",p->id,MG_ESC(p->name),p->size,p->gap,p->angle,p->rotate?"true":"false");
    ok=row && fputs(row,f)>=0; mg_free(row);
  }
  if(fputs("\n]}\n",f)<0) ok=false;
  if(fclose(f)) ok=false;
  if(ok) ok=MoveFileExA(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
  return ok;
}
