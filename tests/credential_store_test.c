#include "../app/src/network/credential_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
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
  assert(credential_store_delete(path)); assert(!credential_store_load(path,a,t));
  assert(credential_store_delete(path));
  puts("Credential store: encrypted round trip, replacement, tamper rejection and deletion passed");
}
