#include "../app/src/network/path_choice.h"
#include <assert.h>
#include <stdio.h>
static bool open_and_check(path_choice* p,const char* address,float rtt,double now) {
  path_choice_opened(p,now); return path_choice_retry(p,address,rtt,now);
}
int main(void) {
  path_choice p={0}; double t=100000;
  // First contact: one extra sample, then the faster of the two is kept.
  path_choice_begin(&p);
  assert(open_and_check(&p,"1.2.3.4:444",38,t));
  assert(!open_and_check(&p,"1.2.3.4:444",40,t+100));
  // Known address on the slow path: retry until fast, at most three connections.
  t+=60000; path_choice_begin(&p);
  assert(open_and_check(&p,"1.2.3.4:444",65,t));
  assert(!open_and_check(&p,"1.2.3.4:444",39,t+100));
  t+=60000; path_choice_begin(&p);
  assert(open_and_check(&p,"1.2.3.4:444",66,t));
  assert(open_and_check(&p,"1.2.3.4:444",64,t+100));
  assert(!open_and_check(&p,"1.2.3.4:444",67,t+200));
  // Fast first try: no extra connection. Small jitter is not a slow path.
  t+=60000; path_choice_begin(&p);
  assert(!open_and_check(&p,"1.2.3.4:444",49,t));
  // First contact slow then fast keeps the fast one; slow twice then gives up on the third.
  t+=60000; path_choice_begin(&p);
  assert(open_and_check(&p,"5.6.7.8:444",65,t));
  assert(!open_and_check(&p,"5.6.7.8:444",38,t+100));
  // Addresses are independent, ports included.
  t+=60000; path_choice_begin(&p);
  assert(open_and_check(&p,"1.2.3.4:443",90,t));
  // Unknown round-trip time (unsupported system): never retry.
  t+=60000; path_choice_begin(&p);
  assert(!open_and_check(&p,"1.2.3.4:444",-1,t) && !open_and_check(&p,"1.2.3.4:444",0,t));
  assert(!path_choice_retry(&p,NULL,50,t));
  // Rapid respawns: no more than five connections in twenty seconds because of retries.
  t+=60000; int opened=0;
  for(int life=0;life<6;life++) {
    path_choice_begin(&p);
    for(;;) { opened++; if(!open_and_check(&p,"1.2.3.4:444",66,t+life*1000+opened)) break; }
  }
  assert(opened==3+2+1+1+1+1);  // retries stop once five connections fall inside the window
  // After the window passes, retries are available again.
  t+=60000; path_choice_begin(&p);
  assert(open_and_check(&p,"1.2.3.4:444",66,t));
  // More addresses than slots: the table recycles without overflow.
  for(int i=0;i<40;i++) { char a[24]; snprintf(a,sizeof(a),"10.0.0.%d:444",i); path_choice_begin(&p); open_and_check(&p,a,40,t+100000+i*30000.0); }
  assert(p.count==PATH_SLOTS);
  puts("Path choice: first-contact sample, slow-path retry, attempt limit, jitter margin, per-address state and rate limit passed");
}
