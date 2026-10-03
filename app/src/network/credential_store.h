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
#endif
