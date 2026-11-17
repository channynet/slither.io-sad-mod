#include "../app/src/platform/swarm_worker.c"
#include <assert.h>

int main(void) {
  char failed[]="{\"state\":\"connection_failed\",\"total\":1,\"waiting\":1,\"detail\":\"websocket: bad handshake\\nHTTP rejected\"}";
  parse_status(failed,strlen(failed));
  assert(state.waiting==1 && !strcmp(state.detail,"websocket: bad handshake HTTP rejected"));
  parse_status(failed,strlen(failed));
  FILE* log=fopen("swarm-log.txt","r");assert(log);
  char line[512];assert(fgets(line,sizeof(line),log));
  assert(strstr(line,"connection_failed: websocket: bad handshake HTTP rejected"));
  assert(!fgets(line,sizeof(line),log));fclose(log);
  char active[]="{\"state\":\"active\",\"active\":1,\"total\":1}";
  parse_status(active,strlen(active));assert(state.active==1 && !state.detail[0]);
  puts("Swarm status: failure details retained, log sanitized and deduplicated, stale details cleared");
}
