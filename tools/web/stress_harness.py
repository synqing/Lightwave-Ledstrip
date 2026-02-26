#!/usr/bin/env python3
"""
LightwaveOS stress harness (REST + WS + serial telemetry).

Purpose:
- run repeatable load scenarios (typical + stress)
- capture request/WS error rates and latency
- parse serial heap/shedding diagnostics
- emit JSON + Markdown evidence with pass/fail gates
"""

from __future__ import annotations

import argparse
import json
import queue
import statistics
import threading
import time
import urllib.error
import urllib.request
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Any

try:
    import serial  # type: ignore
except Exception:  # pragma: no cover
    serial = None

try:
    import websocket  # type: ignore
except Exception:  # pragma: no cover
    websocket = None


REST_PATHS = [
    "/api/v1/device/status",
    "/api/v1/effects/current",
    "/api/v1/effects",
    "/api/v1/parameters",
]

WS_COMMANDS = [
    {"type": "device.getStatus", "payload": {}},
    {"type": "effects.getCurrent", "payload": {}},
    {"type": "effects.parameters.get", "payload": {"effectId": 6912}},
]


@dataclass
class Scenario:
    name: str
    duration_s: int
    rest_hz: float
    ws_hz: float
    rest_workers: int
    ws_workers: int


@dataclass
class ScenarioResult:
    name: str
    duration_s: int
    rest_calls: int
    rest_errors: int
    ws_messages: int
    ws_errors: int
    ws_connects: int
    ws_receives: int
    rest_p50_ms: float | None
    rest_p95_ms: float | None
    ws_p50_ms: float | None
    ws_p95_ms: float | None
    serial_lines: int
    shedding_lines: int
    ws_diag_lines: int
    heap_samples: int
    heap_min: int | None
    heap_max: int | None
    heap_avg: float | None
    largest_samples: int
    largest_min: int | None
    largest_max: int | None
    largest_avg: float | None
    uptime_resets: int
    fatal_lines: int


def pctl(values: list[float], pct: int) -> float | None:
    if not values:
        return None
    arr = sorted(values)
    idx = int((len(arr) - 1) * (pct / 100.0))
    return arr[idx]


def parse_internal_heap(line: str) -> int | None:
    """Extract internal heap bytes from common log token patterns."""
    for token in ("internal=", "internal_heap="):
        idx = line.find(token)
        if idx < 0:
            continue
        start = idx + len(token)
        end = start
        while end < len(line) and line[end].isdigit():
            end += 1
        if end > start:
            try:
                return int(line[start:end])
            except ValueError:
                return None
    return None


def parse_largest_heap_block(line: str) -> int | None:
    token = "largest="
    idx = line.find(token)
    if idx < 0:
        return None
    start = idx + len(token)
    end = start
    while end < len(line) and line[end].isdigit():
        end += 1
    if end <= start:
        return None
    try:
        return int(line[start:end])
    except ValueError:
        return None


def http_get_json(url: str, timeout_s: float = 3.0) -> tuple[dict[str, Any] | None, float, bool]:
    t0 = time.time()
    try:
        req = urllib.request.Request(url, method="GET", headers={"Accept": "application/json"})
        with urllib.request.urlopen(req, timeout=timeout_s) as resp:
            payload = resp.read().decode("utf-8", errors="replace")
        return json.loads(payload), (time.time() - t0) * 1000.0, True
    except Exception:
        return None, (time.time() - t0) * 1000.0, False


def make_profiles() -> dict[str, list[Scenario]]:
    return {
        "quick": [
            Scenario("rest_quick", 30, 4.0, 0.0, 1, 0),
            Scenario("ws_quick", 30, 0.0, 8.0, 0, 1),
            Scenario("mixed_quick", 60, 3.0, 6.0, 1, 1),
        ],
        "typical": [
            Scenario("rest_typical_5m", 300, 4.0, 0.0, 1, 0),
            Scenario("ws_typical_5m", 300, 0.0, 8.0, 0, 1),
            Scenario("mixed_typical_10m", 600, 3.0, 6.0, 1, 1),
        ],
        "stress": [
            Scenario("rest_stress_5m", 300, 12.0, 0.0, 2, 0),
            Scenario("ws_stress_5m", 300, 0.0, 25.0, 0, 2),
            Scenario("mixed_stress_15m", 900, 10.0, 20.0, 2, 2),
        ],
    }


def run_scenario(host: str, ws_url: str, serial_port: str | None, scenario: Scenario) -> ScenarioResult:
    stop_event = threading.Event()
    end_time = time.time() + scenario.duration_s
    metrics_q: queue.Queue[tuple[str, Any]] = queue.Queue()

    def serial_worker() -> None:
        if not serial_port or serial is None:
            return
        try:
            ser = serial.Serial(serial_port, 115200, timeout=0.2)
        except Exception:
            return
        while not stop_event.is_set():
            try:
                data = ser.read(4096)
            except Exception:
                break
            if not data:
                continue
            text = data.decode("utf-8", errors="replace")
            for line in text.splitlines():
                metrics_q.put(("serial_line", line))
        try:
            ser.close()
        except Exception:
            pass

    def rest_worker() -> None:
        if scenario.rest_hz <= 0.0:
            return
        interval = 1.0 / scenario.rest_hz
        i = 0
        while not stop_event.is_set() and time.time() < end_time:
            path = REST_PATHS[i % len(REST_PATHS)]
            i += 1
            obj, latency_ms, ok = http_get_json(f"{host}{path}")
            metrics_q.put(("rest_latency_ms", latency_ms))
            metrics_q.put(("rest_ok", ok))
            if ok and obj and path.endswith("/device/status"):
                uptime = obj.get("data", {}).get("uptime")
                if isinstance(uptime, (int, float)):
                    metrics_q.put(("uptime", int(uptime)))
            time.sleep(interval)

    def ws_worker() -> None:
        if scenario.ws_hz <= 0.0 or websocket is None:
            return
        interval = 1.0 / scenario.ws_hz
        ws = None
        cmd_idx = 0
        while not stop_event.is_set() and time.time() < end_time:
            if ws is None:
                try:
                    ws = websocket.create_connection(ws_url, timeout=2)
                    ws.settimeout(0.4)
                    metrics_q.put(("ws_connect", True))
                except Exception:
                    metrics_q.put(("ws_connect", False))
                    time.sleep(0.3)
                    continue
            cmd = WS_COMMANDS[cmd_idx % len(WS_COMMANDS)].copy()
            cmd_idx += 1
            cmd["requestId"] = f"{scenario.name}-{cmd_idx}-{int(time.time() * 1000)}"
            t0 = time.time()
            try:
                ws.send(json.dumps(cmd))
                metrics_q.put(("ws_sent", True))
                try:
                    _ = ws.recv()
                    metrics_q.put(("ws_recv", True))
                except Exception:
                    pass
                metrics_q.put(("ws_latency_ms", (time.time() - t0) * 1000.0))
            except Exception:
                metrics_q.put(("ws_sent", False))
                try:
                    ws.close()
                except Exception:
                    pass
                ws = None
            time.sleep(interval)
        if ws is not None:
            try:
                ws.close()
            except Exception:
                pass

    threads: list[threading.Thread] = [threading.Thread(target=serial_worker, daemon=True)]
    for _ in range(scenario.rest_workers):
        threads.append(threading.Thread(target=rest_worker, daemon=True))
    for _ in range(scenario.ws_workers):
        threads.append(threading.Thread(target=ws_worker, daemon=True))
    for t in threads:
        t.start()

    rest_lat: list[float] = []
    ws_lat: list[float] = []
    rest_calls = 0
    rest_errors = 0
    ws_messages = 0
    ws_errors = 0
    ws_connects = 0
    ws_receives = 0
    serial_lines = 0
    shedding_lines = 0
    ws_diag_lines = 0
    heaps: list[int] = []
    largest_blocks: list[int] = []
    uptime_resets = 0
    last_uptime: int | None = None
    fatal_lines = 0

    while time.time() < end_time:
        try:
            k, v = metrics_q.get(timeout=0.5)
        except queue.Empty:
            continue
        if k == "rest_latency_ms":
            rest_lat.append(float(v))
        elif k == "rest_ok":
            rest_calls += 1
            if not v:
                rest_errors += 1
        elif k == "ws_latency_ms":
            ws_lat.append(float(v))
        elif k == "ws_sent":
            ws_messages += 1
            if not v:
                ws_errors += 1
        elif k == "ws_recv":
            ws_receives += 1
        elif k == "ws_connect":
            if v:
                ws_connects += 1
            else:
                ws_errors += 1
        elif k == "serial_line":
            serial_lines += 1
            line: str = v
            heap = parse_internal_heap(line)
            if heap is not None:
                heaps.append(heap)
            largest = parse_largest_heap_block(line)
            if largest is not None:
                largest_blocks.append(largest)
            if "Low-heap shedding active" in line:
                shedding_lines += 1
            if "WS diag:" in line:
                ws_diag_lines += 1
            if (
                "Guru Meditation Error" in line
                or "abort()" in line
                or "Backtrace:" in line
                or "rst:0x" in line
                or "Rebooting..." in line
            ):
                fatal_lines += 1
        elif k == "uptime":
            up = int(v)
            if last_uptime is not None and up < last_uptime:
                uptime_resets += 1
            last_uptime = up

    stop_event.set()
    for t in threads:
        t.join(timeout=2.0)

    return ScenarioResult(
        name=scenario.name,
        duration_s=scenario.duration_s,
        rest_calls=rest_calls,
        rest_errors=rest_errors,
        ws_messages=ws_messages,
        ws_errors=ws_errors,
        ws_connects=ws_connects,
        ws_receives=ws_receives,
        rest_p50_ms=pctl(rest_lat, 50),
        rest_p95_ms=pctl(rest_lat, 95),
        ws_p50_ms=pctl(ws_lat, 50),
        ws_p95_ms=pctl(ws_lat, 95),
        serial_lines=serial_lines,
        shedding_lines=shedding_lines,
        ws_diag_lines=ws_diag_lines,
        heap_samples=len(heaps),
        heap_min=min(heaps) if heaps else None,
        heap_max=max(heaps) if heaps else None,
        heap_avg=round(statistics.mean(heaps), 1) if heaps else None,
        largest_samples=len(largest_blocks),
        largest_min=min(largest_blocks) if largest_blocks else None,
        largest_max=max(largest_blocks) if largest_blocks else None,
        largest_avg=round(statistics.mean(largest_blocks), 1) if largest_blocks else None,
        uptime_resets=uptime_resets,
        fatal_lines=fatal_lines,
    )


def verdict(results: list[ScenarioResult]) -> tuple[str, list[str]]:
    notes: list[str] = []
    hard_fail = False
    for r in results:
        if r.fatal_lines > 0:
            hard_fail = True
            notes.append(f"{r.name}: fatal serial indicators detected ({r.fatal_lines})")
        if r.uptime_resets > 0:
            hard_fail = True
            notes.append(f"{r.name}: uptime reset detected ({r.uptime_resets})")
        if r.rest_calls > 0:
            rest_err_rate = r.rest_errors / r.rest_calls
            if rest_err_rate > 0.30:
                hard_fail = True
                notes.append(f"{r.name}: REST error rate too high ({rest_err_rate:.1%})")
        if r.ws_messages > 0:
            ws_err_rate = r.ws_errors / r.ws_messages
            if ws_err_rate > 0.30:
                hard_fail = True
                notes.append(f"{r.name}: WS error rate too high ({ws_err_rate:.1%})")
        if r.heap_min is not None and r.heap_min < 12000:
            notes.append(f"{r.name}: internal heap dipped below 12KB ({r.heap_min})")
    return ("FAIL" if hard_fail else "PASS"), notes


def write_reports(out_base: Path, host: str, ws_url: str, serial_port: str | None, profile: str, results: list[ScenarioResult]) -> tuple[Path, Path]:
    overall, notes = verdict(results)
    payload = {
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        "host": host,
        "ws_url": ws_url,
        "serial_port": serial_port,
        "profile": profile,
        "overall": overall,
        "results": [asdict(r) for r in results],
        "notes": notes,
    }
    json_path = out_base.with_suffix(".json")
    md_path = out_base.with_suffix(".md")
    json_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")

    lines = [
        "# LightwaveOS Stress Harness Report",
        "",
        f"- Timestamp: {payload['timestamp']}",
        f"- Host: `{host}`",
        f"- WS URL: `{ws_url}`",
        f"- Serial: `{serial_port}`",
        f"- Profile: `{profile}`",
        f"- Overall: `{overall}`",
        "",
        "| Scenario | Dur(s) | REST Calls | REST Err | WS Msg | WS Err | Heap Min | Heap Avg | Largest Min | Largest Avg | Shedding | Uptime Resets | Fatal Lines |",
        "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for r in results:
        lines.append(
            f"| {r.name} | {r.duration_s} | {r.rest_calls} | {r.rest_errors} | "
            f"{r.ws_messages} | {r.ws_errors} | {r.heap_min} | {r.heap_avg} | "
            f"{r.largest_min} | {r.largest_avg} | "
            f"{r.shedding_lines} | {r.uptime_resets} | {r.fatal_lines} |"
        )
    lines.append("")
    lines.append("## Notes")
    if notes:
        lines.extend([f"- {n}" for n in notes])
    else:
        lines.append("- No hard-fail gate triggered.")
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return md_path, json_path


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(description="LightwaveOS stress harness")
    ap.add_argument("--host", default="http://192.168.1.101", help="Base host URL")
    ap.add_argument("--ws-url", default="ws://192.168.1.101/ws", help="WebSocket URL")
    ap.add_argument("--serial-port", default="/dev/cu.usbmodem212401", help="Serial port for telemetry")
    ap.add_argument("--profile", choices=["quick", "typical", "stress"], default="typical")
    ap.add_argument("--out-dir", default="docs/planning/perf_runs", help="Output directory")
    ap.add_argument("--serial-only", action="store_true", help="Run serial burn-in only (no REST/WS)")
    ap.add_argument("--serial-duration", type=int, default=600, help="Serial-only duration in seconds")
    return ap.parse_args()


def main() -> int:
    args = parse_args()
    if websocket is None:
        print("ERROR: websocket-client is required. Install in a venv: pip install websocket-client pyserial")
        return 2
    if serial is None:
        print("WARN: pyserial not available; serial telemetry will be disabled")
        args.serial_port = None

    if args.serial_only:
        if not args.serial_port:
            print("ERROR: serial-only mode requires --serial-port")
            return 2
        scenarios = [Scenario(name=f"serial_only_{args.serial_duration}s", duration_s=args.serial_duration, rest_hz=0.0, ws_hz=0.0, rest_workers=0, ws_workers=0)]
        print(f"[preflight] serial-only mode on {args.serial_port}")
    else:
        # Preflight connectivity gate to avoid meaningless stress results.
        pre_obj, pre_lat_ms, pre_ok = http_get_json(f"{args.host}/api/v1/device/status", timeout_s=4.0)
        if not pre_ok:
            print(f"ERROR: preflight failed: cannot reach {args.host}/api/v1/device/status")
            return 3
        print(f"[preflight] REST ok ({pre_lat_ms:.1f} ms)")
        if websocket is not None:
            try:
                ws = websocket.create_connection(args.ws_url, timeout=3)
                ws.close()
                print("[preflight] WS ok")
            except Exception:
                print(f"ERROR: preflight failed: cannot open WebSocket {args.ws_url}")
                return 3
        profiles = make_profiles()
        scenarios = profiles[args.profile]
    print(f"[stress-harness] profile={args.profile} scenarios={len(scenarios)}")
    results: list[ScenarioResult] = []
    for s in scenarios:
        print(f"[run] {s.name} ({s.duration_s}s)")
        results.append(run_scenario(args.host, args.ws_url, args.serial_port, s))
        r = results[-1]
        print(f"[done] {r.name}: rest={r.rest_calls}/{r.rest_errors} ws={r.ws_messages}/{r.ws_errors} heap_min={r.heap_min}")

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    stamp = time.strftime("%Y-%m-%d_%H%M%S")
    md_path, json_path = write_reports(out_dir / f"stress_harness_{args.profile}_{stamp}", args.host, args.ws_url, args.serial_port, args.profile, results)
    print(f"[report] {md_path}")
    print(f"[report] {json_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
