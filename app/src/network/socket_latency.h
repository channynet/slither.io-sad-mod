#ifndef SOCKET_LATENCY_H
#define SOCKET_LATENCY_H
#include <stdbool.h>
#ifdef _WIN32
#include <winsock2.h>
#include <mstcpip.h>
#else
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
typedef int SOCKET;
#endif
static inline bool socket_prompt_ack(SOCKET socket) {
  int on=1;
  if(setsockopt(socket,IPPROTO_TCP,TCP_NODELAY,(const char*)&on,sizeof(on))) return false;
#ifdef _WIN32
  TCP_ACK_FREQUENCY_PARAMETERS ack={.TcpDelayedAckFrequency=1}; DWORD bytes=0;
  // Per connection only. Unsupported systems keep their normal ACK policy.
  return WSAIoctl(socket,SIO_TCP_SET_ACK_FREQUENCY,&ack,sizeof(ack),NULL,0,&bytes,NULL,NULL)==0;
#elif defined(TCP_QUICKACK)
  return setsockopt(socket,IPPROTO_TCP,TCP_QUICKACK,&on,sizeof(on))==0;
#else
  return false;                                  // macOS has no per-socket delayed-ACK switch
#endif
}
typedef struct { float rtt_ms,min_rtt_ms; unsigned retrans; } socket_latency;
static inline socket_latency socket_latency_read(SOCKET socket) {
  socket_latency out={-1,-1,0};
#ifdef _WIN32
#if NTDDI_VERSION >= NTDDI_WIN10_RS2
  TCP_INFO_v0 info={0}; DWORD version=0,bytes=0;
  if(!WSAIoctl(socket,SIO_TCP_INFO,&version,sizeof(version),&info,sizeof(info),&bytes,NULL,NULL) && bytes>=sizeof(info)) {
    out.rtt_ms=info.RttUs/1000.0f; out.min_rtt_ms=info.MinRttUs/1000.0f; out.retrans=info.BytesRetrans;
  }
#endif
#elif defined(__APPLE__)
  struct tcp_connection_info info; socklen_t size=sizeof(info);
  if(!getsockopt(socket,IPPROTO_TCP,TCP_CONNECTION_INFO,&info,&size)) {
    // macOS reports the smoothed RTT in ms; its public struct has no best/minimum RTT field.
    out.rtt_ms=(float)info.tcpi_srtt; out.min_rtt_ms=(float)info.tcpi_srtt;
    out.retrans=(unsigned)info.tcpi_txretransmitbytes;
  }
#elif defined(TCP_INFO)
  struct tcp_info info; socklen_t size=sizeof(info);
  if(!getsockopt(socket,IPPROTO_TCP,TCP_INFO,&info,&size)) {
    out.rtt_ms=info.tcpi_rtt/1000.0f; out.min_rtt_ms=info.tcpi_rtt/1000.0f; out.retrans=info.tcpi_total_retrans;
  }
#endif
  return out;
}
#endif
