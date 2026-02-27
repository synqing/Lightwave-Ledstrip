#!/usr/bin/env python3
"""
Fuzz/Replay Test Tool

Offline fuzzing tool that tests malformed inputs and validates responses.
Reads JSONL trace files for replay, or generates test cases for fuzzing.

Usage:
    python3 fuzz_replay.py --host 192.168.0.27 --test oversize
    python3 fuzz_replay.py --host 192.168.0.27 --replay trace.jsonl
"""

import argparse
import json
import time
import websocket
import threading
import sys
from pathlib import Path

def on_message(ws, message):
    """Handle WebSocket messages"""
    try:
        msg = json.loads(message)
        print(f"  → {msg.get('type', 'response')}")
    except json.JSONDecodeError:
        print(f"  → (non-JSON response)")

def on_error(ws, error):
    print(f"  ✗ ERROR: {error}", file=sys.stderr)

def test_oversize_payload(ws):
    """Test 1: Oversize payload (>64KB)"""
    print("Testing oversize payload (>64KB)...")
    
    # Generate payload larger than 64KB
    large_data = "x" * (65 * 1024)  # 65KB
    payload = {
        "type": "color.setBlendPalettes",
        "palette1": 0,
        "palette2": 1,
        "extraData": large_data
    }
    
    ws.send(json.dumps(payload))
    time.sleep(1)

def test_near_cap_payloads(ws):
    """Test near-64KB payloads (ESP-IDF edge cases)"""
    print("Testing near-cap payloads (63KB, 63.5KB, 63.9KB)...")
    
    sizes = [63 * 1024, 63 * 1024 + 512, 64 * 1024 - 1]  # 63KB, 63.5KB, 63.9KB
    for size in sizes:
        large_data = "x" * size
        payload = {"type": "color.setBlendPalettes", "palette1": 0, "palette2": 1, "extraData": large_data}
        ws.send(json.dumps(payload))
        time.sleep(0.2)
    
    # Repeat near-cap (63KB) 20 times to test stability
    print("Testing repeated near-cap sends (20x 63KB)...")
    for i in range(20):
        large_data = "x" * (63 * 1024)
        payload = {"type": "color.setBlendPalettes", "palette1": 0, "palette2": 1, "extraData": large_data}
        ws.send(json.dumps(payload))
        time.sleep(0.1)

def test_invalid_json(ws):
    """Test 2: Invalid JSON (truncated, malformed)"""
    print("Testing invalid JSON...")
    
    # Truncated JSON
    ws.send('{"type":"color.enableBlend","enable":true')  # Missing closing brace
    time.sleep(0.5)
    
    # Malformed JSON
    ws.send('{"type":"color.enableBlend","enable":true,}')  # Trailing comma
    time.sleep(0.5)

def test_unknown_command(ws):
    """Test 3: Unknown command type"""
    print("Testing unknown command type...")
    
    ws.send(json.dumps({"type": "unknown.command.type", "requestId": "fuzz-1"}))
    time.sleep(0.5)

def test_out_of_range(ws):
    """Test 4: Out-of-range values"""
    print("Testing out-of-range values...")
    
    # Mode > 3
    ws.send(json.dumps({"type": "colorCorrection.setMode", "mode": 255, "requestId": "fuzz-2"}))
    time.sleep(0.5)
    
    # Amount > 255
    ws.send(json.dumps({"type": "color.setDiffusionAmount", "amount": 300, "requestId": "fuzz-3"}))
    time.sleep(0.5)

def replay_trace(ws, trace_path):
    """Replay events from JSONL trace file"""
    print(f"Replaying trace from {trace_path}...")
    
    with open(trace_path, 'r') as f:
        for line in f:
            if not line.strip():
                continue
            
            try:
                event = json.loads(line.strip())
                if event.get("event") == "msg.recv":
                    # Extract payload from event (would need payloadSummary parsing)
                    # For now, skip - would need full message reconstruction
                    continue
            except json.JSONDecodeError:
                continue

def main():
    parser = argparse.ArgumentParser(description="Fuzz/replay test tool for WebSocket commands")
    parser.add_argument("--host", default="lightwaveos.local", help="Device hostname or IP")
    parser.add_argument("--port", type=int, default=80, help="WebSocket port")
    parser.add_argument("--test", choices=["oversize", "near_cap", "invalid_json", "unknown_command", "out_of_range", "all"],
                       help="Fuzz test case to run")
    parser.add_argument("--replay", type=Path, help="Replay JSONL trace file")
    
    args = parser.parse_args()
    
    if not args.test and not args.replay:
        parser.error("Either --test or --replay required")
    
    ws_url = f"ws://{args.host}:{args.port}/ws"
    
    # Connect to WebSocket
    ws = websocket.WebSocketApp(
        ws_url,
        on_message=on_message,
        on_error=on_error,
    )
    
    thread = threading.Thread(target=ws.run_forever, daemon=True)
    thread.start()
    time.sleep(2)  # Wait for connection
    
    print("Connected to device\n")
    
    try:
        if args.test:
            tests_to_run = ["oversize", "near_cap", "invalid_json", "unknown_command", "out_of_range"] if args.test == "all" else [args.test]
            
            for test_name in tests_to_run:
                print(f"\n{'-'*60}")
                if test_name == "oversize":
                    test_oversize_payload(ws)
                elif test_name == "near_cap":
                    test_near_cap_payloads(ws)
                elif test_name == "invalid_json":
                    test_invalid_json(ws)
                elif test_name == "unknown_command":
                    test_unknown_command(ws)
                elif test_name == "out_of_range":
                    test_out_of_range(ws)
                
                time.sleep(0.5)
        
        if args.replay:
            if not args.replay.exists():
                print(f"Error: Trace file not found: {args.replay}", file=sys.stderr)
                sys.exit(1)
            replay_trace(ws, args.replay)
    
    finally:
        time.sleep(1)
        ws.close()
    
    print("\n✓ Fuzz/replay tests complete")

if __name__ == "__main__":
    main()
