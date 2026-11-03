#include "missile_worker.h"
#include "../external/mongoose.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
static HANDLE process,job,writer,reader;
#else
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
static pid_t process;
static int writer=-1,reader=-1;
#endif
static missile_worker_state state;
static char pending[8192];
static size_t pending_size;

static bool send_line(const char* line,size_t size) {
#ifdef _WIN32
  DWORD written=0;
  return writer && WriteFile(writer,line,(DWORD)size,&written,NULL) && written==size;
#else
  return writer>=0 && write(writer,line,size)==(ssize_t)size;
#endif
}
void missile_worker_stop(void) {
  if(state.running) send_line("{\"command\":\"stop\"}\n",19);
#ifdef _WIN32
  if(writer) CloseHandle(writer);
  if(process) {
    if(WaitForSingleObject(process,500)==WAIT_TIMEOUT) TerminateProcess(process,0);
    CloseHandle(process);
  }
  if(reader) CloseHandle(reader);
  if(job) CloseHandle(job);
  process=job=writer=reader=NULL;
#else
  if(writer>=0) close(writer);
  if(process>0) {
    bool exited=false;
    for(int i=0;i<50;i++) { if(waitpid(process,NULL,WNOHANG)!=0) {exited=true;break;} usleep(10000); }
    if(!exited) {kill(process,SIGKILL);waitpid(process,NULL,0);}
  }
  if(reader>=0) close(reader);
  writer=reader=-1;process=0;
#endif
  state.running=false;state.active=0;pending_size=0;
}
bool missile_worker_start(const char* server,float x,float y) {
  missile_worker_stop();memset(&state,0,sizeof(state));
#ifdef _WIN32
  SECURITY_ATTRIBUTES sa={sizeof(sa),NULL,TRUE};
  HANDLE child_in=NULL,child_out=NULL;
  if(!CreatePipe(&child_in,&writer,&sa,8192) || !CreatePipe(&reader,&child_out,&sa,65536)) goto fail;
  SetHandleInformation(writer,HANDLE_FLAG_INHERIT,0);SetHandleInformation(reader,HANDLE_FLAG_INHERIT,0);
  job=CreateJobObjectW(NULL,NULL);if(!job) goto fail;
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits={0};
  limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))) goto fail;
  STARTUPINFOW startup={.cb=sizeof(startup),.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW,.wShowWindow=SW_HIDE,
                       .hStdInput=child_in,.hStdOutput=child_out,.hStdError=child_out};
  PROCESS_INFORMATION pi={0};
  wchar_t path[32768];
  if(!GetFullPathNameW(L"app\\res\\snake-missile\\worker.exe",32768,path,NULL)) goto fail;
  if(!CreateProcessW(path,NULL,NULL,NULL,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,NULL,NULL,&startup,&pi)) goto fail;
  if(!AssignProcessToJobObject(job,pi.hProcess)) {
    TerminateProcess(pi.hProcess,1);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);goto fail;
  }
  process=pi.hProcess;ResumeThread(pi.hThread);CloseHandle(pi.hThread);
  CloseHandle(child_in);CloseHandle(child_out);
#else
  int in[2],out[2];
  if(pipe(in)) return false;
  if(pipe(out)) {close(in[0]);close(in[1]);return false;}
  process=fork();
  if(process==0) {
    dup2(in[0],STDIN_FILENO);dup2(out[1],STDOUT_FILENO);dup2(out[1],STDERR_FILENO);
    close(in[0]);close(in[1]);close(out[0]);close(out[1]);
    execl("app/res/snake-missile/worker","worker",(char*)NULL);_exit(127);
  }
  close(in[0]);close(out[1]);
  if(process<0) {close(in[1]);close(out[0]);process=0;return false;}
  writer=in[1];reader=out[0];
  fcntl(reader,F_SETFL,fcntl(reader,F_GETFL)|O_NONBLOCK);
  fcntl(writer,F_SETFL,fcntl(writer,F_GETFL)|O_NONBLOCK);
  fcntl(reader,F_SETFD,FD_CLOEXEC);fcntl(writer,F_SETFD,FD_CLOEXEC);
#endif
  state.running=true;strcpy(state.state,"connecting");
  char command[512];
  size_t n=mg_snprintf(command,sizeof(command),"{\"command\":\"start\",\"server\":%m,\"x\":%.2f,\"y\":%.2f}\n",mg_print_esc,0,server,x,y);
  if(n>=sizeof(command) || !send_line(command,n)) {missile_worker_stop();return false;}
  return true;
#ifdef _WIN32
fail:
  if(child_in) CloseHandle(child_in);if(child_out) CloseHandle(child_out);
  missile_worker_stop();return false;
#endif
}
void missile_worker_target(float x,float y) {
  if(!state.running || !state.total) return;
  char line[160];int n=snprintf(line,sizeof(line),"{\"command\":\"target\",\"x\":%.2f,\"y\":%.2f}\n",x,y);
  if(n>0 && n<(int)sizeof(line)) send_line(line,(size_t)n);
}
static void parse_status(char* line,size_t n) {
  struct mg_str json=mg_str_n(line,n);
  char* value=mg_json_get_str(json,"$.state");
  if(!value) return;
  snprintf(state.state,sizeof(state.state),"%s",value);free(value);
  state.active=(int)mg_json_get_long(json,"$.active",0);
  state.total=(int)mg_json_get_long(json,"$.total",0);
}
missile_worker_state missile_worker_poll(void) {
  if(!state.running) return state;
  char buffer[2048];
  for(int round=0;round<4;round++) {
    int received=0;
#ifdef _WIN32
    DWORD available=0,n=0;
    if(!PeekNamedPipe(reader,NULL,0,NULL,&available,NULL) || !available) break;
    if(!ReadFile(reader,buffer,available<sizeof(buffer)?available:sizeof(buffer),&n,NULL)) break;
    received=(int)n;
#else
    received=(int)read(reader,buffer,sizeof(buffer));if(received<=0) break;
#endif
    for(int i=0;i<received;i++) {
      if(buffer[i]=='\n') {pending[pending_size]=0;parse_status(pending,pending_size);pending_size=0;}
      else if(pending_size+1<sizeof(pending)) pending[pending_size++]=buffer[i];
      else pending_size=0;
    }
  }
#ifdef _WIN32
  bool exited=WaitForSingleObject(process,0)==WAIT_OBJECT_0;
#else
  bool exited=waitpid(process,NULL,WNOHANG)!=0;
  if(exited) process=0;
#endif
  if(exited) {missile_worker_stop();if(!state.state[0] || !strcmp(state.state,"connecting")) strcpy(state.state,"worker_failed");}
  return state;
}
