"""Run the original engine against local WebSocket servers and the native bridge."""
import os
import subprocess
from build_windows import ROOT, BIN, BUILD, run
from build_missile import SOURCE, build_worker, go_tool

BUILD.mkdir(parents=True, exist_ok=True)
worker=BUILD/'missile-worker/worker.exe'
build_worker(worker)
bridge=BUILD/'missile_worker_test.exe'
run([BIN/'x86_64-w64-mingw32-clang.exe','-std=gnu11','-O1','-static',
     ROOT/'tests/missile_worker_test.c',ROOT/'app/src/platform/missile_worker.c',
     ROOT/'app/src/external/mongoose.c','-lws2_32','-o',bridge])
env=os.environ.copy()
env.update(SLITHER_MISSILE_BRIDGE_TEST=str(bridge),SLITHER_MISSILE_WORKER_TEST=str(worker),GOTOOLCHAIN='local')
subprocess.run([go_tool(),'test','-count=1','-timeout=240s','-v','./application','./utilities','./cmd/native-worker'],cwd=SOURCE,env=env,check=True)
