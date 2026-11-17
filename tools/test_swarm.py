"""Run the original engine against local WebSocket servers and the native bridge."""
import os
import subprocess
import tempfile
from build_windows import ROOT, BIN, BUILD, run
from build_swarm import SOURCE, build_worker, go_tool

BUILD.mkdir(parents=True, exist_ok=True)
worker=BUILD/'missile-worker/worker.exe'
build_worker(worker)
bridge=BUILD/'swarm_worker_test.exe'
status_test=BUILD/'swarm_status_test.exe'
run([BIN/'x86_64-w64-mingw32-clang.exe','-std=gnu11','-O1','-static',
     ROOT/'tests/swarm_status_test.c',ROOT/'app/src/external/mongoose.c','-lws2_32','-o',status_test])
with tempfile.TemporaryDirectory(prefix='swarm-status-',dir=BUILD) as sandbox:
    subprocess.run([str(status_test)],cwd=sandbox,check=True)
run([BIN/'x86_64-w64-mingw32-clang.exe','-std=gnu11','-O1','-static',
     ROOT/'tests/swarm_worker_test.c',ROOT/'app/src/platform/swarm_worker.c',
     ROOT/'app/src/external/mongoose.c','-lws2_32','-o',bridge])
env=os.environ.copy()
env.update(SLITHER_MISSILE_BRIDGE_TEST=str(bridge),SLITHER_MISSILE_WORKER_TEST=str(worker),GOTOOLCHAIN='local')
subprocess.run([go_tool(),'test','-count=1','-timeout=240s','-v','./application','./utilities','./cmd/native-worker'],cwd=SOURCE,env=env,check=True)
