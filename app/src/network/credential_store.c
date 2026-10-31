#include "credential_store.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// The keys are kept as one protected blob: DPAPI (bound to this Windows account) on Windows,
// a file only this user can read (0600) elsewhere. Format 1 holds one auth/team pair; format 2
// holds named profiles (NTL's "Saved keys").
typedef struct { uint32_t version; char auth[513],team[513]; } stored_keys;
typedef struct { uint32_t version, count; int32_t selected; } stored_profiles_head;
#define CREDENTIAL_MAX_FILE (256*1024)

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
static void wipe(void* p,size_t n) { SecureZeroMemory(p,n); }
bool credential_store_path(wchar_t* path,size_t capacity) {
  if(capacity<64 || capacity>32768) return false;
  DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",path,(DWORD)capacity);
  if(!n || n+64>=capacity) return false;
  wcscat(path,L"\\SlitherNative");
  if(!CreateDirectoryW(path,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS) return false;
  wcscat(path,L"\\team-keys.dat"); return true;
}
static bool blob_write(const wchar_t* path,const void* data,size_t size) {
  if(wcslen(path)>32000) return false;
  DATA_BLOB input={(DWORD)size,(BYTE*)data},encrypted={0};
  if(!CryptProtectData(&input,L"Slither Native team keys",NULL,NULL,NULL,CRYPTPROTECT_UI_FORBIDDEN,&encrypted)) return false;
  wchar_t temp[32768]; swprintf(temp,32768,L"%ls.tmp",path);
  HANDLE file=CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
  DWORD written=0;
  bool ok=file!=INVALID_HANDLE_VALUE && WriteFile(file,encrypted.pbData,encrypted.cbData,&written,NULL) && written==encrypted.cbData;
  if(file!=INVALID_HANDLE_VALUE) { if(ok) ok=FlushFileBuffers(file); CloseHandle(file); }
  LocalFree(encrypted.pbData);
  if(ok) ok=MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
  if(!ok) DeleteFileW(temp);
  return ok;
}
// The plain bytes, in a buffer the caller wipes and frees.
static unsigned char* blob_read(const wchar_t* path,size_t* size) {
  *size=0;
  HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
  if(file==INVALID_HANDLE_VALUE) return NULL;
  DWORD length=GetFileSize(file,NULL),read=0;
  unsigned char* bytes=length>0 && length<=CREDENTIAL_MAX_FILE ? malloc(length) : NULL;
  bool ok=bytes && ReadFile(file,bytes,length,&read,NULL) && read==length;
  CloseHandle(file);
  if(!ok) { free(bytes); return NULL; }
  DATA_BLOB input={length,bytes},plain={0};
  ok=CryptUnprotectData(&input,NULL,NULL,NULL,NULL,CRYPTPROTECT_UI_FORBIDDEN,&plain);
  wipe(bytes,length); free(bytes);
  if(!ok) return NULL;
  unsigned char* out=malloc(plain.cbData ? plain.cbData : 1);
  if(out) { memcpy(out,plain.pbData,plain.cbData); *size=plain.cbData; }
  wipe(plain.pbData,plain.cbData); LocalFree(plain.pbData);
  return out;
}
bool credential_store_delete(const wchar_t* path) {
  return DeleteFileW(path) || GetLastError()==ERROR_FILE_NOT_FOUND;
}
#else
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../platform/compat.h"
static void wipe(void* p,size_t n) { volatile unsigned char* b=p; while(n--) *b++=0; }
static bool narrow(const wchar_t* in,char* out,size_t cap) {
  return WideCharToMultiByte(CP_UTF8,0,in,-1,out,(int)cap,NULL,NULL)>0;
}
bool credential_store_path(wchar_t* path,size_t capacity) {
  if(capacity<64 || capacity>32768) return false;
  const char* home=getenv("HOME"); if(!home || !*home) return false;
  char dir[4096];
#ifdef __APPLE__
  snprintf(dir,sizeof(dir),"%s/Library/Application Support/SlitherNative",home);
#else
  snprintf(dir,sizeof(dir),"%s/.local/share/SlitherNative",home);
#endif
  for(char* c=dir+1;*c;c++) if(*c=='/') { *c=0; mkdir(dir,0700); *c='/'; }
  if(mkdir(dir,0700) && errno!=EEXIST) return false;
  char file[4200]; snprintf(file,sizeof(file),"%s/team-keys.dat",dir);
  return MultiByteToWideChar(CP_UTF8,0,file,-1,path,(int)capacity)>0;
}
static bool blob_write(const wchar_t* path,const void* data,size_t size) {
  char file[4200],temp[4300]; if(!narrow(path,file,sizeof(file))) return false;
  snprintf(temp,sizeof(temp),"%s.tmp",file);
  int fd=open(temp,O_WRONLY|O_CREAT|O_TRUNC,0600);
  bool ok=fd>=0 && write(fd,data,size)==(ssize_t)size;
  if(fd>=0) { if(ok) ok=fsync(fd)==0; close(fd); }
  if(ok) ok=rename(temp,file)==0;
  if(!ok) unlink(temp);
  return ok;
}
static unsigned char* blob_read(const wchar_t* path,size_t* size) {
  *size=0;
  char file[4200]; if(!narrow(path,file,sizeof(file))) return NULL;
  int fd=open(file,O_RDONLY); if(fd<0) return NULL;
  struct stat st; unsigned char* bytes=NULL;
  if(!fstat(fd,&st) && st.st_size>0 && st.st_size<=CREDENTIAL_MAX_FILE && (bytes=malloc((size_t)st.st_size))) {
    if(read(fd,bytes,(size_t)st.st_size)==(ssize_t)st.st_size) *size=(size_t)st.st_size;
    else { free(bytes); bytes=NULL; }
  }
  close(fd);
  return bytes;
}
bool credential_store_delete(const wchar_t* path) {
  char file[4200]; if(!narrow(path,file,sizeof(file))) return false;
  return unlink(file)==0 || errno==ENOENT;
}
#endif

#include <stdlib.h>
static bool key_ok(const char* key,size_t cap) { return memchr(key,0,cap) && strlen(key)>=16; }

bool credential_store_save(const wchar_t* path,const char* auth,const char* team) {
  if(strlen(auth)<16 || strlen(auth)>512 || strlen(team)<16 || strlen(team)>512) return false;
  stored_keys keys={0}; keys.version=1; strcpy(keys.auth,auth); strcpy(keys.team,team);
  bool ok=blob_write(path,&keys,sizeof(keys));
  wipe(&keys,sizeof(keys));
  return ok;
}
bool credential_store_load(const wchar_t* path,char auth[513],char team[513]) {
  auth[0]=team[0]=0;
  team_profiles* all=malloc(sizeof(*all)); if(!all) return false;
  bool ok=credential_store_load_profiles(path,all) && all->count>0;
  if(ok) {
    const team_profile* p=&all->items[all->selected>=0 && all->selected<all->count ? all->selected : 0];
    strcpy(auth,p->auth); strcpy(team,p->team);
  }
  wipe(all,sizeof(*all)); free(all);
  return ok;
}
bool credential_store_save_profiles(const wchar_t* path,const team_profiles* profiles) {
  if(profiles->count<0 || profiles->count>TEAM_PROFILE_MAX) return false;
  for(int i=0;i<profiles->count;i++) {
    const team_profile* p=&profiles->items[i];
    if(!memchr(p->name,0,sizeof(p->name)) || !key_ok(p->auth,sizeof(p->auth)) || !key_ok(p->team,sizeof(p->team))) return false;
  }
  size_t size=sizeof(stored_profiles_head)+(size_t)profiles->count*sizeof(team_profile);
  unsigned char* data=calloc(1,size); if(!data) return false;
  stored_profiles_head head={2,(uint32_t)profiles->count,profiles->selected};
  memcpy(data,&head,sizeof(head));
  memcpy(data+sizeof(head),profiles->items,(size_t)profiles->count*sizeof(team_profile));
  bool ok=blob_write(path,data,size);
  wipe(data,size); free(data);
  return ok;
}
bool credential_store_load_profiles(const wchar_t* path,team_profiles* out) {
  memset(out,0,sizeof(*out)); out->selected=-1;
  size_t size=0; unsigned char* data=blob_read(path,&size);
  if(!data) return false;
  bool ok=false;
  if(size==sizeof(stored_keys)) {                         // format 1: one pair, from older versions
    stored_keys keys; memcpy(&keys,data,sizeof(keys));
    if(keys.version==1 && key_ok(keys.auth,513) && key_ok(keys.team,513)) {
      out->count=1; out->selected=0;
      strcpy(out->items[0].name,"기본 팀키"); strcpy(out->items[0].auth,keys.auth); strcpy(out->items[0].team,keys.team);
      ok=true;
    }
    wipe(&keys,sizeof(keys));
  } else if(size>=sizeof(stored_profiles_head)) {
    stored_profiles_head head; memcpy(&head,data,sizeof(head));
    ok=head.version==2 && head.count<=TEAM_PROFILE_MAX && size==sizeof(head)+head.count*sizeof(team_profile);
    if(ok) {
      memcpy(out->items,data+sizeof(head),head.count*sizeof(team_profile));
      out->count=(int)head.count;
      out->selected=head.selected>=0 && head.selected<(int32_t)head.count ? head.selected : (head.count ? 0 : -1);
      for(int i=0;ok && i<out->count;i++) {
        team_profile* p=&out->items[i];
        ok=memchr(p->name,0,sizeof(p->name)) && key_ok(p->auth,sizeof(p->auth)) && key_ok(p->team,sizeof(p->team));
      }
    }
    if(!ok) { wipe(out,sizeof(*out)); out->selected=-1; }
  }
  wipe(data,size); free(data);
  return ok;
}
