#include "text_input_ime.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "../cimgui/cimgui.h"
#include <float.h>
#include <string.h>
static bool enabled;
static void (*previous)(ImGuiContext*,ImGuiViewport*,ImGuiPlatformImeData*);
static char composing[64];       // the syllable being put together, UTF-8
static ImVec2 caret; static float caret_height;
const char* text_input_ime_composing(void) { return enabled ? composing : ""; }
void text_input_ime_draw(void) {
  if(!enabled || !composing[0] || caret_height<=0) return;
  ImDrawList* draw=igGetForegroundDrawList_ViewportPtr(NULL);
  ImFont* font=igGetFont(); ImVec2 size;
  ImFont_CalcTextSizeA(&size,font,caret_height,FLT_MAX,0,composing,NULL,NULL);
  ImVec2 at={caret.x+1,caret.y};
  ImDrawList_AddRectFilled(draw,(ImVec2){at.x-1,at.y},(ImVec2){at.x+size.x+1,at.y+caret_height+2},
      igColorConvertFloat4ToU32((ImVec4){.10f,.11f,.13f,1}),0,0);
  ImDrawList_AddText_FontPtr(draw,font,caret_height,at,igColorConvertFloat4ToU32((ImVec4){1,1,1,1}),composing,NULL,0,NULL);
  ImDrawList_AddLine(draw,(ImVec2){at.x,at.y+caret_height+1},(ImVec2){at.x+size.x,at.y+caret_height+1},
      igColorConvertFloat4ToU32((ImVec4){1,1,1,.9f}),1);
}

#ifdef _WIN32
#include <windows.h>
#include <imm.h>

static HWND window;
static HIMC context;
static WNDPROC window_proc;

// Windows shows a syllable that is still being composed in a small window of its own. Each
// time that window appears over ours the whole picture flashes (the display stops showing our
// frames directly and goes back to composing the desktop). So that window is never opened:
// we take the composition messages ourselves, draw the syllable at the caret, and hand the
// finished text to the input box.
static void take(HIMC himc,DWORD kind,bool commit) {
  wchar_t text[32];
  LONG bytes=ImmGetCompositionStringW(himc,kind,text,sizeof(text)-sizeof(wchar_t));
  int count=bytes>0 ? (int)(bytes/sizeof(wchar_t)) : 0;
  if(commit) {
    for(int i=0;i<count;i++) ImGuiIO_AddInputCharacterUTF16(igGetIO_Nil(),(ImWchar16)text[i]);
    return;
  }
  text[count]=0;
  int written=WideCharToMultiByte(CP_UTF8,0,text,count,composing,sizeof(composing)-1,NULL,NULL);
  composing[written>0 ? written : 0]=0;
}
static LRESULT CALLBACK ime_proc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
  switch(message) {
    case WM_IME_SETCONTEXT:
      lparam&=~(LPARAM)ISC_SHOWUICOMPOSITIONWINDOW;
      break;
    case WM_IME_STARTCOMPOSITION:
      composing[0]=0;
      return 0;
    case WM_IME_COMPOSITION: {
      HIMC himc=ImmGetContext(hwnd);
      if(himc) {
        if(lparam&GCS_RESULTSTR) take(himc,GCS_RESULTSTR,true);
        if(lparam&GCS_COMPSTR) take(himc,GCS_COMPSTR,false); else composing[0]=0;
        ImmReleaseContext(hwnd,himc);
      } else composing[0]=0;
      return 0;
    }
    case WM_IME_ENDCOMPOSITION:
      composing[0]=0;
      return 0;
    case WM_IME_CHAR:   // finished text was already taken above
      return 0;
  }
  return CallWindowProcW(window_proc,hwnd,message,wparam,lparam);
}
static void update_ime(ImGuiContext* ctx,ImGuiViewport* viewport,ImGuiPlatformImeData* data) {
  if(viewport->PlatformHandleRaw==window && enabled!=data->WantTextInput) {
    if(!data->WantTextInput && context) ImmNotifyIME(context,NI_COMPOSITIONSTR,CPS_CANCEL,0);
    ImmAssociateContext(window,data->WantTextInput?context:NULL);
    enabled=data->WantTextInput;
    if(!enabled) composing[0]=0;
  }
  if(data->WantTextInput) { caret=data->InputPos; caret_height=data->InputLineHeight; }
  if(data->WantTextInput && previous) previous(ctx,viewport,data);
}
void text_input_ime_install(void) {
  window=igGetMainViewport()->PlatformHandleRaw;
  if(!window) return;
  // Preserve the user's existing input context; do not change the keyboard
  // layout or the system-wide Korean/English preference.
  context=ImmAssociateContext(window,NULL);
  enabled=false;
  ImGuiPlatformIO* platform=igGetPlatformIO_Nil();
  previous=platform->Platform_SetImeDataFn;
  platform->Platform_SetImeDataFn=update_ime;
  composing[0]=0;
  window_proc=(WNDPROC)SetWindowLongPtrW(window,GWLP_WNDPROC,(LONG_PTR)ime_proc);
}
void text_input_ime_restore(void) {
  if(!window) return;
  if(window_proc) SetWindowLongPtrW(window,GWLP_WNDPROC,(LONG_PTR)window_proc);
  window_proc=NULL; composing[0]=0;
  igGetPlatformIO_Nil()->Platform_SetImeDataFn=previous;
  ImmAssociateContext(window,context);
  window=NULL; context=NULL; previous=NULL; enabled=false;
}

#elif defined(__APPLE__)
// macOS: the input method composes Hangul in the window itself (no separate window as on
// Windows). It runs only while a text box has focus, so game keys never feed a composition,
// and the syllable being composed is drawn at the caret like on Windows. See the GLFW changes
// in glfw/src/cocoa_window.m (glfwSetCocoaTextInputSN, glfwSetCocoaPreeditCallbackSN).
#include <GLFW/glfw3.h>
void glfwSetCocoaTextInputSN(GLFWwindow* window,int enabled);
void glfwSetCocoaPreeditCallbackSN(GLFWwindow* window,void (*callback)(GLFWwindow*,const char*));
void glfwSetCocoaImeCaretSN(GLFWwindow* window,float x,float y,float height);
static GLFWwindow* window;
static void preedit(GLFWwindow* w,const char* text) {
  (void)w;
  size_t n=strlen(text); if(n>=sizeof(composing)) n=0;          // a syllable is a few bytes
  memcpy(composing,text,n); composing[n]=0;
}
static void update_ime(ImGuiContext* ctx,ImGuiViewport* viewport,ImGuiPlatformImeData* data) {
  if(viewport->PlatformHandle==window && enabled!=data->WantTextInput) {
    glfwSetCocoaTextInputSN(window,data->WantTextInput);
    enabled=data->WantTextInput;
    if(!enabled) composing[0]=0;
  }
  if(data->WantTextInput) {
    caret=data->InputPos; caret_height=data->InputLineHeight;
    glfwSetCocoaImeCaretSN(window,caret.x,caret.y,caret_height);   // framebuffer = points here
  }
  if(previous) previous(ctx,viewport,data);
}
void text_input_ime_install(void) {
  window=(GLFWwindow*)igGetMainViewport()->PlatformHandle;
  if(!window) return;
  enabled=false; composing[0]=0;
  glfwSetCocoaTextInputSN(window,0);
  glfwSetCocoaPreeditCallbackSN(window,preedit);
  ImGuiPlatformIO* platform=igGetPlatformIO_Nil();
  previous=platform->Platform_SetImeDataFn;
  platform->Platform_SetImeDataFn=update_ime;
}
void text_input_ime_restore(void) {
  if(!window) return;
  glfwSetCocoaPreeditCallbackSN(window,NULL);
  glfwSetCocoaTextInputSN(window,1);
  igGetPlatformIO_Nil()->Platform_SetImeDataFn=previous;
  window=NULL; previous=NULL; enabled=false; composing[0]=0;
}
#else
// Linux test build: GLFW hands over finished characters only.
void text_input_ime_install(void) { (void)previous; }
void text_input_ime_restore(void) {}
#endif
