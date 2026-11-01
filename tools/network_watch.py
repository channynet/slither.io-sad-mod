"""Log, once per second, where network delay appears while the game is played.

Read-only: it only runs ping / netstat / netsh / route and writes dist/network-watch.csv.
Stop with Ctrl+C or by closing the window.

Columns
  gateway_ms   ping to the home router        -> high here = Wi-Fi / local link
  domestic_ms  ping to a Korean DNS server    -> high here but gateway fine = ISP line
  game_ms      ping to the game server host   -> high only here = route to the game server
  rx_kbps/tx_kbps  all traffic of this PC     -> high during a spike = something is using the line
  signal/channel/bssid/rates  Wi-Fi link state -> a change during a spike = scan or roam
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import datetime
import re
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'dist' / 'network-watch.csv'
DOMESTIC = '168.126.63.1'
NO_WINDOW = getattr(subprocess, 'CREATE_NO_WINDOW', 0)


def run(args, timeout=4):
    try:
        raw = subprocess.run(args, capture_output=True, timeout=timeout, creationflags=NO_WINDOW).stdout
    except Exception:
        return ''
    for encoding in ('utf-8', 'mbcs', 'cp949'):
        try:
            return raw.decode(encoding)
        except Exception:
            continue
    return raw.decode('ascii', 'replace')


def parse_gateway(text):
    match = re.search(r'^\s*0\.0\.0\.0\s+0\.0\.0\.0\s+(\d+\.\d+\.\d+\.\d+)', text, re.M)
    return match.group(1) if match else ''


def parse_ping(text):
    # "time=12ms", "시간=12ms", "time<1ms"; a reply from another host (unreachable) has no time.
    match = re.search(r'[=<]\s*(\d+)\s*ms', text)
    return match.group(1) if match else ''


def parse_totals(text):
    # First row of "netstat -e" with exactly two counters is bytes received / sent.
    match = re.search(r'^\s*\S+\s+(\d+)\s+(\d+)\s*$', text, re.M)
    return (int(match.group(1)), int(match.group(2))) if match else None


def parse_wifi(text):
    info = {'signal': '', 'channel': '', 'bssid': '', 'rx_rate': '', 'tx_rate': ''}
    rates = []
    for line in text.splitlines():
        if ':' not in line:
            continue
        key, value = line.split(':', 1)
        key, value = key.strip(), value.strip()
        low = key.lower()
        if 'bssid' in low:
            info['bssid'] = value
        elif value.endswith('%'):
            info['signal'] = value.rstrip('%').strip()
        elif ('channel' in low or '채널' in key) and value.isdigit():
            info['channel'] = value
        elif 'mbps' in low:
            rates.append(value)
    if len(rates) >= 2:
        info['rx_rate'], info['tx_rate'] = rates[0], rates[1]
    return info


def game_host():
    try:
        first = (ROOT / 'dist' / 'server-history.txt').read_text().split()[0]
        return first.split(':')[0]
    except Exception:
        return ''


def ping(host):
    return parse_ping(run(['ping', '-n', '1', '-w', '900', host])) if host else ''


def main():
    gateway = parse_gateway(run(['route', 'print', '-4', '0.0.0.0']))
    game = game_host()
    print(f'gateway={gateway or "?"} domestic={DOMESTIC} game={game or "?"}')
    print(f'writing {OUT}  (Ctrl+C to stop)')
    OUT.parent.mkdir(exist_ok=True)
    new = not OUT.exists()
    wifi = parse_wifi('')
    last_totals = None
    last_time = None
    with OUT.open('a', encoding='utf-8', newline='') as out, ThreadPoolExecutor(max_workers=4) as pool:
        if new:
            out.write('time,gateway_ms,domestic_ms,game_ms,rx_kbps,tx_kbps,signal,channel,bssid,rx_rate,tx_rate\n')
        tick = 0
        while True:
            started = time.time()
            jobs = [pool.submit(ping, host) for host in (gateway, DOMESTIC, game)]
            totals = parse_totals(run(['netstat', '-e']))
            if tick % 3 == 0:
                wifi = parse_wifi(run(['netsh', 'wlan', 'show', 'interfaces']))
            rx = tx = ''
            if totals and last_totals and started > last_time:
                seconds = started - last_time
                down, up = totals[0] - last_totals[0], totals[1] - last_totals[1]
                if down >= 0 and up >= 0:  # 32-bit counters wrap; skip that sample
                    rx, tx = f'{down * 8 / 1000 / seconds:.0f}', f'{up * 8 / 1000 / seconds:.0f}'
            last_totals, last_time = totals, started
            values = [job.result() for job in jobs]
            stamp = datetime.datetime.fromtimestamp(started).strftime('%Y-%m-%d %H:%M:%S')
            row = [stamp, *values, rx, tx, wifi['signal'], wifi['channel'], wifi['bssid'], wifi['rx_rate'], wifi['tx_rate']]
            out.write(','.join(row) + '\n')
            out.flush()
            if tick % 10 == 0:
                print(' '.join(f'{name}={value or "-"}' for name, value in
                               zip(('time', 'gw', 'kr', 'game', 'rx', 'tx', 'sig'), [stamp[11:], *values, rx, tx, wifi['signal']])))
            tick += 1
            time.sleep(max(0.0, 1.0 - (time.time() - started)))


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print('stopped')
        sys.exit(0)
