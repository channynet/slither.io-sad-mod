#include "game/loop.h"
#include "ui/skin_editor.h"
#include "ui/title_screen.h"
#include "ui/settings.h"
#include "ui/viewport.h"
#include "user.h"
#include "network/ntl.h"
#include "game/native_preferences.h"
#include "game/native_shortcuts.h"
#include "ui/personal_tag.h"
#include "game/perf_trace.h"
#include "game/death_review.h"
#include "network/server.h"
#include "network/server_history.h"
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#endif

void tinput(tenv* env) {
  tuser_data* usr = env->usr;
  user_settings* usrs = &usr->usrs;

  if (twindow_closed(env->wnd)) {
    env->config.running = false;
    save_user_settings(usrs);
  }
  if (tkeyboard_key_pressed(env->kb, GLFW_KEY_F11)) {
    twindow_toggle_fullscreen(env->wnd);
    native_prefs.fullscreen=env->config.fullscreen;
    int key=GLFW_KEY_F11;
    int index=tdarray_find(env->kb->keys_pressed,&key);
    if(index>=0) tdarray_remove(env->kb->keys_pressed,index);
  }
}

void tlaunch(tenv* env) {
#ifdef _WIN32
  wchar_t executable[32768];
  DWORD length = GetModuleFileNameW(NULL, executable, 32768);
  if (length && length < 32768) {
    wchar_t* slash = wcsrchr(executable, L'\\');
    if (slash) { *slash = 0; _wchdir(executable); }
  }
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
  personal_tag_init(env);
}

void tdestroy(tenv* env) {
  native_preferences_tick(&env->usr->usrs,true);
  ntl_destroy();
  personal_tag_destroy(env);
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
  tuser_data* usr = env->usr;
  tcontext* ctx = env->ctx;
  game_data* gdata = &usr->gdata;

  // Input and network service continue even if the GPU cannot accept a frame.
  server_service(env);
  double wait_start=perf_now();
  bool frame_ready=tcontext_begin(ctx);
  perf_add(PERF_WAIT,wait_start);
  if (!frame_ready) return;
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
  if(!death_review.frozen) personal_tag_draw(env);
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
  native_preferences_tick(&usr->usrs,false);
  // render end

  igRender();
  double submit_start=perf_now();
  {
    if(!death_review.frozen) {
      renderer_render(usr->r, ctx, (vec4){0.086f, 0.109f, 0.133f, 1});
      if(gdata->curr_screen==PLAYING && gdata->conn==CONNECTED && gdata->data.follow_view && get_snake(gdata,gdata->data.snake_id))
        death_review.last_frame=ctx->current_frame;
    }
    tcontext_clear(ctx, (vec4){0, 0, 0, 1.0f});
    imgui_render(ctx->frames[ctx->current_frame].cmd);
    renderer_render_cursor(usr->r, ctx);
    tcontext_end(ctx);
  }
  perf_add(PERF_SUBMIT,submit_start);
  ctx->frame_submitted=true;
  perf_frame(usr->r->bpr->num_instances,usr->r->fdr->num_instances,ctx->present_mode,ctx->image_count);
  renderer_clear_instances(usr->r);
  death_review_finish(env);
}

void tidle(tenv* env) { ntl_pace(env); }

void tresize(tenv* env) { ui_viewport_resize(env); }

TDEF_ENTRY();
