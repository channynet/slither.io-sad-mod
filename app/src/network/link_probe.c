#include "link_probe.h"
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <wlanapi.h>

static HANDLE thread,stop;
static volatile LONG gateway_us=-1,signal_quality=-1,scan_count,roam_count,tuned_bits,wanted_tuning;
static HANDLE wlan;
static GUID wlan_guid;
static bool wlan_connected,wlan_present;

static void WINAPI wlan_event(PWLAN_NOTIFICATION_DATA data,PVOID context) {
  (void)context;
  if(!data) return;
  if(data->NotificationSource==WLAN_NOTIFICATION_SOURCE_ACM) {
    if(data->NotificationCode==wlan_notification_acm_scan_complete ||
       data->NotificationCode==wlan_notification_acm_scan_fail) InterlockedIncrement(&scan_count);
  } else if(data->NotificationSource==WLAN_NOTIFICATION_SOURCE_MSM) {
    if(data->NotificationCode==wlan_notification_msm_roaming_start) InterlockedIncrement(&roam_count);
  }
}
static void wlan_find(void) {
  wlan_connected=wlan_present=false;
  if(!wlan) return;
  PWLAN_INTERFACE_INFO_LIST list=NULL;
  if(WlanEnumInterfaces(wlan,NULL,&list)!=ERROR_SUCCESS || !list) return;
  wlan_present=list->dwNumberOfItems>0;
  for(DWORD i=0;i<list->dwNumberOfItems;i++)
    if(list->InterfaceInfo[i].isState==wlan_interface_state_connected) {
      wlan_guid=list->InterfaceInfo[i].InterfaceGuid; wlan_connected=true; break;
    }
  WlanFreeMemory(list);
}
// Both options belong to this handle only; Windows restores them when it closes.
static LONG wlan_tune(bool on) {
  if(!wlan || !wlan_connected) return 0;
  LONG bits=0; BOOL streaming=on,background=!on;
  if(WlanSetInterface(wlan,&wlan_guid,wlan_intf_opcode_media_streaming_mode,sizeof(streaming),&streaming,NULL)==ERROR_SUCCESS && on) bits|=1;
  if(WlanSetInterface(wlan,&wlan_guid,wlan_intf_opcode_background_scan_enabled,sizeof(background),&background,NULL)==ERROR_SUCCESS && on) bits|=2;
  return bits;
}
// 0..100, or why there is no value: -1 no Wi-Fi adapter, -2 adapter present but
// not connected (wired link), -3 connected but Windows refused the query.
static LONG wlan_signal(void) {
  if(!wlan || !wlan_present) return -1;
  if(!wlan_connected) return -2;
  DWORD size=0; PWLAN_CONNECTION_ATTRIBUTES attributes=NULL;
  if(WlanQueryInterface(wlan,&wlan_guid,wlan_intf_opcode_current_connection,NULL,&size,(PVOID*)&attributes,NULL)!=ERROR_SUCCESS || !attributes) return -3;
  LONG quality=size>=sizeof(*attributes)?(LONG)attributes->wlanAssociationAttributes.wlanSignalQuality:-3;
  WlanFreeMemory(attributes);
  return quality;
}
static DWORD gateway_address(void) {
  MIB_IPFORWARDROW row;
  // Route towards a public address; its next hop is the home router.
  if(GetBestRoute(0x08080808,0,&row)!=NO_ERROR) return 0;
  return row.dwForwardNextHop==0x08080808?0:row.dwForwardNextHop;
}
static LONG gateway_ping(HANDLE icmp,DWORD address) {
  if(icmp==INVALID_HANDLE_VALUE || !address) return -1;
  char payload[8]="slither",reply[sizeof(ICMP_ECHO_REPLY)+sizeof(payload)+8];
  LARGE_INTEGER frequency,start,end;
  QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&start);
  DWORD replies=IcmpSendEcho(icmp,address,payload,sizeof(payload),NULL,reply,sizeof(reply),800);
  QueryPerformanceCounter(&end);
  if(!replies || ((PICMP_ECHO_REPLY)reply)->Status!=IP_SUCCESS) return -1;
  return (LONG)((end.QuadPart-start.QuadPart)*1000000/frequency.QuadPart);
}
static DWORD WINAPI worker(void* unused) {
  (void)unused;
  DWORD version=0;
  if(WlanOpenHandle(2,NULL,&version,&wlan)!=ERROR_SUCCESS) wlan=NULL;
  if(wlan) WlanRegisterNotification(wlan,WLAN_NOTIFICATION_SOURCE_ACM|WLAN_NOTIFICATION_SOURCE_MSM,TRUE,wlan_event,NULL,NULL,NULL);
  HANDLE icmp=IcmpCreateFile();
  DWORD gateway=0; LONG applied=0; unsigned tick=0;
  for(;;) {
    ULONGLONG started=GetTickCount64();
    if(tick%30==0) {
      gateway=gateway_address();
      bool was=wlan_connected; wlan_find();
      if(wlan_connected!=was) applied=0;  // a new connection starts with default options
    }
    LONG wanted=InterlockedCompareExchange(&wanted_tuning,0,0);
    if(wanted!=applied || (wanted && tick%30==0)) {
      InterlockedExchange(&tuned_bits,wlan_tune(wanted!=0)); applied=wanted;
    }
    InterlockedExchange(&signal_quality,wlan_signal());
    InterlockedExchange(&gateway_us,gateway_ping(icmp,gateway));
    tick++;
    ULONGLONG spent=GetTickCount64()-started;
    if(WaitForSingleObject(stop,spent<1000?(DWORD)(1000-spent):1)==WAIT_OBJECT_0) break;
  }
  if(applied) wlan_tune(false);
  if(icmp!=INVALID_HANDLE_VALUE) IcmpCloseHandle(icmp);
  if(wlan) WlanCloseHandle(wlan,NULL);
  wlan=NULL;
  return 0;
}
void link_probe_init(void) {
  if(thread) return;
  stop=CreateEventW(NULL,TRUE,FALSE,NULL);
  if(stop) thread=CreateThread(NULL,0,worker,NULL,0,NULL);
}
void link_probe_destroy(void) {
  if(stop) SetEvent(stop);
  if(thread) { WaitForSingleObject(thread,3000); CloseHandle(thread); }
  if(stop) CloseHandle(stop);
  thread=stop=NULL;
}
void link_probe_read(link_sample* out) {
  LONG us=InterlockedCompareExchange(&gateway_us,0,0);
  out->gateway_ms=us<0?-1:us/1000.0f;
  out->signal=(int)InterlockedCompareExchange(&signal_quality,0,0);
  out->scans=(unsigned)InterlockedCompareExchange(&scan_count,0,0);
  out->roams=(unsigned)InterlockedCompareExchange(&roam_count,0,0);
  out->tuned=(unsigned)InterlockedCompareExchange(&tuned_bits,0,0);
}
void link_probe_low_latency(bool wanted) { InterlockedExchange(&wanted_tuning,wanted?1:0); }

#else
// macOS / others: no router ping or Wi-Fi tuning; the fields read as "unknown".
void link_probe_init(void) {}
void link_probe_destroy(void) {}
void link_probe_read(link_sample* out) { *out=(link_sample){.gateway_ms=-1,.signal=-1}; }
void link_probe_low_latency(bool wanted) { (void)wanted; }
#endif
