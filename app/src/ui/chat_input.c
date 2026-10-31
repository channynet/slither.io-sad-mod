#include "chat_input.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "../cimgui/cimgui.h"
#include "../network/utf8_text.h"
#include <stdlib.h>

static int edit_callback(ImGuiInputTextCallbackData* data) {
  chat_edit* edit=data->UserData;
  if(edit->command) {
    int start=edit->start<edit->end?edit->start:edit->end;
    int end=edit->start>edit->end?edit->start:edit->end;
    if(start>data->BufTextLen) start=data->BufTextLen;
    if(end>data->BufTextLen) end=data->BufTextLen;
    int cursor=edit->cursor>data->BufTextLen?data->BufTextLen:edit->cursor;
    if(edit->command==CHAT_EDIT_SELECT_ALL) ImGuiInputTextCallbackData_SelectAll(data);
    else if(edit->command==CHAT_EDIT_COPY || edit->command==CHAT_EDIT_CUT) {
      if(start==end) { start=0; end=data->BufTextLen; }
      char* copy=malloc((size_t)(end-start)+1);
      if(copy) {
        memcpy(copy,data->Buf+start,end-start); copy[end-start]=0;
        igSetClipboardText(copy); free(copy);
        if(edit->command==CHAT_EDIT_CUT) ImGuiInputTextCallbackData_DeleteChars(data,start,end-start);
      }
    } else if(edit->command==CHAT_EDIT_PASTE) {
      const char* source=igGetClipboardText();
      if(source && *source) {
        // A chat message is one line. Keep complete UTF-8 characters at the limit.
        size_t room=(size_t)(data->BufSize-1-data->BufTextLen+end-start),used=0,left=strlen(source);
        char* paste=malloc(room+1);
        if(paste) {
          while(left) {
            size_t n=utf8_char_bytes(source,left); if(!n) break;
            if((unsigned char)*source<32 || (unsigned char)*source==127) {
              if(*source=='\r' || *source=='\n' || *source=='\t') {
                if(used>=room) break;
                if(!used || paste[used-1]!=' ') paste[used++]=' ';
              }
            } else { if(used+n>room) break; memcpy(paste+used,source,n); used+=n; }
            source+=n; left-=n;
          }
          paste[used]=0;
          if(used) {
            if(start!=end) ImGuiInputTextCallbackData_DeleteChars(data,start,end-start);
            else start=cursor;
            ImGuiInputTextCallbackData_InsertChars(data,start,paste,paste+used);
          }
          free(paste);
        }
      }
    }
    edit->command=CHAT_EDIT_NONE;
  }
  edit->cursor=data->CursorPos; edit->start=data->SelectionStart; edit->end=data->SelectionEnd;
  return 0;
}
bool chat_input(chat_edit* edit,char* text,int capacity,bool* typing) {
  bool restore=edit->command!=CHAT_EDIT_NONE;
  if(restore) igSetKeyboardFocusHere(0);
  // Only a click or an explicitly chosen editing menu command starts typing.
  // Tab/Enter navigation must not silently turn gameplay keys into chat text.
  igPushItemFlag(ImGuiItemFlags_NoNav,!restore);
  bool send=igInputTextWithHint("##team_message","클릭해서 채팅 · Enter 전송",text,capacity,
      ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_CallbackAlways,edit_callback,edit);
  igPopItemFlag();
  *typing=igIsItemActive();
  if(igBeginPopupContextItem("chat-edit",ImGuiPopupFlags_MouseButtonRight)) {
    if(igMenuItem_Bool(edit->start==edit->end?"전체 복사":"선택 복사","Ctrl+C",false,text[0]!=0)) edit->command=CHAT_EDIT_COPY;
    if(igMenuItem_Bool("잘라내기","Ctrl+X",false,text[0]!=0)) edit->command=CHAT_EDIT_CUT;
    if(igMenuItem_Bool("붙여넣기","Ctrl+V",false,true)) edit->command=CHAT_EDIT_PASTE;
    if(igMenuItem_Bool("전체 선택","Ctrl+A",false,true)) edit->command=CHAT_EDIT_SELECT_ALL;
    *typing=true;
    igEndPopup();
  }
  return send;
}
