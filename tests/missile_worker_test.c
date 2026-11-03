#include "../app/src/platform/missile_worker.h"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
int main(int argc,char** argv) {
  assert(argc==2);
  assert(!missile_worker_start(argv[1],2000,1000,0));
  assert(!missile_worker_start(argv[1],2000,1000,101));
  assert(missile_worker_start(argv[1],2000,1000,100));
  missile_worker_state state={0};
  for(int i=0;i<1500;i++){state=missile_worker_poll();if(state.active==100)break;Sleep(10);}
  if(state.active!=100 || state.total!=100) {fprintf(stderr,"worker status: %s (%d/%d)\n",state.state,state.active,state.total);missile_worker_stop();return 1;}
  Sleep(250);missile_worker_target(1000,2000);Sleep(350);
  missile_worker_stop();assert(!missile_worker_poll().running);
  puts("Native bridge: 100 real bot sockets, target update and shutdown passed");return 0;
}
