#!/usr/bin/env python3
"""
Tab5 pipeline regression harness (hardware-in-the-loop).

Validates control-plane routing across UI tabs without depending on LVGL rendering.

Coverage:
1) /api/v1/ui/state and /api/v1/ui/screen endpoints
2) Global tab: ENC-B must not emit legacy Zone labels or save p8..p15
3) Zone Composer tab: encoder deltas must route to ZoneComposer handlers
4) Connectivity tab: ENC-B must still avoid legacy Zone labels / p8..p15 saves
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

import serial


ZONE_LABEL_RE = re.compile(r"\[B:[0-7]\]\s+Z[0-3]\s")
UNIT_B_NVS_RE = re.compile(r"\[NVS\]\s+Saved p(?:8|9|1[0-5])=")
ZONE_ACTIVITY_RE = re.compile(r"\[ZoneComposer\]\s+Zone\s+[0-3]\s+(Effect|Palette|Speed|Brightness)\s")
GLOBAL_PARAM_RE = re.compile(r"\[A:[0-7]\]\s")


def _http_json(url: str, method: str = "GET", payload: Optional[dict] = None, timeout: float = 3.0) -> dict:
    data = None
    headers = {"Accept": "application/json"}
    if payload is not None:
        data = json.dumps(payload).encode("utf-8")
        headers["Content-Type"] = "application/json"

    req = Request(url=url, method=method, data=data, headers=headers)
    with urlopen(req, timeout=timeout) as resp:
        body = resp.read().decode("utf-8", errors="replace")
        return json.loads(body) if body else {}


@dataclass
class HarnessConfig:
    serial_port: str
    baud: int
    device_ip: str
    http_port: int
    settle_seconds: float
    apply_timeout_seconds: float
    screen_timeout_seconds: float
    output_log: Optional[Path]


class SerialCapture:
    def __init__(self, port: str, baud: int) -> None:
        self._port = port
        self._baud = baud
        self._thread: Optional[threading.Thread] = None
        self._running = False
        self._ser: Optional[serial.Serial] = None
        self._lines: List[str] = []
        self._lock = threading.Lock()
        self.error: Optional[Exception] = None

    def start(self) -> None:
        self._running = True
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()
        time.sleep(0.3)

    def stop(self) -> None:
        self._running = False
        if self._thread:
            self._thread.join(timeout=2.0)
        if self._ser:
            try:
                self._ser.close()
            except Exception:
                pass

    def _run(self) -> None:
        try:
            self._ser = serial.Serial(self._port, self._baud, timeout=0.2, write_timeout=1)
            self._ser.reset_input_buffer()
            while self._running:
                if self._ser.in_waiting:
                    line = self._ser.readline().decode("utf-8", errors="replace").strip()
                    if line:
                        with self._lock:
                            self._lines.append(line)
                else:
                    time.sleep(0.02)
        except Exception as exc:
            self.error = exc

    def line_count(self) -> int:
        with self._lock:
            return len(self._lines)

    def lines_since(self, start_idx: int) -> List[str]:
        with self._lock:
            return list(self._lines[start_idx:])

    def all_lines(self) -> List[str]:
        with self._lock:
            return list(self._lines)


@dataclass
class PhaseResult:
    name: str
    passed: bool
    details: List[str]
    line_count: int


class Tab5Harness:
    def __init__(self, cfg: HarnessConfig, cap: SerialCapture) -> None:
        self.cfg = cfg
        self.cap = cap
        self.base = f"http://{cfg.device_ip}:{cfg.http_port}"
        self.encoder_status_url = f"{self.base}/api/v1/encoder/status"
        self.encoder_batch_url = f"{self.base}/api/v1/encoder/batch"
        self.ui_state_url = f"{self.base}/api/v1/ui/state"
        self.ui_screen_url = f"{self.base}/api/v1/ui/screen"

    def ui_state(self) -> Dict:
        return _http_json(self.ui_state_url, method="GET")

    def set_screen(self, screen: str) -> None:
        resp = _http_json(self.ui_screen_url, method="POST", payload={"screen": screen})
        if not resp.get("ok", False):
            raise RuntimeError(f"screen switch failed: {resp}")

    def wait_screen(self, expected: str) -> bool:
        deadline = time.time() + self.cfg.screen_timeout_seconds
        while time.time() < deadline:
            st = self.ui_state()
            if st.get("uiReady") and st.get("screen") == expected:
                return True
            time.sleep(0.1)
        return False

    def enqueue_batch_and_wait(self, ops: List[dict], persist: bool = True) -> int:
        batch = _http_json(self.encoder_batch_url, method="POST", payload={"persist": persist, "ops": ops})
        if not batch.get("ok", False):
            raise RuntimeError(f"batch enqueue failed: {batch}")
        batch_id = int(batch.get("batch_id", 0))

        deadline = time.time() + self.cfg.apply_timeout_seconds
        while time.time() < deadline:
            st = _http_json(self.encoder_status_url, method="GET")
            if int(st.get("last_applied_id", 0)) >= batch_id:
                return batch_id
            time.sleep(0.08)
        raise TimeoutError(f"batch {batch_id} not drained before timeout")

    def run_phase(self, name: str, screen: str, ops: List[dict], checks: dict) -> PhaseResult:
        details: List[str] = []

        self.set_screen(screen)
        if not self.wait_screen(screen):
            return PhaseResult(name=name, passed=False,
                               details=[f"screen did not settle to '{screen}'"], line_count=0)

        start_idx = self.cap.line_count()
        _ = self.enqueue_batch_and_wait(ops, persist=True)
        time.sleep(1.2)
        lines = self.cap.lines_since(start_idx)

        if checks.get("forbid_zone_labels", False):
            hits = [ln for ln in lines if ZONE_LABEL_RE.search(ln)]
            if hits:
                details.append(f"zone-label hits: {len(hits)}")
                details.extend(hits[:5])

        if checks.get("forbid_unit_b_nvs", False):
            hits = [ln for ln in lines if UNIT_B_NVS_RE.search(ln)]
            if hits:
                details.append(f"unit-b nvs hits: {len(hits)}")
                details.extend(hits[:5])

        if checks.get("require_zone_activity", False):
            hits = [ln for ln in lines if ZONE_ACTIVITY_RE.search(ln)]
            if not hits:
                details.append("missing ZoneComposer activity logs")

        if checks.get("forbid_global_param_logs", False):
            hits = [ln for ln in lines if GLOBAL_PARAM_RE.search(ln)]
            if hits:
                details.append(f"global-param log hits: {len(hits)}")
                details.extend(hits[:5])

        return PhaseResult(name=name, passed=(len(details) == 0), details=details, line_count=len(lines))


def run_harness(cfg: HarnessConfig) -> int:
    cap = SerialCapture(cfg.serial_port, cfg.baud)
    cap.start()
    if cap.error:
        print(f"[FAIL] Serial open failed: {cap.error}", file=sys.stderr)
        return 2

    try:
        print(f"[INFO] Settling for {cfg.settle_seconds:.1f}s...", file=sys.stderr)
        time.sleep(cfg.settle_seconds)

        h = Tab5Harness(cfg, cap)
        st = h.ui_state()
        if not st.get("ok", False):
            print(f"[FAIL] UI state endpoint unavailable: {st}", file=sys.stderr)
            return 2
        if not st.get("uiReady", False):
            print(f"[FAIL] UI not ready: {st}", file=sys.stderr)
            return 2

        phases: List[PhaseResult] = []

        phases.append(h.run_phase(
            name="global_enc_b_isolation",
            screen="global",
            ops=[
                {"t": "delta", "i": 8, "v": 1},
                {"t": "delta", "i": 9, "v": 1},
                {"t": "delta", "i": 14, "v": 1},
                {"t": "delta", "i": 15, "v": 1},
            ],
            checks={
                "forbid_zone_labels": True,
                "forbid_unit_b_nvs": True,
            },
        ))

        phases.append(h.run_phase(
            name="zone_tab_routes_to_zone_composer",
            screen="zone_composer",
            ops=[
                {"t": "delta", "i": 0, "v": 1},
                {"t": "delta", "i": 1, "v": 1},
                {"t": "delta", "i": 2, "v": 1},
                {"t": "delta", "i": 3, "v": 1},
            ],
            checks={
                "require_zone_activity": True,
                "forbid_global_param_logs": True,
            },
        ))

        phases.append(h.run_phase(
            name="connectivity_enc_b_isolation",
            screen="connectivity",
            ops=[
                {"t": "delta", "i": 8, "v": 1},
                {"t": "delta", "i": 9, "v": 1},
            ],
            checks={
                "forbid_zone_labels": True,
                "forbid_unit_b_nvs": True,
            },
        ))

        print("\n=== TAB5 PIPELINE HARNESS REPORT ===")
        all_passed = True
        for p in phases:
            state = "PASS" if p.passed else "FAIL"
            print(f"{state} {p.name} (serial_lines={p.line_count})")
            if not p.passed:
                all_passed = False
                for d in p.details:
                    print(f"  {d}")

        if cfg.output_log:
            cfg.output_log.parent.mkdir(parents=True, exist_ok=True)
            cfg.output_log.write_text("\n".join(cap.all_lines()) + "\n", encoding="utf-8")
            print(f"[INFO] Wrote capture log: {cfg.output_log}")

        return 0 if all_passed else 1

    except (HTTPError, URLError, TimeoutError, RuntimeError, json.JSONDecodeError) as exc:
        print(f"[FAIL] Harness error: {exc}", file=sys.stderr)
        return 2
    finally:
        cap.stop()


def parse_args() -> HarnessConfig:
    p = argparse.ArgumentParser(description="Tab5 UI/control-plane pipeline harness")
    p.add_argument("--serial-port", default="/dev/tty.usbmodem1101")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--device-ip", default="192.168.1.109")
    p.add_argument("--http-port", type=int, default=80)
    p.add_argument("--settle-seconds", type=float, default=2.0)
    p.add_argument("--apply-timeout-seconds", type=float, default=6.0)
    p.add_argument("--screen-timeout-seconds", type=float, default=5.0)
    p.add_argument("--output-log", type=Path, default=None)
    args = p.parse_args()
    return HarnessConfig(
        serial_port=args.serial_port,
        baud=args.baud,
        device_ip=args.device_ip,
        http_port=args.http_port,
        settle_seconds=args.settle_seconds,
        apply_timeout_seconds=args.apply_timeout_seconds,
        screen_timeout_seconds=args.screen_timeout_seconds,
        output_log=args.output_log,
    )


if __name__ == "__main__":
    sys.exit(run_harness(parse_args()))
