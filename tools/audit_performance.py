"""Read historical logs and benchmark current source without launching the game.

No credentials, settings, servers or production binaries are touched. Generated
harness/results stay under build/performance-audit. UI figures are CPU-only.
"""
import csv
import hashlib
import json
import re
import statistics
from datetime import datetime, timezone

from build_windows import ROOT, BIN, BUILD, run

OUT = ROOT / 'build/performance-audit'
STAGES = ['wait', 'network', 'update', 'geometry', 'submit', 'pacing', 'bot',
          'commands', 'queue_submit', 'present']


def summarize(rows):
    n = sum(int(r['frames']) for r in rows)
    if not n:
        return {'frames': 0}
    mean = sum(int(r['frames']) * float(r['mean_ms']) for r in rows) / n
    return {
        'frames': n, 'estimated_seconds': n * mean / 1000,
        'mean_ms': mean, 'mean_fps': 1000 / mean,
        'over_33_34_ms': sum(int(r['over33']) for r in rows),
        'max_ms': max(float(r['max_ms']) for r in rows),
        'stage_mean_ms': {s: sum(int(r['frames']) * float(r[s+'_ms']) for r in rows) / n for s in STAGES},
        'median_of_one_second_last_ping_samples_ms': statistics.median(float(r['last_ping_ms']) for r in rows),
        'max_ping_ms': max(float(r['max_ping_ms']) for r in rows),
        'max_backlog_bytes': max(int(r['max_backlog_bytes']) for r in rows),
        'worst_buckets': sorted(rows, key=lambda r: float(r['max_ms']), reverse=True)[:5],
    }


HARNESS = r'''
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_NO_EXPORT
#include "app/src/cimgui/cimgui.h"
#include "app/src/ui/team_status.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
// Only the environment fields read by the extracted production UI function.
enum { CONNECTED=1 };
typedef struct { struct {int size[2];} *ctx; struct {struct {int conn;} gdata; struct {char ipv4[161];} usrs;} *usr; } tenv;
static struct {bool team_status;} native_prefs={true};
static team_status_view roster;
static ntl_reply reply;
static bool active=true,demo=false,options_open,capture_mouse;
static ULONGLONG updated;
static char visible_server[161]="synthetic.example:444";
static LARGE_INTEGER frequency;
static volatile int sink;
static double now_ms(void) { LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart*1000.0/frequency.QuadPart; }
/* PRODUCTION_UI */
static int compare(const void* a,const void* b) { double x=*(const double*)a,y=*(const double*)b; return (x>y)-(x<y); }
static void frame(tenv* env) {
  updated=GetTickCount64();
  igNewFrame(); draw_team_status(env); igRender();
  ImDrawData* data=igGetDrawData(); sink+=data->TotalVtxCount;
  // Emulate successful font texture uploads; no graphics device or GPU timing.
  if(data->Textures) for(int i=0;i<data->Textures->Size;i++) {
    ImTextureData* t=data->Textures->Data[i];
    if(t->Status==ImTextureStatus_WantCreate || t->Status==ImTextureStatus_WantUpdates) {
      ImTextureData_SetTexID(t,1); ImTextureData_SetStatus(t,ImTextureStatus_OK);
    }
  }
}
int main(void) {
  QueryPerformanceFrequency(&frequency);
  igCreateContext(NULL); ImGuiIO* io=igGetIO_Nil();
  io->IniFilename=NULL; io->LogFilename=NULL;
  io->BackendFlags|=ImGuiBackendFlags_RendererHasTextures;
  io->DisplaySize=(ImVec2){2560,1600}; io->DeltaTime=1.0f/60;
  char font[MAX_PATH]; GetWindowsDirectoryA(font,MAX_PATH); strcat(font,"/Fonts/malgun.ttf");
  assert(ImFontAtlas_AddFontFromFileTTF(io->Fonts,font,20,NULL,NULL));
  // Match the production spacing relevant to the table.
  ImGuiStyle* style=igGetStyle();
  style->FramePadding=(ImVec2){8,8}; style->ItemSpacing=(ImVec2){4,4};
  style->WindowPadding=(ImVec2){4,4}; style->CellPadding.x=2;
  tenv env={0}; env.ctx=calloc(1,sizeof(*env.ctx)); env.usr=calloc(1,sizeof(*env.usr));
  env.ctx->size[0]=2560; env.ctx->size[1]=1600; env.usr->gdata.conn=CONNECTED;
  strcpy(env.usr->usrs.ipv4,visible_server);
  int sizes[]={-1,0,8,32,128};
  puts("{\"ui_cpu\":[");
  for(int c=0;c<5;c++) {
    native_prefs.team_status=sizes[c]>=0;
    reply.count=sizes[c]>0?sizes[c]:0;
    for(int i=0;i<reply.count;i++) {
      ntl_member* m=reply.rows+i; snprintf(m->nick,sizeof(m->nick),"TEST%04d팀원 %03d",i,i);
      snprintf(m->server,sizeof(m->server),"synthetic-%03d.example:444",i%4);
      m->fps=60; m->ping=45; m->score=12345+i; m->sos=i%13==0;
    }
    team_status_build(&roster,&reply,"TEST0000팀원 000",visible_server);
    for(int i=0;i<120;i++) frame(&env);
    double times[2000],sum=0;
    for(int i=0;i<2000;i++) { double start=now_ms(); frame(&env); times[i]=now_ms()-start; sum+=times[i]; }
    qsort(times,2000,sizeof(double),compare);
    printf("%s{\"members\":%d,\"frames\":2000,\"mean_ms\":%.6f,\"p95_ms\":%.6f,\"p99_ms\":%.6f,\"max_ms\":%.6f}",c?",\n":"",sizes[c],sum/2000,times[1899],times[1979],times[1999]);
  }
  puts("],\"snapshot_cpu\":[");
  char json[65536];
  for(int c=2;c<5;c++) {
    int len=0; json[len++]='[';
    for(int i=0;i<sizes[c];i++) len+=snprintf(json+len,sizeof(json)-len,"%s{\"nick\":\"TEST%04dmember\",\"srv\":\"synthetic-%d.example:444\",\"msg\":\"short synthetic message\",\"dt\":\"FPS: 60 @ 45(70) ms\",\"score\":12345,\"sid\":3,\"valx\":12000,\"valy\":13000}",i?",":"",i,i%4);
    json[len++]=']'; json[len]=0;
    for(int i=0;i<20;i++) assert(ntl_parse(json,"TEST0000member",&reply)==0);
    double start=now_ms();
    for(int i=0;i<200;i++) { assert(ntl_parse(json,"TEST0000member",&reply)==0); sink+=reply.count; }
    double parse=(now_ms()-start)/200;
    start=now_ms();
    for(int i=0;i<2000;i++) { team_status_build(&roster,&reply,"TEST0000member",visible_server); sink+=roster.count; }
    printf("%s{\"members\":%d,\"parse_ms\":%.6f,\"roster_build_ms\":%.6f}",c>2?",\n":"",sizes[c],parse,(now_ms()-start)/2000);
  }
  puts("]}");
  igDestroyContext(NULL); free(env.ctx); free(env.usr); return 0;
}
'''


def benchmark():
    source = (ROOT / 'app/src/network/ntl.c').read_text(encoding='utf-8')
    draw = source.split('static void draw_team_status(tenv* env) {', 1)[1].split('static void draw_chat', 1)[0]
    draw = 'static void draw_team_status(tenv* env) {' + draw
    harness = OUT / 'ui_benchmark.c'
    harness.write_text(HARNESS.replace('/* PRODUCTION_UI */', draw), encoding='utf-8')
    obj = OUT / 'ui_benchmark.o'
    run([BIN / 'x86_64-w64-mingw32-clang.exe', '-std=gnu11', '-O2', '-I', ROOT, '-c', harness, '-o', obj])
    objects = [obj, BUILD/'app_src_cimgui_cimgui.cpp.o']
    imgui_sources = ['app/src/cimgui/cimgui.cpp'] + [f'app/src/cimgui/imgui/{name}.cpp' for name in ['imgui','imgui_draw','imgui_tables','imgui_widgets','imgui_demo']]
    for rel in imgui_sources:
        compiled = BUILD/(rel.replace('/', '_')+'.o')
        if not compiled.exists() or compiled.stat().st_mtime < (ROOT/rel).stat().st_mtime:
            raise RuntimeError('Build the current client first: python tools/build_windows.py')
    for rel in ['app/src/network/ntl_protocol.c', 'app/src/external/mongoose.c']:
        compiled = OUT / (rel.replace('/', '_') + '.o')
        run([BIN/'x86_64-w64-mingw32-clang.exe', '-std=gnu11', '-O2', '-c', ROOT/rel, '-o', compiled])
        objects.append(compiled)
    objects += [BUILD/f'app_src_cimgui_imgui_{name}.cpp.o' for name in ['imgui','imgui_draw','imgui_tables','imgui_widgets','imgui_demo']]
    executable = OUT / 'ui_benchmark.exe'
    run([BIN/'x86_64-w64-mingw32-clang++.exe', *objects, '-static', '-lws2_32', '-limm32', '-o', executable])
    # Consecutive rounds, no competing build/test process.
    rounds = [json.loads(run([executable])) for _ in range(3)]
    bots = {}
    for name in ['defensive_planner', 'body_loop']:
        exe = OUT / f'{name}.exe'
        run([BIN/'x86_64-w64-mingw32-clang.exe', '-std=gnu11', '-O2', '-static',
             ROOT/f'tests/{name}_test.c', ROOT/'app/src/game/defensive_planner.c',
             ROOT/'app/src/game/body_loop.c', '-o', exe])
        outputs = [run([exe]) for _ in range(7)]
        bots[name] = {'trial_mean_ms_per_decision': [float(re.search(r'([\d.]+) ms/decision', s)[1]) for s in outputs], 'last_output': outputs[-1].strip()}
    full_bot = OUT/'full_bot.exe'
    flags = ['-DGLFW_INCLUDE_NONE','-DGLFW_INCLUDE_VULKAN','-DCIMGUI_NO_EXPORT','-DSETTINGS_VERSION="1.3"']
    for path in ['glfw/include','thermite/include','thermite/src','.tools/Vulkan-Headers/include']:
        flags += ['-I', str(ROOT/path)]
    run([BIN/'x86_64-w64-mingw32-clang.exe','-std=gnu11','-O2','-static',*flags,
         ROOT/'tools/performance_bot_benchmark.c', ROOT/'app/src/game/sbot.c',
         ROOT/'app/src/game/defensive_planner.c', ROOT/'app/src/game/body_loop.c',
         ROOT/'thermite/src/util/tdarray.c','-Wl,--wrap=def_choose','-o',full_bot])
    full_bot_rounds = [json.loads(run([full_bot])) for _ in range(3)]
    return {'ui_and_snapshot_rounds': rounds, 'bot_O2_synthetic': bots, 'full_bot_rounds':full_bot_rounds,
            'production_draw_function_sha256': hashlib.sha256(draw.encode()).hexdigest()}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    result = {'measured_at_utc': datetime.now(timezone.utc).isoformat(), 'limitations': [
        'Historical CSV has no build/version identity and only one-second aggregates; no frame p95/p99 can be recovered.',
        'Timing stages overlap; submit contains commands/queue/present, pacing includes network.',
        'Headless UI includes NewFrame/Render with warm fonts, no Vulkan/GPU work or input-to-photon measurement.',
        'Synthetic roster benchmarks use short chat messages and four server groups, not production NTL responses.',
        'Planner and full sbot_go benchmarks are separate; static synthetic scenes do not establish real multiplayer survival.',
    ], 'logs': {}}
    combined = []
    for name in ['performance.csv', 'performance.previous.csv']:
        path = ROOT / 'dist' / name
        if not path.exists():
            continue
        raw = path.read_bytes()
        rows = list(csv.DictReader(raw.decode('utf-8-sig').splitlines()))
        game = [r for r in rows if int(r['frames']) > 0 and r['playing_frames'] == r['frames']]
        combined += game
        result['logs'][name] = {'sha256': hashlib.sha256(raw).hexdigest(), 'pure_gameplay': summarize(game),
                              'excluded_mixed_buckets': sum(0 < int(r['playing_frames']) < int(r['frames']) for r in rows)}
    result['combined_pure_gameplay'] = summarize(combined)
    result['benchmarks'] = benchmark()
    output = OUT / 'results.json'
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(output)
    print(json.dumps({'gameplay': {k:v for k,v in result['combined_pure_gameplay'].items() if k!='worst_buckets'}, 'benchmarks':result['benchmarks']}, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
