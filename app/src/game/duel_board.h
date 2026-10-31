#ifndef DUEL_BOARD_H
#define DUEL_BOARD_H
#include <thermite.h>
#include "../network/duel_record.h"
struct game_data; struct snake;
void duel_connected(void);                                              // a new life starts
void duel_snake_removed(struct game_data* game,struct snake* o,bool is_kill);  // before the snake is taken away
void duel_kill_confirmed(void);                                         // the server counted a kill for us
void duel_game_over(struct game_data* game);
// Our record against a named opponent; false when there is none yet (unnamed snakes share one
// line in the table, so they never get a label).
bool duel_board_record(const char* name,int* kills,int* deaths);
void duel_board_draw(tenv* env);                                        // shown while W is held
const duel_table* duel_board_table(void);
#endif
