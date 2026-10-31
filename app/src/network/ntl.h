#ifndef NTL_H
#define NTL_H
#include <thermite.h>
#include "../game/user_settings.h"
void ntl_init(void);
void ntl_destroy(void);
void ntl_frame(tenv* env);
void ntl_pace(tenv* env);
bool ntl_mouse_captured(void);
bool ntl_chat_typing(void);
bool ntl_is_teammate(tenv* env,int sid);
void ntl_lightweight_settings(user_settings* settings);
// Servers teammates are playing on now (from the last fresh NTL reply), most teammates first.
typedef struct { char address[68]; char names[120]; int count; } ntl_team_server;
int ntl_team_servers(ntl_team_server* out,int capacity);
// Go to a teammate's server: connect from the menu, or leave the current game and reconnect there.
bool ntl_join_server(tenv* env,const char* address);
// NTL SOS flag (S twice while playing): teammates see the player in red and get an alert.
void ntl_toggle_sos(void);
bool ntl_sos(void);
// A teammate's NTL tag (from the team list) by game snake NTL id: -1 none, -2 not a teammate here.
int ntl_teammate_tag(tenv* env,int sid);
// The NTL identity (8-letter key + nickname) this client uses with NTL's servers.
const char* ntl_own_identity(tenv* env);
// Empties the team chat shown (NTL: - twice); messages arriving later still show.
void ntl_clear_chat(void);
#endif
