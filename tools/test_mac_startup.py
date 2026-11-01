"""Start the real Mac app with isolated windowed/fullscreen preferences."""
import csv
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time

root = Path(__file__).resolve().parents[1]
bundle = root / 'dist/SlitherNative.app/Contents'
for fullscreen in (False, True):
    with tempfile.TemporaryDirectory(prefix='slither-startup-') as temporary:
        data = Path(temporary)
        (data / 'app').symlink_to(bundle / 'Resources/app', target_is_directory=True)
        (data / 'native-settings.json').write_text(json.dumps({
            'version': 2, 'frame_cap': 1, 'render_scale': 1,
            'team_panel': False, 'fullscreen': fullscreen,
        }))
        env = dict(os.environ, SLITHER_DATA_DIR=str(data))
        with (data / 'startup.log').open('w+') as log:
            process = subprocess.Popen([str(bundle / 'MacOS/SlitherNative')],
                                       cwd=data, env=env, stdout=log, stderr=log)
            try:
                time.sleep(12)
                if process.poll() is not None:
                    log.seek(0)
                    try:
                        debug = subprocess.run(['lldb', '--batch', '-k', 'thread backtrace all',
                                                '-o', 'breakpoint set -n __chk_fail_overflow',
                                                '-o', 'run', '-o', 'thread backtrace all',
                                                '--', str(bundle / 'MacOS/SlitherNative')],
                                               cwd=data, env=env, capture_output=True, text=True, timeout=45)
                        print(debug.stdout[-16000:] + debug.stderr[-2000:], flush=True)
                    except subprocess.TimeoutExpired:
                        print('Debugger timed out', flush=True)
                    raise RuntimeError(f'fullscreen={fullscreen}: exit {process.returncode}\n{log.read()}')
                perf = data / 'performance.csv'
                rows = list(csv.DictReader(perf.read_text().splitlines())) if perf.exists() else []
                frames = sum(int(row['frames']) for row in rows)
                if frames < 2:
                    log.seek(0)
                    raise RuntimeError(f'No rendering telemetry: {log.read()}')
                saved = json.loads((data / 'native-settings.json').read_text())
                if saved['fullscreen'] != fullscreen:
                    raise RuntimeError(f'Fullscreen state was not applied: {saved["fullscreen"]}')
                print(f'PASS: fullscreen={fullscreen}, alive after 12 seconds, {frames} frames recorded', flush=True)
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=8)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
