#!/usr/bin/env python3
"""
K1 Input Merge Layer Integration Test Suite (Experiment 4.3)

Tests merge layer behavior over USB serial. 6 tests covering:
1. Baseline capture (MANUAL only)
2. Merge dump — parameter-level verification (not pixel-level)
3. HTP merge — AI_AGENT brightness via merged param values
4. LTP merge — AI_AGENT hue via merged param values
5. Multi-source conflict — MANUAL vs AI_AGENT priority arbitration
6. Staleness timeout — AI_AGENT goes stale, MANUAL resumes

Usage:
    python3 tools/test_merge_layer.py [--port /dev/cu.usbmodem21401]

Requirements: pyserial
"""

import serial
import time
import re
import sys
import argparse


# ============================================================================
# Parsers
# ============================================================================

def parse_vrms(text: str) -> dict | None:
    patterns = {
        'dominantHue': r'Dominant Hue:\s+([\d.]+)',
        'colourVariance': r'Colour Variance:\s+([\d.]+)',
        'spatialCentroid': r'Spatial Centroid:\s+([\d.]+)',
        'symmetryScore': r'Symmetry Score:\s+([-\d.]+)',
        'brightnessMean': r'Brightness Mean:\s+([\d.]+)',
        'brightnessVariance': r'Brightness Var:\s+([\d.]+)',
        'temporalFreq': r'Temporal Freq:\s+([\d.]+)',
        'audioVisualCorr': r'Audio-Visual Corr:\s+([-\d.]+)',
    }
    result = {}
    for key, pattern in patterns.items():
        m = re.search(pattern, text)
        if m:
            result[key] = float(m.group(1))
    return result if len(result) == 8 else None


def parse_merge_dump(text: str) -> dict:
    """Parse merge dump output into {merged_params, manual_state, vrms}."""
    result = {'merged': {}, 'manual': {}, 'vrms': {}}

    # Merged params
    merged_block = re.search(r'MERGED_PARAMS_START(.*?)MERGED_PARAMS_END', text, re.DOTALL)
    if merged_block:
        for m in re.finditer(r'(\w+)=(\d+)', merged_block.group(1)):
            result['merged'][m.group(1)] = int(m.group(2))

    # Manual state
    manual_block = re.search(r'MANUAL_STATE_START(.*?)MANUAL_STATE_END', text, re.DOTALL)
    if manual_block:
        for m in re.finditer(r'(\w+)=(\d+)', manual_block.group(1)):
            result['manual'][m.group(1)] = int(m.group(2))

    # VRMS
    vrms_block = re.search(r'VRMS_START(.*?)VRMS_END', text, re.DOTALL)
    if vrms_block:
        for m in re.finditer(r'(\w+)=([-\d.]+)', vrms_block.group(1)):
            result['vrms'][m.group(1)] = float(m.group(2))

    return result


# ============================================================================
# Serial helpers
# ============================================================================

def send_cmd(ser: serial.Serial, cmd: str, wait: float = 0.5) -> str:
    ser.reset_input_buffer()
    ser.write((cmd + "\n").encode())
    time.sleep(wait)
    response = ""
    deadline = time.time() + 2.0
    while time.time() < deadline:
        if ser.in_waiting > 0:
            response += ser.read(ser.in_waiting).decode(errors='replace')
            time.sleep(0.05)
        else:
            time.sleep(0.1)
    return response


def capture_vrms(ser: serial.Serial) -> dict | None:
    for _ in range(3):
        resp = send_cmd(ser, "vrms", wait=0.5)
        metrics = parse_vrms(resp)
        if metrics:
            return metrics
        time.sleep(0.5)
    return None


def capture_dump(ser: serial.Serial) -> dict:
    for _ in range(3):
        resp = send_cmd(ser, "merge dump", wait=0.5)
        dump = parse_merge_dump(resp)
        if dump['merged']:
            return dump
        time.sleep(0.5)
    return {'merged': {}, 'manual': {}, 'vrms': {}}


def merge_param(ser: serial.Serial, param: str, value: int, source: int = 2) -> bool:
    resp = send_cmd(ser, f"merge {param} {value} {source}", wait=0.3)
    return "[MERGE]" in resp


# ============================================================================
# Tests
# ============================================================================

def test_1_baseline(ser: serial.Serial) -> tuple[bool, dict]:
    """Test 1: Capture baseline — MANUAL only, no AI_AGENT."""
    print("\n[TEST 1] Baseline capture (MANUAL only)")
    dump = capture_dump(ser)
    if not dump['merged']:
        print("  FAIL: Could not capture merge dump")
        return False, {}

    print(f"  Merged brightness={dump['merged'].get('brightness', '?')}")
    print(f"  Manual brightness={dump['manual'].get('brightness', '?')}")
    print(f"  VRMS brightnessMean={dump['vrms'].get('brightnessMean', '?')}")
    print(f"  PASS: Baseline captured")
    return True, dump


def test_2_merge_dump_verification(ser: serial.Serial) -> bool:
    """Test 2: Verify merge dump shows parameter values, not pixel output."""
    print("\n[TEST 2] Merge dump parameter verification")
    dump = capture_dump(ser)
    if not dump['merged']:
        print("  FAIL: No merged params")
        return False

    required = ['brightness', 'speed', 'intensity', 'saturation',
                'complexity', 'variation', 'hue', 'mood', 'fadeAmount', 'paletteIdx']
    missing = [k for k in required if k not in dump['merged']]
    if missing:
        print(f"  FAIL: Missing params: {missing}")
        return False

    print(f"  All 10 params present: {list(dump['merged'].keys())}")
    print(f"  PASS: Merge dump verified")
    return True


def test_3_htp_brightness(ser: serial.Serial, baseline: dict) -> bool:
    """Test 3: HTP merge — AI_AGENT brightness=255 should override if higher."""
    print("\n[TEST 3] HTP brightness — AI_AGENT=255 vs MANUAL")

    manual_brt = baseline['merged'].get('brightness', 128)
    print(f"  Manual brightness: {manual_brt}")

    # Send max brightness from AI_AGENT
    if not merge_param(ser, "brightness", 255):
        print("  FAIL: Command not accepted")
        return False

    # Wait for IIR convergence (tau=2.0s, 4s ≈ 87%)
    print("  Waiting 5s for IIR convergence (tau=2.0s)...")
    time.sleep(5.0)

    dump = capture_dump(ser)
    merged_brt = dump['merged'].get('brightness', 0)
    print(f"  Merged brightness after AI: {merged_brt}")

    # HTP: merged should be max(MANUAL, AI_AGENT_smoothed)
    # After 5s with tau=2.0: smoothed ≈ 255 * (1 - e^(-5/2)) ≈ 255 * 0.918 ≈ 234
    if merged_brt > manual_brt:
        print(f"  PASS: Merged ({merged_brt}) > Manual ({manual_brt}) — HTP working")
        return True
    elif merged_brt >= 230:
        print(f"  PASS: Merged at {merged_brt} (AI converging toward 255)")
        return True
    else:
        print(f"  FAIL: Merged ({merged_brt}) not higher than Manual ({manual_brt})")
        return False


def test_4_ltp_hue(ser: serial.Serial, baseline: dict) -> bool:
    """Test 4: LTP merge — AI_AGENT hue should override MANUAL (higher priority)."""
    print("\n[TEST 4] LTP hue — AI_AGENT (priority 80) vs MANUAL (priority 50)")

    manual_hue = baseline['merged'].get('hue', 0)
    # Pick a hue far from current
    target_hue = (manual_hue + 128) % 256
    print(f"  Manual hue: {manual_hue}, Target: {target_hue}")

    if not merge_param(ser, "hue", target_hue):
        print("  FAIL: Command not accepted")
        return False

    print("  Waiting 5s for convergence...")
    time.sleep(5.0)

    dump = capture_dump(ser)
    merged_hue = dump['merged'].get('hue', 0)
    print(f"  Merged hue: {merged_hue}")

    # LTP: AI_AGENT (priority 80) should win over MANUAL (priority 50)
    # After smoothing, merged should be closer to target than to manual
    dist_to_target = min(abs(merged_hue - target_hue), 256 - abs(merged_hue - target_hue))
    dist_to_manual = min(abs(merged_hue - manual_hue), 256 - abs(merged_hue - manual_hue))

    if dist_to_target < dist_to_manual:
        print(f"  PASS: Merged hue closer to AI target ({dist_to_target}) than Manual ({dist_to_manual})")
        return True
    else:
        print(f"  FAIL: Merged hue closer to Manual ({dist_to_manual}) than AI target ({dist_to_target})")
        return False


def test_5_multi_source_conflict(ser: serial.Serial) -> bool:
    """Test 5: Multi-source conflict — MANUAL and AI_AGENT compete on brightness."""
    print("\n[TEST 5] Multi-source conflict — HTP arbitration")

    # Set AI brightness to 50 (low) — MANUAL should win if MANUAL > 50
    if not merge_param(ser, "brightness", 50):
        print("  FAIL: Command not accepted")
        return False

    print("  AI_AGENT brightness=50, waiting 5s...")
    time.sleep(5.0)

    dump = capture_dump(ser)
    manual_brt = dump['manual'].get('brightness', 0)
    merged_brt = dump['merged'].get('brightness', 0)

    print(f"  Manual brightness: {manual_brt}")
    print(f"  AI_AGENT brightness: 50 (smoothed)")
    print(f"  Merged brightness: {merged_brt}")

    # HTP: max(MANUAL, AI_smoothed). If MANUAL > 50, MANUAL wins.
    if manual_brt > 50 and merged_brt >= manual_brt - 2:
        print(f"  PASS: MANUAL ({manual_brt}) wins over AI (50) — HTP correct")
        return True
    elif manual_brt <= 50 and merged_brt >= 45:
        print(f"  PASS: AI (~50) wins because MANUAL ({manual_brt}) is lower — HTP correct")
        return True
    else:
        print(f"  FAIL: Unexpected merged value")
        return False


def test_6_staleness(ser: serial.Serial, baseline: dict) -> bool:
    """Test 6: AI_AGENT goes stale after 10s — merged values revert to MANUAL."""
    print("\n[TEST 6] Staleness timeout — AI_AGENT expires after 10s")

    # Set AI brightness high
    merge_param(ser, "brightness", 255)
    merge_param(ser, "hue", 0)
    time.sleep(5.0)

    dump_during = capture_dump(ser)
    merged_brt_during = dump_during['merged'].get('brightness', 0)
    merged_hue_during = dump_during['merged'].get('hue', 0)
    print(f"  During AI: brightness={merged_brt_during}, hue={merged_hue_during}")

    # Now stop sending — wait for 10s staleness + margin
    print("  Waiting 13s for staleness timeout...")
    time.sleep(13.0)

    dump_after = capture_dump(ser)
    merged_brt_after = dump_after['merged'].get('brightness', 0)
    manual_brt = dump_after['manual'].get('brightness', 0)
    print(f"  After stale: brightness={merged_brt_after}, manual={manual_brt}")

    # After staleness, merged should revert to MANUAL
    if abs(merged_brt_after - manual_brt) <= 3:
        print(f"  PASS: Merged reverted to Manual ({manual_brt}) — staleness working")
        return True
    elif merged_brt_after < merged_brt_during:
        print(f"  PASS: Brightness dropped ({merged_brt_during} → {merged_brt_after}) — staleness effective")
        return True
    else:
        print(f"  FAIL: Merged ({merged_brt_after}) didn't revert toward Manual ({manual_brt})")
        return False


# ============================================================================
# Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description="K1 Merge Layer Integration Test Suite")
    parser.add_argument("--port", default="/dev/cu.usbmodem21401", help="Serial port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    args = parser.parse_args()

    print(f"{'=' * 55}")
    print(f"  K1 Merge Layer Integration Test (Experiment 4.3)")
    print(f"  Port: {args.port} | Baud: {args.baud}")
    print(f"{'=' * 55}")

    try:
        ser = serial.Serial(args.port, args.baud, timeout=2)
    except serial.SerialException as e:
        print(f"ERROR: Cannot open {args.port}: {e}")
        sys.exit(1)

    time.sleep(2)
    ser.reset_input_buffer()

    results = {}

    baseline_data = {}
    ok, baseline_data = test_1_baseline(ser)
    results['1_baseline'] = ok

    results['2_dump_verify'] = test_2_merge_dump_verification(ser)
    results['3_htp_brightness'] = test_3_htp_brightness(ser, baseline_data)
    results['4_ltp_hue'] = test_4_ltp_hue(ser, baseline_data)
    results['5_conflict'] = test_5_multi_source_conflict(ser)
    results['6_staleness'] = test_6_staleness(ser, baseline_data)

    ser.close()

    print(f"\n{'=' * 55}")
    print("RESULTS")
    print(f"{'=' * 55}")
    passed = 0
    for name, result in results.items():
        status = "PASS" if result else "FAIL"
        marker = "✓" if result else "✗"
        print(f"  {marker} {name:25s} {status}")
        if result:
            passed += 1

    total = len(results)
    print(f"\n  {passed}/{total} tests passed")
    print(f"{'=' * 55}")
    sys.exit(0 if passed == total else 1)


if __name__ == "__main__":
    main()
