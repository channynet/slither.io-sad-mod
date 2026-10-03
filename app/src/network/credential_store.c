#include "credential_store.h"
#include <windows.h>
#include <wincrypt.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef struct { uint32_t version; char auth[513],team[513]; } stored_keys;
bool credential_store_path(wchar_t* path,size_t capacity) {
  if(capacity<64 || capacity>32768) return false;
  DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",path,(DWORD)capacity);
  if(!n || n+64>=capacity) return false;
  wcscat(path,L"\\SlitherNative");
  if(!CreateDirectoryW(path,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS) return false;
  wcscat(path,L"\\team-keys.dat"); return true;
}
bool credential_store_save(const wchar_t* path,const char* auth,const char* team) {
  if(strlen(auth)<16 || strlen(auth)>512 || strlen(team)<16 || strlen(team)>512 || wcslen(path)>32000) return false;
  stored_keys keys={0}; keys.version=1; strcpy(keys.auth,auth); strcpy(keys.team,team);
  DATA_BLOB input={sizeof(keys),(BYTE*)&keys},encrypted={0};
  bool ok=CryptProtectData(&input,L"Slither Native team keys",NULL,NULL,NULL,CRYPTPROTECT_UI_FORBIDDEN,&encrypted);
  SecureZeroMemory(&keys,sizeof(keys)); if(!ok) return false;
  wchar_t temp[32768]; swprintf(temp,32768,L"%ls.tmp",path);
  HANDLE file=CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
  DWORD written=0;
  ok=file!=INVALID_HANDLE_VALUE && WriteFile(file,encrypted.pbData,encrypted.cbData,&written,NULL) && written==encrypted.cbData;
  if(file!=INVALID_HANDLE_VALUE) { if(ok) ok=FlushFileBuffers(file); CloseHandle(file); }
  LocalFree(encrypted.pbData);
  if(ok) ok=MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
  if(!ok) DeleteFileW(temp);
  return ok;
}
bool credential_store_load(const wchar_t* path,char auth[513],char team[513]) {
  auth[0]=team[0]=0;
  HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
  if(file==INVALID_HANDLE_VALUE) return false;
  DWORD length=GetFileSize(file,NULL),read=0; BYTE bytes[8192];
  bool ok=length>0 && length<=sizeof(bytes) && ReadFile(file,bytes,length,&read,NULL) && read==length;
  CloseHandle(file); if(!ok) return false;
  DATA_BLOB input={length,bytes},plain={0};
  ok=CryptUnprotectData(&input,NULL,NULL,NULL,NULL,CRYPTPROTECT_UI_FORBIDDEN,&plain);
  SecureZeroMemory(bytes,sizeof(bytes)); if(!ok) return false;
  stored_keys* keys=(stored_keys*)plain.pbData;
  ok=plain.cbData==sizeof(stored_keys) && keys->version==1 && memchr(keys->auth,0,513) && memchr(keys->team,0,513);
  if(ok) ok=strlen(keys->auth)>=16 && strlen(keys->team)>=16;
  if(ok) { strcpy(auth,keys->auth); strcpy(team,keys->team); }
  SecureZeroMemory(plain.pbData,plain.cbData); LocalFree(plain.pbData);
  return ok;
}
bool credential_store_delete(const wchar_t* path) {
  return DeleteFileW(path) || GetLastError()==ERROR_FILE_NOT_FOUND;
}
