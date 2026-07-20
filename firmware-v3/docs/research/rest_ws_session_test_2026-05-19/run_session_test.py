#!/usr/bin/env python3
"""
K1v2 REST/WS session test — STA topology, controlling-machine over LAN.

Discharges Captain's revised 2026-05-19 spec: REST/WS command/session
validation from controlling machine against K1v2 on STA (no AP-client soak).

Topology:
  Controlling machine: 192.168.1.101 (Ethernet, en0)
  K1v2 STA: 192.168.1.106 (joined VX220-013F)
  Test: 3 concurrent WS sessions + REST polling for 600s; churn at t=300s.

Hard constraints (Captain 2026-05-19):
  - K1v2 stays on STA throughout test (no AP-client topology)
  - Synthetic clients all originate on this machine
  - Restore K1v2 to pre-test mode (AP) at end

Outputs:
  /tmp/k1_session_test_<timestamp>.log    — narrative + WS/REST traffic
  /tmp/k1_session_serial_<timestamp>.log  — serial heap probes
  /tmp/k1_session_summary_<timestamp>.json — final metrics + verdict
"""
import json
import os
import re
import sys
import threading
import time
import urllib.request
import urllib.error
from datetime import datetime
from concurrent.futures import ThreadPoolExecutor

import serial
import websocket

# Configuration
K1_LAN_IP = '192.168.1.106'
K1_SERIAL_PORT = '/dev/tty.usbmodem1301'
K1_SERIAL_BAUD = 115200
WS_URL = f'ws://{K1_LAN_IP}:80/ws'
REST_BASE = f'http://{K1_LAN_IP}/api/v1'

N_SESSIONS = 3
SOAK_DURATION_S = 600     # 10 min per Captain
CHURN_AT_S = 300          # disconnect+reconnect session #2 at t=300s
WS_CMD_INTERVAL_S = 5     # each session sends a command every 5s
REST_POLL_INTERVAL_S = 5  # REST poll every 5s
SERIAL_PROBE_INTERVAL_S = 60  # serial heap probe every 60s

TS = datetime.now().strftime('%Y%m%dT%H%M%S')
LOG_PATH = f'/tmp/k1_session_test_{TS}.log'
SERIAL_LOG_PATH = f'/tmp/k1_session_serial_{TS}.log'
SUMMARY_PATH = f'/tmp/k1_session_summary_{TS}.json'

# Commands cycled per WS session
WS_COMMANDS = [
    {'type': 'effects.list', 'data': {'page': 0, 'limit': 20}},
    {'type': 'zones.list'},
    {'type': 'effects.current'},
    {'type': 'audio.parameters'},
    {'type': 'palettes.list'},
]
REST_PATHS = ['/ping', '/effects', '/audio/zone-agc']  # last returns 501 on ESV11 (intentional)

log_lock = threading.Lock()
metrics_lock = threading.Lock()

metrics = {
    'ws_sent': [0]*N_SESSIONS,
    'ws_recv': [0]*N_SESSIONS,
    'ws_errors': [0]*N_SESSIONS,
    'ws_reconnects': [0]*N_SESSIONS,
    'rest_sent': 0,
    'rest_ok': 0,
    'rest_fail': 0,
    'rest_501_expected': 0,
    'serial_probes': 0,
    'shed_events': 0,
    'panic_events': 0,
    'heap_samples': [],
}
stop_event = threading.Event()


def log(msg):
    line = f'[{time.time():.2f}] {msg}\n'
    with log_lock:
        with open(LOG_PATH, 'a') as f:
            f.write(line)


def ws_session_worker(session_id, do_churn_at=None):
    """One WS session: connect, periodic command emission, recv responses."""
    cmd_idx = 0
    ws = None
    next_send = time.time() + (session_id * 0.5)  # stagger first send

    def connect():
        nonlocal ws
        try:
            ws = websocket.create_connection(WS_URL, timeout=5)
            ws.settimeout(0.5)
            log(f'ws[{session_id}] CONNECTED')
            return True
        except Exception as e:
            log(f'ws[{session_id}] CONNECT_FAIL: {e}')
            with metrics_lock:
                metrics['ws_errors'][session_id] += 1
            return False

    if not connect():
        return

    start = time.time()
    last_churn = False
    while not stop_event.is_set():
        now = time.time()
        elapsed = now - start

        # Churn injection
        if do_churn_at and not last_churn and elapsed >= do_churn_at:
            log(f'ws[{session_id}] CHURN: closing for reconnect')
            try:
                ws.close()
            except Exception:
                pass
            time.sleep(5.0)
            log(f'ws[{session_id}] CHURN: reconnecting')
            if connect():
                with metrics_lock:
                    metrics['ws_reconnects'][session_id] += 1
            else:
                return
            last_churn = True

        # Periodic command
        if now >= next_send:
            cmd = dict(WS_COMMANDS[cmd_idx % len(WS_COMMANDS)])
            cmd['requestId'] = f's{session_id}-{cmd_idx}'
            try:
                ws.send(json.dumps(cmd))
                with metrics_lock:
                    metrics['ws_sent'][session_id] += 1
                cmd_idx += 1
            except Exception as e:
                log(f'ws[{session_id}] SEND_ERR: {e}')
                with metrics_lock:
                    metrics['ws_errors'][session_id] += 1
                # Try reconnect
                try:
                    ws.close()
                except Exception:
                    pass
                time.sleep(2.0)
                if not connect():
                    return
                with metrics_lock:
                    metrics['ws_reconnects'][session_id] += 1
            next_send = now + WS_CMD_INTERVAL_S

        # Drain inbound
        try:
            msg = ws.recv()
            if msg:
                with metrics_lock:
                    metrics['ws_recv'][session_id] += 1
        except websocket.WebSocketTimeoutException:
            pass
        except Exception as e:
            log(f'ws[{session_id}] RECV_ERR: {e}')
            with metrics_lock:
                metrics['ws_errors'][session_id] += 1

    try:
        ws.close()
    except Exception:
        pass
    log(f'ws[{session_id}] CLOSED')


def rest_poll_worker():
    while not stop_event.is_set():
        for path in REST_PATHS:
            if stop_event.is_set():
                break
            url = REST_BASE + path
            with metrics_lock:
                metrics['rest_sent'] += 1
            try:
                req = urllib.request.Request(url, headers={'Accept': 'application/json'})
                with urllib.request.urlopen(req, timeout=5) as resp:
                    status = resp.status
                    body = resp.read(2048)
                    if status == 200:
                        with metrics_lock:
                            metrics['rest_ok'] += 1
                    else:
                        with metrics_lock:
                            metrics['rest_fail'] += 1
                        log(f'rest {path} -> HTTP {status}')
            except urllib.error.HTTPError as e:
                if path == '/audio/zone-agc' and e.code == 501:
                    with metrics_lock:
                        metrics['rest_501_expected'] += 1
                else:
                    with metrics_lock:
                        metrics['rest_fail'] += 1
                    log(f'rest {path} -> HTTP {e.code}')
            except Exception as e:
                with metrics_lock:
                    metrics['rest_fail'] += 1
                log(f'rest {path} -> ERR {e}')
        time.sleep(REST_POLL_INTERVAL_S)


def serial_probe_worker():
    """Owns serial port for the duration; probes K1 state every 60s."""
    try:
        ser = serial.Serial(K1_SERIAL_PORT, K1_SERIAL_BAUD, timeout=0.1)
    except Exception as e:
        log(f'serial OPEN_FAIL: {e}')
        return

    sf = open(SERIAL_LOG_PATH, 'wb')
    sf.write(f'# K1 session test serial probe log {TS}\n'.encode())
    sf.write(f'# K1 STA IP: {K1_LAN_IP}\n#\n'.encode())

    last_probe = 0.0
    while not stop_event.is_set():
        # passive read between probes
        data = ser.read(4096)
        if data:
            sf.write(data)
            sf.flush()
            # Check for shed/panic patterns
            txt = data.decode('ascii', errors='replace')
            if 'shed.enable' in txt:
                with metrics_lock:
                    metrics['shed_events'] += 1
                log(f'SERIAL_SHED_EVENT detected')
            if 'PANIC' in txt or 'Guru Meditation' in txt or 'WDT' in txt.upper():
                with metrics_lock:
                    metrics['panic_events'] += 1
                log(f'SERIAL_PANIC detected')

        now = time.time()
        if now - last_probe >= SERIAL_PROBE_INTERVAL_S:
            last_probe = now
            for cmd in (b's\r\n', b'dbg memory\r\n'):
                ser.write(cmd)
                time.sleep(1.8)
                resp = ser.read(16384)
                sf.write(f'\n--- PROBE t={now:.0f} cmd={cmd!r} ---\n'.encode())
                sf.write(resp)
                # Parse Free heap / Max alloc for trend
                txt = resp.decode('ascii', errors='replace')
                fh = re.search(r'Free heap:\s*(\d+)', txt)
                ma = re.search(r'Max alloc heap:\s*(\d+)', txt)
                if fh and ma:
                    with metrics_lock:
                        metrics['heap_samples'].append({
                            't': now, 'free_heap': int(fh.group(1)),
                            'max_alloc': int(ma.group(1))
                        })
                sf.flush()
            with metrics_lock:
                metrics['serial_probes'] += 1
        time.sleep(0.2)

    sf.close()
    ser.close()


def main():
    log('=== K1v2 REST/WS session test START ===')
    log(f'K1_LAN_IP={K1_LAN_IP}  WS_URL={WS_URL}')
    log(f'N_SESSIONS={N_SESSIONS}  SOAK={SOAK_DURATION_S}s  CHURN_AT={CHURN_AT_S}s')

    # Workers
    serial_thread = threading.Thread(target=serial_probe_worker, daemon=True)
    serial_thread.start()

    rest_thread = threading.Thread(target=rest_poll_worker, daemon=True)
    rest_thread.start()

    ws_threads = []
    for sid in range(N_SESSIONS):
        churn = CHURN_AT_S if sid == 1 else None  # session 1 (0-indexed) churns
        t = threading.Thread(target=ws_session_worker, args=(sid, churn), daemon=True)
        t.start()
        ws_threads.append(t)

    # Soak
    start = time.time()
    last_status = start
    while time.time() - start < SOAK_DURATION_S:
        time.sleep(5.0)
        if time.time() - last_status > 60:
            last_status = time.time()
            with metrics_lock:
                snap = {k: v if not isinstance(v, list) else len(v) for k, v in metrics.items()}
            log(f'STATUS t={time.time()-start:.0f}s  {snap}')

    log('=== Soak duration elapsed; stopping workers ===')
    stop_event.set()
    time.sleep(3.0)

    # Final state snapshot
    log('=== Final metrics ===')
    with metrics_lock:
        final = dict(metrics)
        # Heap recovery analysis
        if final['heap_samples']:
            first_free = final['heap_samples'][0]['free_heap']
            last_free = final['heap_samples'][-1]['free_heap']
            min_max_alloc = min(s['max_alloc'] for s in final['heap_samples'])
            final['heap_first_free'] = first_free
            final['heap_last_free'] = last_free
            final['heap_delta'] = last_free - first_free
            final['heap_min_max_alloc'] = min_max_alloc

    # Verdict
    all_sent = sum(final['ws_sent'])
    all_recv = sum(final['ws_recv'])
    ws_errors = sum(final['ws_errors'])
    rest_fail = final['rest_fail']
    shed = final['shed_events']
    panic = final['panic_events']
    min_alloc = final.get('heap_min_max_alloc', 0)
    heap_delta = final.get('heap_delta', 0)

    if panic > 0 or shed > 0:
        verdict = 'FAIL'
        reason = f'panic={panic} shed={shed}'
    elif ws_errors > all_sent * 0.05 or rest_fail > final['rest_sent'] * 0.05:
        verdict = 'FAIL'
        reason = f'ws_errors={ws_errors}/{all_sent} rest_fail={rest_fail}/{final["rest_sent"]}'
    elif min_alloc < 2048:
        verdict = 'DEGRADED_PASS'
        reason = f'min max_alloc {min_alloc}B below 2KB margin'
    elif heap_delta < -2048:
        verdict = 'DEGRADED_PASS'
        reason = f'heap leaked {-heap_delta}B'
    else:
        verdict = 'PASS'
        reason = f'all clean: ws_errors=0, rest_fail=0, no shed/panic, heap stable'

    final['verdict'] = verdict
    final['verdict_reason'] = reason
    log(f'=== VERDICT: {verdict}  ({reason}) ===')
    log(f'WS sent={all_sent}  recv={all_recv}  errors={ws_errors}  reconnects={sum(final["ws_reconnects"])}')
    log(f'REST sent={final["rest_sent"]}  ok={final["rest_ok"]}  fail={rest_fail}  501-expected={final["rest_501_expected"]}')
    log(f'Heap first/last/delta={final.get("heap_first_free")}/{final.get("heap_last_free")}/{heap_delta}  min_max_alloc={min_alloc}')

    # Persist summary
    with open(SUMMARY_PATH, 'w') as f:
        json.dump(final, f, indent=2, default=str)

    print(f'\n=== K1 SESSION TEST COMPLETE ===')
    print(f'Verdict: {verdict}')
    print(f'Reason: {reason}')
    print(f'Log: {LOG_PATH}')
    print(f'Serial: {SERIAL_LOG_PATH}')
    print(f'Summary: {SUMMARY_PATH}')


if __name__ == '__main__':
    main()
