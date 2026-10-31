#include "../app/src/network/credential_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <stdlib.h>
int main(void) {
  const wchar_t* path=L"synthetic-keys.dat";
  const char *auth="synthetic-auth-key-123456789", *team="synthetic-team-key-987654321";
  char a[513],t[513];
  assert(!credential_store_load(path,a,t));
  assert(!credential_store_save(path,"short",team));
  assert(credential_store_save(path,auth,team));
  assert(credential_store_load(path,a,t));
  assert(!strcmp(a,auth) && !strcmp(t,team));
  FILE* f=_wfopen(path,L"rb+"); assert(f);
  unsigned char data[8192]; size_t n=fread(data,1,sizeof(data),f); assert(n>100);
  for(size_t i=0;i+strlen(auth)<=n;i++) assert(memcmp(data+i,auth,strlen(auth)));
  for(size_t i=0;i+strlen(team)<=n;i++) assert(memcmp(data+i,team,strlen(team)));
  data[n-1]^=0x80; rewind(f); assert(fwrite(data,1,n,f)==n); fclose(f);
  assert(!credential_store_load(path,a,t)); assert(!a[0] && !t[0]);
  assert(credential_store_save(path,"replacement-auth-12345","replacement-team-12345"));
  assert(credential_store_load(path,a,t)); assert(!strcmp(a,"replacement-auth-12345"));
  // A file from an older version (one pair) reads as one profile.
  team_profiles* p=calloc(1,sizeof(*p)); team_profiles* q=calloc(1,sizeof(*q)); assert(p && q);
  assert(credential_store_load_profiles(path,p) && p->count==1 && p->selected==0);
  assert(!strcmp(p->items[0].auth,"replacement-auth-12345") && !strcmp(p->items[0].team,"replacement-team-12345") && p->items[0].name[0]);
  // Several named profiles, the selected one kept; the selected pair is what the old call returns.
  memset(p,0,sizeof(*p)); p->count=3; p->selected=1;
  for(int i=0;i<3;i++) {
    snprintf(p->items[i].name,sizeof(p->items[i].name),"팀 %d",i+1);
    snprintf(p->items[i].auth,sizeof(p->items[i].auth),"synthetic-auth-%d-0123456789",i);
    snprintf(p->items[i].team,sizeof(p->items[i].team),"synthetic-team-%d-0123456789",i);
  }
  assert(credential_store_save_profiles(path,p));
  assert(credential_store_load_profiles(path,q) && q->count==3 && q->selected==1 && !memcmp(q->items,p->items,3*sizeof(team_profile)));
  assert(credential_store_load(path,a,t) && !strcmp(a,"synthetic-auth-1-0123456789") && !strcmp(t,"synthetic-team-1-0123456789"));
  f=_wfopen(path,L"rb"); n=fread(data,1,sizeof(data),f); fclose(f);
  for(size_t i=0;i+20<=n;i++) assert(memcmp(data+i,"synthetic-auth-",15) && memcmp(data+i,"synthetic-team-",15));
  // Invalid profiles are not written; an empty list is.
  strcpy(p->items[2].auth,"short"); assert(!credential_store_save_profiles(path,p));
  p->count=TEAM_PROFILE_MAX+1; assert(!credential_store_save_profiles(path,p));
  memset(p,0,sizeof(*p)); p->selected=-1; assert(credential_store_save_profiles(path,p));
  assert(credential_store_load_profiles(path,q) && q->count==0 && q->selected==-1);
  assert(!credential_store_load(path,a,t));
  free(p); free(q);
  assert(credential_store_delete(path)); assert(!credential_store_load(path,a,t));
  assert(credential_store_delete(path));
  puts("Credential store: encrypted round trip, replacement, tamper rejection, profiles, old-format reading and deletion passed");
}
