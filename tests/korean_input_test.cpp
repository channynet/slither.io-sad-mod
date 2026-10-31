// Exercise the real Win32 -> GLFW -> ImGui text path without focusing a
// visible window, contacting the team service, or changing the user's IME.
#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include "../app/src/cimgui/imgui/imgui.h"
#include "../app/src/cimgui/imgui/imgui_internal.h"
#include "../app/src/cimgui/imgui/imgui_impl_glfw.h"
#include "../app/src/ui/chat_input.h"
#include "../app/src/ui/text_input_ime.h"
#include <imm.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

static char draft[901];
static bool submitted;
static chat_edit editor;
static std::string clipboard;
static bool draw_composition;

static void frame(bool focus = false, ImVec2 position = ImVec2(40, 220), bool outside = false)
{
    ImGui_ImplGlfw_NewFrame();
    static bool release_mouse;
    if(focus || outside) {
        ImGui::GetIO().AddMousePosEvent(outside?700:position.x+20,outside?500:position.y+198);
        ImGui::GetIO().AddMouseButtonEvent(0,true);
        release_mouse=true;
    } else if(release_mouse) {
        ImGui::GetIO().AddMouseButtonEvent(0,false); release_mouse=false;
    }
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(ImVec2(280, 230));
    ImGui::Begin("chat", nullptr, ImGuiWindowFlags_NoTitleBar);
    ImGui::SetCursorPosY(185);
    bool typing;
    submitted = chat_input(&editor,draft,sizeof(draft),&typing);
    ImGui::End();
    if(draw_composition) text_input_ime_draw();
    ImGui::Render();
    // No GPU needed. Acknowledge atlas uploads as a renderer would.
    for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
    {
        if (tex->Status == ImTextureStatus_WantCreate)
            tex->SetTexID((ImTextureID)1);
        if (tex->Status != ImTextureStatus_OK)
            tex->SetStatus(ImTextureStatus_OK);
    }
}

static void key(HWND hwnd,int code,bool down) {
    LPARAM scan=(LPARAM)MapVirtualKeyW(code,MAPVK_VK_TO_VSC)<<16;
    SendMessageW(hwnd,down?WM_KEYDOWN:WM_KEYUP,code,scan|(down?1:(LPARAM)0xC0000001));
    frame();
}
static void shortcut(HWND hwnd,int code) {
    key(hwnd,VK_CONTROL,true); key(hwnd,code,true);
    key(hwnd,code,false); key(hwnd,VK_CONTROL,false);
}

static void check_caret(HIMC himc)
{
    const ImGuiPlatformImeData& data = ImGui::GetCurrentContext()->PlatformImeData;
    assert(data.WantVisible);
    COMPOSITIONFORM composition = {};
    CANDIDATEFORM candidate = {};
    assert(ImmGetCompositionWindow(himc, &composition));
    assert(ImmGetCandidateWindow(himc, 0, &candidate));
    const ImVec2 origin = ImGui::GetMainViewport()->Pos;
    assert(composition.dwStyle == CFS_FORCE_POSITION);
    assert(composition.ptCurrentPos.x == (LONG)(data.InputPos.x - origin.x));
    assert(composition.ptCurrentPos.y == (LONG)(data.InputPos.y - origin.y));
    assert(candidate.ptCurrentPos.x == composition.ptCurrentPos.x);
    assert(candidate.ptCurrentPos.y == composition.ptCurrentPos.y);
    assert(composition.ptCurrentPos.x > 0 && composition.ptCurrentPos.y > 300);
}

int main()
{
    assert(glfwInit());
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Korean input regression", nullptr, nullptr);
    assert(window);
    HWND hwnd = glfwGetWin32Window(window);
    HIMC himc = ImmCreateContext();
    assert(himc);
    HIMC previous = ImmAssociateContext(hwnd, himc);
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    char font[MAX_PATH];
    GetWindowsDirectoryA(font, MAX_PATH);
    strcat(font, "/Fonts/malgun.ttf");
    assert(io.Fonts->AddFontFromFileTTF(font, 18));
    assert(ImGui_ImplGlfw_InitForOther(window, true));
    text_input_ime_install();
    assert(ImmGetContext(hwnd)==nullptr);
    // Exercise shortcuts and the real editing callback without overwriting
    // the user's system clipboard (the backend's Unicode callbacks stay intact in production).
    ImGui::GetPlatformIO().Platform_GetClipboardTextFn=[](ImGuiContext*) { return clipboard.c_str(); };
    ImGui::GetPlatformIO().Platform_SetClipboardTextFn=[](ImGuiContext*,const char* text) { clipboard=text; };
    assert(ImGui::GetMainViewport()->PlatformHandleRaw == hwnd);
    frame();
    SendMessageW(hwnd,WM_CHAR,'t',1); frame(); assert(!draft[0]);
    key(hwnd,'T',true); assert(ImGui::IsKeyDown(ImGuiKey_T)); key(hwnd,'T',false);
    key(hwnd,VK_RETURN,true); key(hwnd,VK_RETURN,false);
    assert(!draft[0] && ImmGetContext(hwnd)==nullptr);
    frame(true); frame();
    HIMC current=ImmGetContext(hwnd); assert(current==himc); ImmReleaseContext(hwnd,current);
    check_caret(himc);

    // The composition window is ours: these messages are answered here (0), a syllable in
    // progress is kept for drawing, nothing is typed until text is finished, and ending or
    // losing the composition clears it. (No IME runs in the test, so the strings are empty.)
    assert(SendMessageW(hwnd,WM_IME_STARTCOMPOSITION,0,0)==0);
    assert(SendMessageW(hwnd,WM_IME_COMPOSITION,0,GCS_COMPSTR)==0);
    assert(SendMessageW(hwnd,WM_IME_COMPOSITION,0,GCS_RESULTSTR|GCS_COMPSTR)==0);
    assert(SendMessageW(hwnd,WM_IME_COMPOSITION,0,0)==0);
    assert(SendMessageW(hwnd,WM_IME_ENDCOMPOSITION,0,0)==0);
    assert(SendMessageW(hwnd,WM_IME_CHAR,0xD55C,1)==0);
    SendMessageW(hwnd,WM_IME_SETCONTEXT,TRUE,ISC_SHOWUIALL);
    frame();
    assert(!draft[0] && !text_input_ime_composing()[0]);
    text_input_ime_draw();   // nothing to draw, and no crash

    // Where the input context can hold a composition (it can under Wine and with a real IME),
    // walk a syllable through it the way the IME does: in progress it is only kept for drawing,
    // finished it is typed exactly once.
    {
        wchar_t part[]=L"하", full[]=L"한", read[8];
        if(ImmSetCompositionStringW(himc,SCS_SETSTR,part,sizeof(wchar_t),NULL,0) &&
           ImmGetCompositionStringW(himc,GCS_COMPSTR,read,sizeof(read))==(LONG)sizeof(wchar_t)) {
            SendMessageW(hwnd,WM_IME_STARTCOMPOSITION,0,0);
            SendMessageW(hwnd,WM_IME_COMPOSITION,0,GCS_COMPSTR); frame();
            assert(!strcmp(text_input_ime_composing(),u8"하") && !draft[0]);
            ImmSetCompositionStringW(himc,SCS_SETSTR,full,sizeof(wchar_t),NULL,0);
            SendMessageW(hwnd,WM_IME_COMPOSITION,0,GCS_COMPSTR); frame();
            assert(!strcmp(text_input_ime_composing(),u8"한") && !draft[0]);
            draw_composition=true; frame(); draw_composition=false;         // draws without a crash
            if(ImmNotifyIME(himc,NI_COMPOSITIONSTR,CPS_COMPLETE,0) &&
               ImmGetCompositionStringW(himc,GCS_RESULTSTR,read,sizeof(read))==(LONG)sizeof(wchar_t)) {
                SendMessageW(hwnd,WM_IME_COMPOSITION,0,GCS_RESULTSTR);
                SendMessageW(hwnd,WM_IME_ENDCOMPOSITION,0,0); frame(); frame();
                assert(!text_input_ime_composing()[0] && !strcmp(draft,u8"한"));
                // leave the box, empty it, and click back in for the checks below
                frame(false,ImVec2(40,220),true); frame(); draft[0]=0; frame(true); frame();
                std::puts("Korean composition: in-progress syllable kept for drawing, finished syllable typed once");
            } else std::puts("Korean composition: finishing could not be simulated here");
            ImmNotifyIME(himc,NI_COMPOSITIONSTR,CPS_CANCEL,0); SendMessageW(hwnd,WM_IME_ENDCOMPOSITION,0,0);
        } else std::puts("Korean composition: this system keeps no composition without an IME; not simulated");
    }

    // Completed Korean syllables follow WM_CHAR, not an 8-bit key callback.
    const wchar_t* korean = L"한글 입력";
    for (const wchar_t* p = korean; *p; ++p) SendMessageW(hwnd, WM_CHAR, *p, 1);
    frame();
    assert(strcmp(draft, u8"한글 입력") == 0);
    assert(!submitted);
    assert(io.Fonts->Fonts[0]->GetFontBaked(18)->FindGlyphNoFallback(0xD55C));
    check_caret(himc);

    // IME-consumed Enter must not submit chat; GLFW filters VK_PROCESSKEY.
    const LPARAM enter_scan = (LPARAM)MapVirtualKeyW(VK_RETURN, MAPVK_VK_TO_VSC) << 16;
    SendMessageW(hwnd, WM_KEYDOWN, VK_PROCESSKEY, enter_scan | 1);
    frame();
    assert(!submitted && strcmp(draft, u8"한글 입력") == 0);

    // Moving chat updates both composition and candidate positions.
    frame(false, ImVec2(310, 240));
    check_caret(himc);
    assert(ImGui::GetCurrentContext()->PlatformImeData.InputPos.x > 300);

    // Backspace removes one whole syllable; a normal Enter can then send.
    SendMessageW(hwnd, WM_KEYDOWN, VK_BACK, (LPARAM)MapVirtualKeyW(VK_BACK, MAPVK_VK_TO_VSC) << 16 | 1);
    frame();
    assert(strcmp(draft, u8"한글 입") == 0);
    SendMessageW(hwnd, WM_KEYUP, VK_BACK, (LPARAM)MapVirtualKeyW(VK_BACK, MAPVK_VK_TO_VSC) << 16 | (LPARAM)0xC0000001);
    frame();
    SendMessageW(hwnd, WM_KEYDOWN, VK_RETURN, enter_scan | 1);
    frame();
    assert(submitted && strcmp(draft, u8"한글 입") == 0);
    assert(ImmGetContext(hwnd)==nullptr);
    key(hwnd,VK_RETURN,false);
    frame(true); frame();
    shortcut(hwnd,'A'); shortcut(hwnd,'C');
    assert(clipboard==u8"한글 입");
    clipboard=u8"복사 붙여넣기 English 123";
    shortcut(hwnd,'V'); assert(strcmp(draft,clipboard.c_str())==0);
    shortcut(hwnd,'A'); shortcut(hwnd,'X'); assert(!draft[0]);
    shortcut(hwnd,'V'); assert(strcmp(draft,clipboard.c_str())==0);

    // The context-menu commands share the production callback, with selection
    // restored after popup focus is returned to the input field.
    editor.command=CHAT_EDIT_SELECT_ALL; frame(); frame();
    clipboard=u8"첫째\r\n둘째\t셋째";
    editor.command=CHAT_EDIT_PASTE; frame(); frame();
    assert(strcmp(draft,u8"첫째 둘째 셋째")==0);
    editor.command=CHAT_EDIT_SELECT_ALL; frame(); frame();
    editor.command=CHAT_EDIT_COPY; frame(); frame();
    assert(clipboard==u8"첫째 둘째 셋째");
    editor.command=CHAT_EDIT_CUT; frame(); frame(); assert(!draft[0]);
    clipboard="a"; for(int i=0;i<400;i++) clipboard+=u8"한";
    editor.command=CHAT_EDIT_PASTE; frame(); frame();
    assert(strlen(draft)==898 && draft[897]==(char)0x9c);
    editor.command=CHAT_EDIT_SELECT_ALL; frame(); frame();
    editor.command=CHAT_EDIT_CUT; frame(); frame();
    shortcut(hwnd,'V'); assert(strlen(draft)==898);
    const std::string preserved=draft;
    frame(false,ImVec2(40,220),true); frame();
    assert(ImmGetContext(hwnd)==nullptr && !io.WantTextInput);
    SendMessageW(hwnd,WM_CHAR,'v',1); frame(); assert(preserved==draft);
    key(hwnd,VK_TAB,true); key(hwnd,VK_TAB,false);
    key(hwnd,VK_RETURN,true); key(hwnd,VK_RETURN,false);
    assert(ImmGetContext(hwnd)==nullptr);
    frame(true); frame();
    current=ImmGetContext(hwnd); assert(current==himc); ImmReleaseContext(hwnd,current);
    key(hwnd,VK_ESCAPE,true); key(hwnd,VK_ESCAPE,false); frame();
    assert(ImmGetContext(hwnd)==nullptr && !io.WantTextInput);
    text_input_ime_restore();
    current=ImmGetContext(hwnd); assert(current==himc); ImmReleaseContext(hwnd,current);
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    ImmAssociateContext(hwnd, previous);
    ImmDestroyContext(himc);
    glfwDestroyWindow(window);
    glfwTerminate();
    puts("Korean input: click-only activation, idle gameplay keys, IME gating, outside/Escape/Enter exit, Unicode, clipboard and UTF-8 limits passed");
}
