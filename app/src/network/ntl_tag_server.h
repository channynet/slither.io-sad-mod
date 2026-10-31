#ifndef NTL_TAG_SERVER_H
#define NTL_TAG_SERVER_H
// NTL's tag server (ws://ws.ntl-slither.com:9000) and its password check, on their own threads.
// While playing, this client says who it is (NTL identity, game server, the bought tag it wears
// with its password hash, its game snake's NTL id and position) as NTL does, and the server
// answers with which NTL id wears which bought tag: those tags are then drawn on those snakes.
#include <thermite.h>
#include <stdbool.h>
#include <stddef.h>

// Every frame: connects while playing on a public game server (other players' tags shown, or a
// bought tag worn), sends the position once a second, drops the connection otherwise.
void ntl_tag_server_tick(tenv* env);
void ntl_tag_server_destroy(void);
// "Connected", "Connecting", "Off" and the like, for the settings.
const char* ntl_tag_server_status(void);

// Checks a bought tag's password with NTL (the answer comes back on a thread).
void ntl_tag_password_check(int id, const char* password);
// 0 nothing asked, 1 waiting, 2 accepted (the hash is stored), 3 refused or failed; msg: NTL's word.
int ntl_tag_password_result(char* msg, size_t capacity);
// Asks NTL once whether the stored passwords still hold (at start); refused ones are dropped.
void ntl_tag_passwords_recheck(void);
#endif
