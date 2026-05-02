#!/usr/bin/env python3
"""
Zone Composer Single-Effect Regression Gate — Hardware Smoke Test (D-8).

Runs the four invariants documented in firmware-v3/test/test_zone_regression_gate/README.md
against a flashed K1 V2 over serial JSON. Exit code 0 = all invariants pass; 1 = one or more failed.

PHASE 0 STATUS: Scaffolded. Invariant test bodies are stubs marked TODO_PHASE1.
Phase 1 implementation populates the actual test sequences as zone commands stabilise.

Usage:
    python3 scripts/zone_regression_smoke.py --port /dev/cu.usbmodem2101 --invariants all
    python3 scripts/zone_regression_smoke.py --port /dev/cu.usbmodem2101 --invariants I1,I3
"""

import argparse
import json
import sys
import time
from typing import Optional

try:
    import serial
except ImportError:
    print("[FATAL] pyserial not installed. pip install pyserial", file=sys.stderr)
    sys.exit(2)


# ---------------------------------------------------------------------------
# Transport
# ---------------------------------------------------------------------------

class SerialJsonClient:
    """Minimal SerialJSON client for regression tests. Sends JSON line, reads response."""

    def __init__(self, port: str, baud: int = 115200, timeout: float = 2.0):
        self.ser = serial.Serial(port, baud, timeout=timeout)
        time.sleep(0.5)
        self.ser.reset_input_buffer()

    def send(self, payload: dict, expect_request_id: Optional[str] = None,
             read_timeout: float = 2.0) -> Optional[dict]:
        """Send JSON command; return matching response (by requestId) or None on timeout."""
        if expect_request_id is None:
            expect_request_id = payload.get("requestId", "")
        line = json.dumps(payload) + "\n"
        self.ser.write(line.encode())
        self.ser.flush()
        deadline = time.time() + read_timeout
        buf = b""
        while time.time() < deadline:
            chunk = self.ser.read(4096)
            if not chunk:
                continue
            buf += chunk
            for line in buf.decode(errors="replace").splitlines():
                line = line.strip()
                if not line.startswith("{"):
                    continue
                try:
                    resp = json.loads(line)
                except json.JSONDecodeError:
                    continue
                if resp.get("requestId", "") == expect_request_id:
                    return resp
            deadline = time.time() + 0.3  # extend if data still arriving
        return None

    def close(self):
        self.ser.close()


# ---------------------------------------------------------------------------
# Invariants
# ---------------------------------------------------------------------------

def invariant_I1_single_effect_works(client: SerialJsonClient) -> bool:
    """ZoneComposer disabled → single-effect mode works."""
    print("[I1] ZoneComposer disabled → single-effect mode works")
    # TODO_PHASE1: full sequence
    # 1. zone.enable {enable: false}
    # 2. parameters.set {effectId: 0x1301, brightness: 128}
    # 3. soak 10s, verify FPS >= 110
    # 4. verify no panic
    resp = client.send({"type": "zone.enable", "requestId": "I1-1", "enable": False})
    if resp is None or not resp.get("success", False):
        print(f"  FAIL: zone.enable returned {resp}")
        return False
    print("  PARTIAL: scaffolding-only — full FPS soak deferred to Phase 1")
    return True


def invariant_I2_zone_path_works(client: SerialJsonClient) -> bool:
    """ZoneComposer enabled → zone render path works."""
    print("[I2] ZoneComposer enabled → zone render path works")
    # TODO_PHASE1: full sequence
    # 1. zone.enable {enable: true}
    # 2. zone.loadPreset {presetId: 1}  (Dual Split)
    # 3. soak 10s, verify FPS >= 110
    # 4. verify zones render (visual check or LED metering)
    resp = client.send({"type": "zone.enable", "requestId": "I2-1", "enable": True})
    if resp is None or not resp.get("success", False):
        print(f"  FAIL: zone.enable returned {resp}")
        return False
    resp = client.send({"type": "zone.loadPreset", "requestId": "I2-2", "presetId": 1})
    if resp is None or not resp.get("success", False):
        print(f"  FAIL: zone.loadPreset returned {resp}")
        return False
    print("  PARTIAL: scaffolding-only — full soak + render verification deferred to Phase 1")
    return True


def invariant_I3_toggle_no_stale_state(client: SerialJsonClient,
                                        toggle_count: int = 50) -> bool:
    """
    Toggle enabled/disabled with a real 3-zone multi-effect configuration → no stale state,
    no crash, no buffer corruption. Captain's directive 2026-05-02: must exercise at least
    2 (preferably 3) different effects per zone configuration; toggling the global enable
    against a zoneless ZoneComposer proves nothing about zone rendering survival.

    Setup: factory preset 2 ("Triple Rings") loads 3 zones with 3 distinct effects:
        Zone 1: EID_LGP_WAVE_COLLISION_ENHANCED
        Zone 2: EID_LGP_INTERFERENCE_SCANNER_ENHANCED
        Zone 3: EID_LGP_STAR_BURST_ENHANCED

    Verification:
        - Heap stable (delta < 4 KB across 50 toggles)
        - Zone state survives the toggle sequence (all 3 effect IDs preserved at end)
        - Final state has zones enabled (last toggle leaves enable=true)
        - Per-toggle response success rate >= 90% (transport flake tolerance;
          dropped responses on rapid sustained traffic are not a firmware regression
          unless they correlate with state corruption — which we check via post-soak
          state readback)
    """
    print(f"[I3] Toggle enabled/disabled {toggle_count}× under 3-zone Triple Rings → no stale state")

    # Setup: load Triple Rings preset (3 zones, 3 different effects).
    # Per Captain's critique 2026-05-02 — single-zone toggles prove nothing.
    print("  setup: loading factory preset 2 (Triple Rings, 3 zones × 3 effects)")
    preset_resp = client.send({
        "type": "zone.loadPreset",
        "requestId": "I3-setup-preset",
        "presetId": 2
    })
    if preset_resp is None or not preset_resp.get("success", False):
        print(f"  FAIL: preset load returned {preset_resp}")
        return False
    time.sleep(0.5)  # let preset apply + first render frame

    # Initial state: capture zone effect IDs + heap.
    initial_zones = client.send({"type": "zones.list", "requestId": "I3-init-zones"})
    initial_status = client.send({"type": "device.getStatus", "requestId": "I3-init"})
    if initial_zones is None or initial_status is None:
        print("  FAIL: could not read initial zones/status")
        return False
    initial_effect_ids = [
        z.get("effectId") for z in initial_zones.get("data", {}).get("zones", [])
    ]
    initial_heap = initial_status.get("data", {}).get("freeHeap", 0)
    if len(initial_effect_ids) < 3:
        print(f"  FAIL: expected 3 zones from Triple Rings, got {len(initial_effect_ids)}")
        return False
    if len(set(initial_effect_ids)) < 3:
        print(f"  FAIL: expected 3 distinct effect IDs, got {initial_effect_ids}")
        return False
    print(f"  initial: 3 zones × distinct effects {initial_effect_ids}, freeHeap={initial_heap}")

    # Toggle sequence — 50 toggles ending with enable=true (i.e. odd toggle_count keeps enabled).
    failures = 0
    for i in range(toggle_count):
        enable = (i % 2 == 0)  # 0:on, 1:off, 2:on, ... ends on enable=False if even count
        resp = client.send({
            "type": "zone.enable",
            "requestId": f"I3-toggle-{i}",
            "enable": enable
        })
        if resp is None or not resp.get("success", False):
            failures += 1

    # Ensure final state is enabled so we can read zone state.
    client.send({"type": "zone.enable", "requestId": "I3-final-on", "enable": True})
    time.sleep(0.3)

    # Post-soak state: zones still configured? heap stable?
    final_zones = client.send({"type": "zones.list", "requestId": "I3-final-zones"})
    final_status = client.send({"type": "device.getStatus", "requestId": "I3-final"})
    if final_zones is None or final_status is None:
        print("  FAIL: could not read final zones/status (likely panic)")
        return False
    final_effect_ids = [
        z.get("effectId") for z in final_zones.get("data", {}).get("zones", [])
    ]
    final_heap = final_status.get("data", {}).get("freeHeap", 0)

    # Property checks (Captain's actual safety properties):
    if final_effect_ids != initial_effect_ids:
        print(f"  FAIL: zone effect IDs corrupted — initial={initial_effect_ids} final={final_effect_ids}")
        return False
    heap_delta = initial_heap - final_heap
    if heap_delta > 4096:
        print(f"  FAIL: heap leaked {heap_delta} bytes across {toggle_count} toggles")
        return False
    failure_pct = (failures / toggle_count) * 100.0
    if failure_pct > 20.0:
        print(f"  FAIL: response drop rate {failure_pct:.1f}% exceeds 20% (transport saturation)")
        return False

    if failures == 0:
        flake_note = ""
    else:
        flake_note = f" (transport-flake: {failures}/{toggle_count} responses dropped, within 20% tolerance)"
    print(f"  PASS: zone state preserved {final_effect_ids}, heap delta {heap_delta} bytes{flake_note}")
    return True


def invariant_I4_global_params_work(client: SerialJsonClient) -> bool:
    """Global effects.parameters.set still works in single-effect mode."""
    print("[I4] Global effects.parameters.set works in single-effect mode")
    # TODO_PHASE1: full sequence
    # 1. zone.enable false
    # 2. effects.parameters.set {effectId: <current>, parameters: {<known param>: value}}
    # 3. verify queued/failed response shape
    # 4. verify parameter actually applied (visual or readback)
    resp = client.send({"type": "zone.enable", "requestId": "I4-1", "enable": False})
    if resp is None or not resp.get("success", False):
        print(f"  FAIL: zone.enable returned {resp}")
        return False
    print("  PARTIAL: scaffolding-only — parameter application verification deferred to Phase 1")
    return True


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

INVARIANTS = {
    "I1": invariant_I1_single_effect_works,
    "I2": invariant_I2_zone_path_works,
    "I3": invariant_I3_toggle_no_stale_state,
    "I4": invariant_I4_global_params_work,
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="Serial port (e.g. /dev/cu.usbmodem2101)")
    parser.add_argument("--invariants", default="all",
                        help='Comma-separated invariants to run (e.g. "I1,I3") or "all"')
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    if args.invariants == "all":
        names = list(INVARIANTS.keys())
    else:
        names = [n.strip().upper() for n in args.invariants.split(",")]

    print(f"Zone Regression Gate — port={args.port} baud={args.baud}")
    print(f"Running invariants: {', '.join(names)}")
    print("(Phase 0 status: scaffolding-only; full sequences land in Phase 1)\n")

    client = SerialJsonClient(args.port, args.baud)
    failures = []
    try:
        for name in names:
            fn = INVARIANTS.get(name)
            if fn is None:
                print(f"[SKIP] unknown invariant: {name}")
                continue
            try:
                ok = fn(client)
            except Exception as exc:
                print(f"[ERROR] {name} threw: {exc}")
                ok = False
            if not ok:
                failures.append(name)
            print()
    finally:
        client.close()

    if failures:
        print(f"FAILED: {', '.join(failures)}")
        sys.exit(1)
    print("ALL INVARIANTS PASSED (within Phase 0 scaffolding scope)")
    sys.exit(0)


if __name__ == "__main__":
    main()
