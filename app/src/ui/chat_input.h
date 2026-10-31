#ifndef CHAT_INPUT_H
#define CHAT_INPUT_H
#include <stdbool.h>
enum { CHAT_EDIT_NONE, CHAT_EDIT_COPY, CHAT_EDIT_CUT, CHAT_EDIT_PASTE, CHAT_EDIT_SELECT_ALL };
typedef struct { int command,cursor,start,end; } chat_edit;
#ifdef __cplusplus
extern "C" {
#endif
bool chat_input(chat_edit* edit,char* text,int capacity,bool* typing);
#ifdef __cplusplus
}
#endif
#endif
