#include "auto_update.h"
#include <stdbool.h>
#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#ifndef APP_VERSION
#define APP_VERSION "unknown"
#endif
void auto_update_startup(void) {
  wchar_t skip[8];
  bool skip_once=GetEnvironmentVariableW(L"SLITHER_SKIP_UPDATE_ONCE",skip,8)!=0;
  SetEnvironmentVariableW(L"SLITHER_SKIP_UPDATE_ONCE",NULL);
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
  int count=swprintf(command,32768,L"\"%ls\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -File \"%ls\" -CurrentVersion \"%ls\" -ParentId %lu -ReadyEvent \"%ls\" %ls",shell,script,version,GetCurrentProcessId(),event_name,skip_once?L"-SkipCheck":L"");
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
#elif defined(__APPLE__)
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#ifndef APP_VERSION
#define APP_VERSION "unknown"
#endif
static int run_mac_updater(const char* helper,const char* app,const char* mode) {
  pid_t child=fork();
  if(child==0) { execl(helper,helper,app,APP_VERSION,mode,(char*)NULL); _exit(1); }
  if(child<0) return 1;
  int status=0;
  while(waitpid(child,&status,0)<0) { if(errno!=EINTR) return 1; }
  return WIFEXITED(status)?WEXITSTATUS(status):1;
}
static void exec_mac_client(const char* exe,int fd) {
  char descriptor[32]; snprintf(descriptor,sizeof(descriptor),"%d",fd);
  setenv("SLITHER_SKIP_UPDATE_ONCE","1",1);
  setenv("SLITHER_UPDATE_LOCK_FD",descriptor,1);
  fcntl(fd,F_SETFD,0); // Keep the installation locked across exec.
  char* argv[]={(char*)exe,NULL};
  execv(exe,argv);
}
void auto_update_startup(void) {
  bool skip_once=getenv("SLITHER_SKIP_UPDATE_ONCE")!=NULL;
  unsetenv("SLITHER_SKIP_UPDATE_ONCE");
  // Isolated tests and unpackaged development binaries never update an installation.
  if(getenv("SLITHER_DATA_DIR")) return;
  char raw[PATH_MAX],exe[PATH_MAX],app[PATH_MAX],helper[PATH_MAX],lock[PATH_MAX];
  uint32_t size=sizeof(raw);
  if(_NSGetExecutablePath(raw,&size) || !realpath(raw,exe)) return;
  const char* suffix="/Contents/MacOS/SlitherNative";
  size_t len=strlen(exe),tail=strlen(suffix);
  if(len<=tail || strcmp(exe+len-tail,suffix)) return;
  memcpy(app,exe,len-tail); app[len-tail]=0;
  if(strlen(app)<4 || strcmp(app+strlen(app)-4,".app")) return;
  if(snprintf(helper,sizeof(helper),"%s/Contents/MacOS/SlitherUpdater",app)>=(int)sizeof(helper) ||
     snprintf(lock,sizeof(lock),"%s.update-lock",app)>=(int)sizeof(lock)) return;
  int fd=-1;
  const char* inherited=getenv("SLITHER_UPDATE_LOCK_FD");
  if(skip_once && inherited) {
    char* end=NULL; long value=strtol(inherited,&end,10);
    struct stat held,path;
    if(end && !*end && value>=3 && value<=INT_MAX &&
       !fstat((int)value,&held) && !lstat(lock,&path) &&
       S_ISREG(held.st_mode) && held.st_uid==getuid() &&
       held.st_dev==path.st_dev && held.st_ino==path.st_ino) fd=(int)value;
  }
  unsetenv("SLITHER_UPDATE_LOCK_FD");
  if(fd<0) fd=open(lock,O_CREAT|O_RDWR|O_NOFOLLOW|O_CLOEXEC,0600);
  if(fd<0) return; // Read-only/translocated installations continue without updating.
  fcntl(fd,F_SETFD,FD_CLOEXEC);
  struct stat info;
  if(fstat(fd,&info) || !S_ISREG(info.st_mode) || info.st_uid!=getuid()) { close(fd); return; }
  if(skip_once) { if(flock(fd,LOCK_SH)) close(fd); return; }
  // Each running app retains a shared lock. Only a solitary startup can update.
  bool exclusive=flock(fd,LOCK_EX|LOCK_NB)==0;
  if(!exclusive) {
    if(flock(fd,LOCK_SH)) { close(fd); return; }
    // Another startup may have exchanged the bundle while we waited for its lock.
    exec_mac_client(exe,fd);
    _exit(1);
  }
  if(run_mac_updater(helper,app,"--update")==10) {
    exec_mac_client(exe,fd);
    // exec failed: atomically restore the previous bundle, then restart it.
    if(run_mac_updater(helper,app,"--rollback")==0) exec_mac_client(exe,fd);
    _exit(1); // Never run old code with resources from an unrecoverable replacement.
  }
  flock(fd,LOCK_SH); // Retained until process exit; settings remain outside the bundle.
}
#else
void auto_update_startup(void) {}
#endif
