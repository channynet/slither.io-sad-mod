#ifndef CREDENTIAL_STORE_H
#define CREDENTIAL_STORE_H
#include <stdbool.h>
#include <stddef.h>
#include <wchar.h>
// DPAPI binds the encrypted file to the current Windows account.
bool credential_store_path(wchar_t* path,size_t capacity);
bool credential_store_save(const wchar_t* path,const char* auth,const char* team);
bool credential_store_load(const wchar_t* path,char auth[513],char team[513]);
bool credential_store_delete(const wchar_t* path);
// Several named key pairs, as NTL's "Saved keys"; the selected one is used when connecting.
#define TEAM_PROFILE_MAX 16
typedef struct { char name[65], auth[513], team[513]; } team_profile;
typedef struct { int count, selected; team_profile items[TEAM_PROFILE_MAX]; } team_profiles;
bool credential_store_save_profiles(const wchar_t* path,const team_profiles* profiles);
// Also reads a file saved by older versions (one pair) as a single profile.
bool credential_store_load_profiles(const wchar_t* path,team_profiles* profiles);
#endif
