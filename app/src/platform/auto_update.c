#include "auto_update.h"
#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#ifndef APP_VERSION
#define APP_VERSION "unknown"
#endif
void auto_update_startup(void) {
  wchar_t skip[8];
  if(GetEnvironmentVariableW(L"SLITHER_SKIP_UPDATE_ONCE",skip,8)) {
    SetEnvironmentVariableW(L"SLITHER_SKIP_UPDATE_ONCE",NULL);
    return;
  }
  wchar_t exe[32768],script[32768],shell[MAX_PATH],command[32768],event_name[128];
  DWORD n=GetModuleFileNameW(NULL,exe,32768);
  if(!n || n>=32768) return;
  wchar_t* slash=wcsrchr(exe,L'\\'); if(!slash) return;
  // Renamed development/test executables must not update the installed app.
  if(_wcsicmp(slash+1,L"SlitherNative.exe")) return;
  *slash=0;
  if(swprintf(script,32768,L"%ls\\update.ps1",exe)<0 || GetFileAttributesW(script)==INVALID_FILE_ATTRIBUTES) return;
  n=GetSystemDirectoryW(shell,MAX_PATH);
  if(!n || n+50>=MAX_PATH) return;
  wcscat(shell,L"\\WindowsPowerShell\\v1.0\\powershell.exe");
  swprintf(event_name,128,L"Local\\SlitherUpdate-%lu-%llu",GetCurrentProcessId(),GetTickCount64());
  HANDLE ready=CreateEventW(NULL,TRUE,FALSE,event_name); if(!ready) return;
  wchar_t version[64]; MultiByteToWideChar(CP_UTF8,0,APP_VERSION,-1,version,64);
  int count=swprintf(command,32768,L"\"%ls\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -File \"%ls\" -CurrentVersion \"%ls\" -ParentId %lu -ReadyEvent \"%ls\"",shell,script,version,GetCurrentProcessId(),event_name);
  STARTUPINFOW si={.cb=sizeof(si),.dwFlags=STARTF_USESHOWWINDOW,.wShowWindow=SW_HIDE};
  PROCESS_INFORMATION pi={0};
  if(count>0 && CreateProcessW(shell,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,exe,&si,&pi)) {
    HANDLE waits[]={ready,pi.hProcess};
    // The worker only modifies installed files after this process exits.
    DWORD result=WaitForMultipleObjects(2,waits,FALSE,180000);
    if(result==WAIT_OBJECT_0) {
      CloseHandle(pi.hThread); CloseHandle(pi.hProcess); CloseHandle(ready);
      ExitProcess(0);
    }
    if(result==WAIT_TIMEOUT || result==WAIT_FAILED) {
      TerminateProcess(pi.hProcess,1); WaitForSingleObject(pi.hProcess,5000);
    }
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
  }
  CloseHandle(ready);
}
#else
void auto_update_startup(void) {}
#endif
