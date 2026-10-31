#ifndef NTL_TAGS_ONLINE_H
#define NTL_TAGS_ONLINE_H
// NTL's online tag lists, as the NTL mod fetches them at start: its bought tags
// (https://ntl-slither.com/tags/cstags, worn as 580, 579, ...) and the public tags added after
// the ones it ships (tags/fstags, after 303). Fetched on a thread, pictures turned into PNG and
// kept with their catalog in ntl-tags-online/ (so the next start has them at once).
#include <stdbool.h>
#define NTL_TAGS_ONLINE_DIR "ntl-tags-online"
// The lists as they were when this client was made (NTL's server refuses requests from outside a
// browser, so this is often all there is): used for a list not fetched here.
#define NTL_TAGS_ONLINE_BUNDLED "app/res/ntl-tags-online"
// Adds the lists kept from last time, then starts fetching newer ones (Windows; macOS: kept only).
void ntl_tags_online_start(void);
// True once (on the main thread) when newer lists were fetched and kept: add them again then.
bool ntl_tags_online_take(void);
// Adds the kept lists to the catalog.
void ntl_tags_online_merge(void);
// For the settings: what happened ("216개 받음", "연결 안 됨" ...).
const char* ntl_tags_online_status(void);
#endif
