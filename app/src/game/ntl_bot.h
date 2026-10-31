#ifndef NTL_BOT_H
#define NTL_BOT_H
#include <thermite.h>
// Runs the NTL-style bot (ntl_bot_core) on the live game while the bot key is on, and leaves
// its aim and boost in gdata->bot.output for the input code to send.
void ntl_bot_go(tenv* env);
// 0 grow, 1 curling up, 2 circling; -1 while the bot is off.
int ntl_bot_stage(void);
#endif
