#include "game/loop.h"
#include "platform/auto_update.h"
#include "ui/text_input_ime.h"
#include "ui/skin_editor.h"
#include "ui/title_screen.h"
#include "ui/settings.h"
#include "ui/viewport.h"
#include "user.h"
#include "network/ntl.h"
#include "game/native_preferences.h"
#include "game/native_shortcuts.h"
#include "ui/personal_tag.h"
#include "ui/ntl_tags_ui.h"
#include "network/ntl_tag_server.h"
#include "network/tag_share.h"
#include "ui/head_face_assets.h"
#include "network/link_probe.h"
#include "game/perf_trace.h"
#include "game/death_review.h"
#include "network/server.h"
#include "network/server_history.h"
#include "network/server_status.h"
#include "network/server_catalog.h"
#include "network/client_access.h"
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
// Where the game keeps its files outside Windows. Next to the program when the game files are there
// (a portable folder, a test build); inside SlitherNative.app the bundle stays read-only, so the
// settings and logs go to ~/Library/Application Support/SlitherNative with a link "app" to the
// bundle's game files, and every relative path in the client works unchanged.
static bool make_folders(char* path) {          // mkdir -p
  for(char* c=path+1;*c;c++) if(*c=='/') { *c=0; mkdir(path,0700); *c='/'; }
  return mkdir(path,0700)==0 || errno==EEXIST;
}
static bool has_game_files(const char* dir) {
  char probe[PATH_MAX+64]; snprintf(probe,sizeof(probe),"%s/app/res/shaders/bin/bstv.spv",dir);
  return access(probe,R_OK)==0;
}
static void enter_data_folder(void) {
  signal(SIGPIPE,SIG_IGN);        // a write to a closed connection must not end the game
  const char* forced=getenv("SLITHER_DATA_DIR");
  if(forced && *forced && chdir(forced)==0) return;
  char exe[PATH_MAX]={0},real[PATH_MAX];
#ifdef __APPLE__
  uint32_t size=sizeof(exe); if(_NSGetExecutablePath(exe,&size)) return;
#else
  ssize_t n=readlink("/proc/self/exe",exe,sizeof(exe)-1); if(n<=0) return; exe[n]=0;
#endif
  if(!realpath(exe,real)) return;
  char* slash=strrchr(real,'/'); if(!slash) return; *slash=0;            // the program's folder
  if(has_game_files(real)) { chdir(real); return; }
  char resources[PATH_MAX+32]; snprintf(resources,sizeof(resources),"%s/../Resources",real);
  char bundle[PATH_MAX]; if(!realpath(resources,bundle) || !has_game_files(bundle)) return;
  const char* home=getenv("HOME"); if(!home || !*home) { chdir(bundle); return; }
  char data[PATH_MAX];
#ifdef __APPLE__
  snprintf(data,sizeof(data),"%s/Library/Application Support/SlitherNative",home);
#else
  snprintf(data,sizeof(data),"%s/.local/share/SlitherNative",home);
#endif
  if(!make_folders(data)) { chdir(bundle); return; }
  if(chdir(data)) { chdir(bundle); return; }
  char target[PATH_MAX+8]; snprintf(target,sizeof(target),"%s/app",bundle);
  char now[PATH_MAX]; ssize_t len=readlink("app",now,sizeof(now)-1);
  if(len<0 || (size_t)len!=strlen(target) || memcmp(now,target,(size_t)len)) {   // first run, or the app moved
    unlink("app");
    if(symlink(target,"app")) chdir(bundle);
  }
}
#endif

void tinput(tenv* env) {
  tuser_data* usr = env->usr;
  user_settings* usrs = &usr->usrs;

  if (twindow_closed(env->wnd)) {
    env->config.running = false;
    save_user_settings(usrs);
  }
#ifdef __APPLE__
  // F11 is "Show Desktop" on a Mac; control+command+F is the system's full-screen key.
  bool mac_fullscreen_key = tkeyboard_key_pressed(env->kb, GLFW_KEY_F) &&
      (twindow_key_down(env->wnd, GLFW_KEY_LEFT_SUPER) || twindow_key_down(env->wnd, GLFW_KEY_RIGHT_SUPER)) &&
      (twindow_key_down(env->wnd, GLFW_KEY_LEFT_CONTROL) || twindow_key_down(env->wnd, GLFW_KEY_RIGHT_CONTROL));
  if (mac_fullscreen_key) twindow_toggle_fullscreen(env->wnd);
  env->config.fullscreen = native_prefs.fullscreen = twindow_is_fullscreen(env->wnd);
#endif
  if (tkeyboard_key_pressed(env->kb, GLFW_KEY_F11)) {
    twindow_toggle_fullscreen(env->wnd);
    native_prefs.fullscreen=env->config.fullscreen;
    int key=GLFW_KEY_F11;
    int index=tdarray_find(env->kb->keys_pressed,&key);
    if(index>=0) tdarray_remove(env->kb->keys_pressed,index);
  }
}

void tlaunch(tenv* env) {
  auto_update_startup();
#ifdef _WIN32
  wchar_t executable[32768];
  DWORD length = GetModuleFileNameW(NULL, executable, 32768);
  if (length && length < 32768) {
    wchar_t* slash = wcsrchr(executable, L'\\');
    if (slash) { *slash = 0; _wchdir(executable); }
  }
#else
  enter_data_folder();
#endif
  tuser_data* usr = env->usr;
  user_settings* usrs = &usr->usrs;
  srand(time(NULL));
  usr->render_scale = 0.75f;

  memset(usrs, 0, sizeof(user_settings));
  strcpy(usrs->ipv4, "15.204.212.200:444");
  strcpy(usrs->nickname, "");

  usrs->custom_skin = false;
  usrs->default_skin = rand() % 9;
  usrs->accessory = NO_ACCESSORY;

  read_user_settings(usrs);
  native_preferences_init(usrs);
  server_history_init(usrs->ipv4);
  const float scales[]={1,0.75f,0.5f};
  usr->render_scale=scales[native_prefs.render_scale];

  env->config.vsync = usrs->vsync;
  env->config.fullscreen = native_prefs.fullscreen;
  env->config.title = "Slither Native - NTL";
  env->config.fif = 2;
}

void tinit(tenv* env) {
  tuser_data* usr = env->usr;
  perf_trace_init();

  imgui_init(env);
  env->usr->r = renderer_create(env);
  ui_viewport_init(env);
  ui_title_screen_init(env);
  ui_skin_editor_init(env);
  ui_settings_init(env);
  game_data_init(env);
  ntl_init();
  server_status_init();
  server_catalog_init();
  client_access_init();
  link_probe_init();
  personal_tag_init(env);
  ntl_tags_init(env);
  head_face_init(env);
}

void tdestroy(tenv* env) {
  native_preferences_tick(&env->usr->usrs,true);
  ntl_destroy();
  server_status_destroy();
  server_catalog_destroy();
  client_access_destroy();
  link_probe_destroy();
  head_face_destroy(env);
  personal_tag_destroy(env);
  ntl_tags_destroy(env);
  game_data_destroy(env);
  ui_settings_destroy(env);
  ui_skin_editor_destroy(env);
  ui_title_screen_destroy(env);
  ui_viewport_destroy(env);
  renderer_destroy(env->usr->r, env->ctx);
  imgui_destroy();
  perf_trace_destroy();
}

void trender(tenv* env) {
  perf_since_mark(PERF_EVENTS);
  perf_window(glfwGetWindowAttrib(env->wnd->handle,GLFW_FOCUSED));
  server_status_menu(env->usr->gdata.curr_screen==TITLE_SCREEN && env->usr->gdata.conn==DISCONNECTED);
  tuser_data* usr = env->usr;
  tcontext* ctx = env->ctx;
  game_data* gdata = &usr->gdata;

  // Input and network service continue even if the GPU cannot accept a frame.
  server_service(env);
  double wait_start=perf_now();
  bool frame_ready=tcontext_begin(ctx);
  perf_add(PERF_WAIT,wait_start);
  if (!frame_ready) { perf_mark(); return; }
  double cursor_x, cursor_y;
  glfwGetCursorPos(env->wnd->handle,&cursor_x,&cursor_y);
  env->ms->pos[0]=(float)cursor_x;
  env->ms->pos[1]=(float)cursor_y;

  const float scales[]={1,0.75f,0.5f};
  usr->render_scale=scales[native_prefs.render_scale];
  if (usr->r->size[0] != GLM_MAX(1,(int)(ctx->size[0]*usr->render_scale)) ||
      usr->r->size[1] != GLM_MAX(1,(int)(ctx->size[1]*usr->render_scale)))
    ui_viewport_resize(env);
  personal_tag_prepare(env);
  ntl_tags_prepare(env);
  imgui_prerender();
  native_shortcuts(env);
  // render begin
  ImGuiStyle* style = igGetStyle();

  igSetNextWindowPos(igGetMainViewport()->Pos, ImGuiCond_None, (ImVec2){});
  igSetNextWindowSize(igGetMainViewport()->Size, ImGuiCond_None);
  igPushStyleVar_Float(ImGuiStyleVar_WindowBorderSize, 0);
  igBegin("##fullscreen_holder", NULL,
          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
              ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
              ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings);
  igPopStyleVar(1);
  switch (usr->gdata.curr_screen) {
    case TITLE_SCREEN:
      ui_title_screen(env);
      break;
    case SKIN_EDITOR:
      ui_skin_editor(env);
      break;
    case PLAYING:
      game_loop(env);
      break;
    case SETTINGS:
      ui_settings(env);
      break;
  }
  if(!death_review.frozen) { ntl_tags_draw(env); personal_tag_draw(env); }
  igEnd();
  // A death packet may arrive inside game_loop; select the held image afterwards.
  ui_viewport(env);
  if(gdata->curr_screen!=PLAYING) native_settings_open=false;
  if(native_settings_open) {
    screen previous=gdata->curr_screen;
    igSetNextWindowPos(igGetMainViewport()->Pos,ImGuiCond_Always,(ImVec2){0});
    igSetNextWindowSize(igGetMainViewport()->Size,ImGuiCond_Always);
    igBegin("Settings overlay",NULL,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoSavedSettings);
    ui_settings(env);
    if(gdata->curr_screen!=previous) native_settings_open=false;
    gdata->curr_screen=previous;
    igEnd();
  }
  ntl_frame(env);
  ntl_tag_server_tick(env);
  tag_share_tick(env);
  native_preferences_tick(&usr->usrs,false);
  // render end
  text_input_ime_draw();

  igRender();
  double submit_start=perf_now();
  {
    if(!death_review.frozen) {
      double game_start=perf_now();
      renderer_render(usr->r, ctx, (vec4){0.086f, 0.109f, 0.133f, 1});
      perf_add(PERF_DRAW_GAME,game_start);
      if(gdata->curr_screen==PLAYING && gdata->conn==CONNECTED && gdata->data.follow_view && get_snake(gdata,gdata->data.snake_id))
        death_review.last_frame=ctx->current_frame;
    }
    tcontext_clear(ctx, (vec4){0, 0, 0, 1.0f});
    double ui_start=perf_now();
    renderer_gpu_ui(usr->r, ctx, false);
    imgui_render(ctx->frames[ctx->current_frame].cmd);
    renderer_gpu_ui(usr->r, ctx, true);
    perf_add(PERF_DRAW_UI,ui_start);
    renderer_render_cursor(usr->r, ctx);
    perf_add(PERF_COMMANDS,submit_start);
    tcontext_end(ctx);
    perf_add_duration(PERF_QUEUE_SUBMIT,ctx->queue_submit_ms);
    perf_add_duration(PERF_PRESENT,ctx->present_ms);
  }
  perf_add(PERF_SUBMIT,submit_start);
  ctx->frame_submitted=true;
  perf_context(gdata->curr_screen==PLAYING && gdata->conn==CONNECTED && !death_review.frozen);
  link_sample link; link_probe_read(&link);
  link_probe_low_latency(native_prefs.wifi_low_latency && gdata->curr_screen==PLAYING);
  perf_link(link.gateway_ms,link.signal,link.scans,link.roams,link.tuned);
  perf_frame(usr->r->bpr->num_instances,usr->r->fdr->num_instances,ctx->present_mode,ctx->image_count);
  renderer_clear_instances(usr->r);
  death_review_finish(env);
}

void tidle(tenv* env) { ntl_pace(env); }
void tbackground(tenv* env) {
  static double last; double now=perf_now();
  if(last && now-last<1000) perf_minimized(now-last);   // called about every 16 ms while minimized
  last=now;
  game_background(env);
}

void tresize(tenv* env) { ui_viewport_resize(env); }

TDEF_ENTRY();
