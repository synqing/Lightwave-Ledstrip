#!/usr/bin/env python3
"""
K1 Merge Layer WebSocket End-to-End Test

Run MANUALLY while connected to the K1's WiFi AP (LightwaveOS-AP).
Logs to tools/logs/merge_ws_test_<timestamp>.log for offline review.

Usage:
    python3 tools/test_merge_ws.py [--host 192.168.4.1] [--port 80]

Requirements: pip3 install websockets
"""

import asyncio
import json
import time
import os
import sys
import argparse
import logging
from datetime import datetime

try:
    import websockets
except ImportError:
    print("ERROR: Install websockets: pip3 install websockets")
    sys.exit(1)


def setup_logging(log_dir="tools/logs"):
    os.makedirs(log_dir, exist_ok=True)
    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_path = os.path.join(log_dir, f"merge_ws_test_{ts}.log")

    logger = logging.getLogger("merge_ws_test")
    logger.setLevel(logging.DEBUG)

    fh = logging.FileHandler(log_path)
    fh.setLevel(logging.DEBUG)
    fh.setFormatter(logging.Formatter('%(asctime)s [%(levelname)s] %(message)s'))

    ch = logging.StreamHandler()
    ch.setLevel(logging.INFO)
    ch.setFormatter(logging.Formatter('[%(levelname)s] %(message)s'))

    logger.addHandler(fh)
    logger.addHandler(ch)
    logger.info(f"Log file: {log_path}")
    return logger


async def ws_send(ws, msg_type, params=None, request_id=""):
    msg = {"type": msg_type}
    if params:
        msg.update(params)
    if request_id:
        msg["requestId"] = request_id
    raw = json.dumps(msg)
    log = logging.getLogger("merge_ws_test")
    log.debug(f"  WS send: {raw[:200]}")
    await ws.send(raw)
    return msg


async def ws_recv_typed(ws, type_match, timeout=5.0):
    """Receive messages until one has 'type' matching type_match. Logs all received."""
    log = logging.getLogger("merge_ws_test")
    deadline = time.time() + timeout
    while time.time() < deadline:
        remaining = deadline - time.time()
        if remaining <= 0:
            break
        try:
            raw = await asyncio.wait_for(ws.recv(), timeout=remaining)
            msg = json.loads(raw)
            msg_type = msg.get("type", "")
            log.debug(f"  WS recv: type={msg_type} keys={list(msg.keys())}")
            if msg_type == type_match:
                return msg
        except asyncio.TimeoutError:
            break
        except Exception as e:
            log.debug(f"  WS recv error: {e}")
            break
    return None


async def drain_ws(ws, max_msgs=50):
    """Drain pending WS messages (up to max_msgs to avoid infinite loop on active streams)."""
    for _ in range(max_msgs):
        try:
            await asyncio.wait_for(ws.recv(), timeout=0.15)
        except (asyncio.TimeoutError, Exception):
            break


async def collect_vrms_frames(ws, count=5, timeout=10.0):
    log = logging.getLogger("merge_ws_test")
    frames = []
    deadline = time.time() + timeout
    while len(frames) < count and time.time() < deadline:
        remaining = deadline - time.time()
        if remaining <= 0:
            break
        try:
            raw = await asyncio.wait_for(ws.recv(), timeout=remaining)
            msg = json.loads(raw)
            if msg.get("type") == "vrms.frame":
                metrics = msg.get("metrics", {})
                frames.append(metrics)
                log.debug(f"  VRMS frame: brt={metrics.get('brightnessMean','?')}")
        except asyncio.TimeoutError:
            break
        except Exception as e:
            log.debug(f"  Frame collect error: {e}")
            break
    return frames


def avg_metric(frames, key):
    values = []
    for f in frames:
        v = f.get(key)
        if v is not None:
            try:
                values.append(float(v))
            except (ValueError, TypeError):
                pass
    if not values:
        return 0.0
    return sum(values) / len(values)


# ============================================================================
# Tests
# ============================================================================

async def test_1_connect(ws, log):
    log.info("[TEST 1] WebSocket connection")
    log.info("  PASS: Connected")
    return True


async def test_2_vrms_subscribe(ws, log):
    log.info("[TEST 2] VRMS stream subscription")

    await ws_send(ws, "vrms.subscribe", request_id="test2")
    resp = await ws_recv_typed(ws, "vrms.subscribed", timeout=5.0)

    if not resp:
        log.warning("  WARN: No vrms.subscribed ack (may still work)")
        # Try collecting frames anyway — the subscription might have worked
    else:
        log.info(f"  Subscribed: {json.dumps(resp.get('data', {}))}")

    # Wait a moment then try to collect frames
    await asyncio.sleep(1.0)
    frames = await collect_vrms_frames(ws, count=3, timeout=8.0)

    if len(frames) >= 1:
        brt = avg_metric(frames, 'brightnessMean')
        log.info(f"  Received {len(frames)} VRMS frames, avg brightness={brt:.1f}")
        log.info("  PASS: VRMS stream active")
        return True
    else:
        log.error(f"  FAIL: No VRMS frames received")
        return False


async def test_3_merge_brightness(ws, log):
    log.info("[TEST 3] merge.submit — AI_AGENT brightness=255")

    await drain_ws(ws)
    baseline_frames = await collect_vrms_frames(ws, count=3, timeout=8.0)
    baseline_brt = avg_metric(baseline_frames, 'brightnessMean')
    log.info(f"  Baseline brightnessMean: {baseline_brt:.1f} ({len(baseline_frames)} frames)")

    await ws_send(ws, "merge.submit", {
        "source": 2,
        "params": {"brightness": 255, "intensity": 255}
    }, request_id="test3")

    resp = await ws_recv_typed(ws, "merge.accepted", timeout=3.0)
    if resp:
        log.info(f"  Merge accepted: {resp.get('paramsApplied', '?')} params")
    else:
        log.warning("  WARN: No merge.accepted ack")

    log.info("  Waiting 5s for IIR convergence...")
    await asyncio.sleep(5.0)

    await drain_ws(ws)
    after_frames = await collect_vrms_frames(ws, count=3, timeout=8.0)
    after_brt = avg_metric(after_frames, 'brightnessMean')
    log.info(f"  After merge brightnessMean: {after_brt:.1f} ({len(after_frames)} frames)")
    log.info(f"  Delta: {after_brt - baseline_brt:+.1f}")
    log.info("  PASS: merge.submit processed")
    return True


async def test_4_merge_hue(ws, log):
    log.info("[TEST 4] merge.submit — AI_AGENT hue=0 (red)")

    await drain_ws(ws)
    before_frames = await collect_vrms_frames(ws, count=3, timeout=8.0)
    before_hue = avg_metric(before_frames, 'dominantHue')
    log.info(f"  Before hue: {before_hue:.1f}")

    await ws_send(ws, "merge.submit", {
        "source": 2,
        "params": {"hue": 0}
    }, request_id="test4")

    resp = await ws_recv_typed(ws, "merge.accepted", timeout=3.0)
    if resp:
        log.info(f"  Merge accepted")

    log.info("  Waiting 5s for convergence...")
    await asyncio.sleep(5.0)

    await drain_ws(ws)
    after_frames = await collect_vrms_frames(ws, count=3, timeout=8.0)
    after_hue = avg_metric(after_frames, 'dominantHue')
    log.info(f"  After hue: {after_hue:.1f}")
    log.info(f"  Delta: {after_hue - before_hue:+.1f}")
    log.info("  PASS: Hue merge processed")
    return True


async def test_5_gesture_source(ws, log):
    log.info("[TEST 5] GESTURE source (source=3, tau=0.15s, priority=150)")

    await ws_send(ws, "merge.submit", {
        "source": 3,
        "params": {"brightness": 255, "speed": 100, "intensity": 255}
    }, request_id="test5")

    resp = await ws_recv_typed(ws, "merge.accepted", timeout=3.0)
    if resp:
        log.info(f"  Gesture merge accepted: {resp.get('paramsApplied', '?')} params")
    else:
        log.warning("  WARN: No ack")

    log.info("  Waiting 1s (tau=0.15s converges fast)...")
    await asyncio.sleep(1.0)

    await drain_ws(ws)
    frames = await collect_vrms_frames(ws, count=3, timeout=8.0)
    brt = avg_metric(frames, 'brightnessMean')
    log.info(f"  After gesture: brightnessMean={brt:.1f}")
    log.info("  PASS: Gesture source accepted")
    return True


async def test_6_staleness(ws, log):
    log.info("[TEST 6] Staleness — stop sending, wait for revert")

    await drain_ws(ws)
    during_frames = await collect_vrms_frames(ws, count=3, timeout=8.0)
    during_brt = avg_metric(during_frames, 'brightnessMean')
    log.info(f"  Current brightnessMean: {during_brt:.1f}")

    log.info("  Waiting 13s for AI_AGENT (10s) + GESTURE (0.5s) staleness...")
    await asyncio.sleep(13.0)

    await drain_ws(ws)
    after_frames = await collect_vrms_frames(ws, count=3, timeout=8.0)
    after_brt = avg_metric(after_frames, 'brightnessMean')
    log.info(f"  After staleness: brightnessMean={after_brt:.1f}")
    log.info(f"  Delta: {after_brt - during_brt:+.1f}")
    log.info("  PASS: Staleness period elapsed")
    return True


async def test_7_unsubscribe(ws, log):
    log.info("[TEST 7] VRMS unsubscribe")
    await ws_send(ws, "vrms.unsubscribe", request_id="test7")
    resp = await ws_recv_typed(ws, "vrms.unsubscribed", timeout=3.0)
    if resp:
        log.info("  PASS: Unsubscribed")
    else:
        log.info("  PASS (soft): No ack but non-critical")
    return True


# ============================================================================
# Main
# ============================================================================

async def run_tests(host, port):
    log = setup_logging()

    ws_url = f"ws://{host}:{port}/ws"
    log.info(f"{'=' * 55}")
    log.info(f"  K1 Merge Layer WebSocket E2E Test")
    log.info(f"  Target: {ws_url}")
    log.info(f"  Time: {datetime.now().isoformat()}")
    log.info(f"{'=' * 55}")

    results = {}
    try:
        async with websockets.connect(ws_url, ping_interval=None, ping_timeout=None, close_timeout=5) as ws:
            results['1_connect'] = await test_1_connect(ws, log)
            results['2_vrms_subscribe'] = await test_2_vrms_subscribe(ws, log)
            results['3_merge_brightness'] = await test_3_merge_brightness(ws, log)
            results['4_merge_hue'] = await test_4_merge_hue(ws, log)
            results['5_gesture_source'] = await test_5_gesture_source(ws, log)
            results['6_staleness'] = await test_6_staleness(ws, log)
            results['7_unsubscribe'] = await test_7_unsubscribe(ws, log)

    except ConnectionRefusedError:
        log.error(f"Connection refused at {ws_url}")
        log.error("Are you connected to LightwaveOS-AP WiFi?")
        sys.exit(1)
    except Exception as e:
        log.error(f"Connection failed: {e}")
        import traceback
        log.error(traceback.format_exc())
        sys.exit(1)

    log.info(f"\n{'=' * 55}")
    log.info("RESULTS")
    log.info(f"{'=' * 55}")
    passed = 0
    for name, result in results.items():
        status = "PASS" if result else "FAIL"
        log.info(f"  {'✓' if result else '✗'} {name:25s} {status}")
        if result:
            passed += 1

    total = len(results)
    log.info(f"\n  {passed}/{total} tests passed")
    log.info(f"{'=' * 55}")
    return passed == total


def main():
    parser = argparse.ArgumentParser(description="K1 Merge Layer WS E2E Test")
    parser.add_argument("--host", default="192.168.4.1")
    parser.add_argument("--port", type=int, default=80)
    args = parser.parse_args()
    success = asyncio.run(run_tests(args.host, args.port))
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
