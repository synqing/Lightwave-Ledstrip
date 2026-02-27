#!/usr/bin/env python3
"""
Direct serial capture for zones traces - reads serial port directly and runs zones WebSocket scenarios

Captures raw serial output (with ESP_LOG prefixes) and post-processes through extract_jsonl.py
to guarantee valid JSONL even with log prefixes.
"""
import serial
import sys
import time
import json
import websocket
import threading
import subprocess
from pathlib import Path

def capture_serial_direct(port, raw_output_file, duration=15):
    """Capture raw serial output directly using pyserial (includes ESP_LOG prefixes)"""
    try:
        # Open serial port with explicit settings
        ser = serial.Serial(port, 115200, timeout=1, write_timeout=1)
        lines = []
        start_time = time.time()
        
        # Flush any existing input
        ser.reset_input_buffer()
        
        print(f"Serial port {port} opened, capturing for {duration}s...", file=sys.stderr)
        
        while time.time() - start_time < duration:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='ignore')
                if line:
                    lines.append(line)
                    # Print first few lines for debugging
                    if len(lines) <= 5:
                        print(f"Captured: {line[:80]}", file=sys.stderr)
            else:
                time.sleep(0.1)  # Small delay when no data
        
        ser.close()
        
        # Write raw capture (will be post-processed with extract_jsonl.py)
        if lines:
            with open(raw_output_file, 'w') as f:
                for line in lines:
                    f.write(line)
            print(f"Captured {len(lines)} lines to {raw_output_file}", file=sys.stderr)
        else:
            # Create empty file so post-processing doesn't fail
            Path(raw_output_file).touch()
            print(f"WARNING: No lines captured, created empty file {raw_output_file}", file=sys.stderr)
        
        return len(lines)
    except serial.SerialException as e:
        print(f"Serial port error: {e}", file=sys.stderr)
        print(f"  Port: {port}", file=sys.stderr)
        print(f"  Make sure device is connected and port is not in use", file=sys.stderr)
        return 0
    except Exception as e:
        print(f"Serial error: {e}", file=sys.stderr)
        return 0

def run_zones_happy_path(ws):
    """Scenario 1: zones_happy_path - connect → getStatus → zones.get → zones.update"""
    print("Executing zones_happy_path scenario...", file=sys.stderr)
    
    # Handshake: getStatus
    ws.send(json.dumps({"type": "getStatus", "requestId": "zones-happy-1"}))
    time.sleep(0.8)  # Wait for status response
    
    # Get zones state
    ws.send(json.dumps({"type": "zones.get", "requestId": "zones-happy-2"}))
    time.sleep(0.8)  # Wait for zones.list response
    
    # Update zone (zoneId=0, effectId=1, brightness=200)
    ws.send(json.dumps({
        "type": "zones.update",
        "requestId": "zones-happy-3",
        "zoneId": 0,
        "effectId": 1,
        "brightness": 200
    }))
    time.sleep(1.0)  # Wait for zones.changed broadcast
    
    print("zones_happy_path scenario complete", file=sys.stderr)

def run_zones_reconnect_mid_update(ws, ws_url):
    """Scenario 2: zones_duplicate_or_reorder (reconnect-mid-update) - zones.update then disconnect before zones.changed"""
    print("Executing zones_reconnect_mid_update scenario...", file=sys.stderr)
    
    # Handshake: getStatus
    ws.send(json.dumps({"type": "getStatus", "requestId": "zones-reconnect-1"}))
    time.sleep(0.8)
    
    # Get zones state
    ws.send(json.dumps({"type": "zones.get", "requestId": "zones-reconnect-2"}))
    time.sleep(0.8)
    
    # Send zones.update
    ws.send(json.dumps({
        "type": "zones.update",
        "requestId": "zones-reconnect-3",
        "zoneId": 0,
        "effectId": 2,
        "brightness": 150
    }))
    time.sleep(0.3)  # Short wait, then disconnect BEFORE zones.changed
    
    # Disconnect before zones.changed arrives
    print("Disconnecting before zones.changed...", file=sys.stderr)
    ws.close()
    time.sleep(2.0)
    
    # Reconnect
    print("Reconnecting...", file=sys.stderr)
    ws = websocket.create_connection(ws_url, timeout=5)
    
    # Handshake on reconnected session
    ws.send(json.dumps({"type": "getStatus", "requestId": "zones-reconnect-4"}))
    time.sleep(0.8)
    
    # Get zones state again (should see updated state from previous update)
    ws.send(json.dumps({"type": "zones.get", "requestId": "zones-reconnect-5"}))
    time.sleep(0.8)
    
    # Send another zones.update to complete flow
    ws.send(json.dumps({
        "type": "zones.update",
        "requestId": "zones-reconnect-6",
        "zoneId": 0,
        "effectId": 3,
        "speed": 75
    }))
    time.sleep(1.0)
    
    ws.close()
    print("zones_reconnect_mid_update scenario complete", file=sys.stderr)

def run_zones_reconnect_churn(ws, ws_url):
    """Scenario 3: zones_reconnect_churn - connect → start zones flow → disconnect → reconnect → complete"""
    print("Executing zones_reconnect_churn scenario...", file=sys.stderr)
    
    # First connection: start zones flow
    ws.send(json.dumps({"type": "getStatus", "requestId": "zones-churn-1"}))
    time.sleep(0.8)
    
    ws.send(json.dumps({"type": "zones.get", "requestId": "zones-churn-2"}))
    time.sleep(0.5)
    
    # Disconnect mid-flow
    print("Disconnecting mid-flow...", file=sys.stderr)
    ws.close()
    time.sleep(1.5)
    
    # Reconnect (epoch increments)
    print("Reconnecting (epoch increments)...", file=sys.stderr)
    ws = websocket.create_connection(ws_url, timeout=5)
    
    # Complete zones flow on new connection
    ws.send(json.dumps({"type": "getStatus", "requestId": "zones-churn-3"}))
    time.sleep(0.8)
    
    ws.send(json.dumps({"type": "zones.get", "requestId": "zones-churn-4"}))
    time.sleep(0.8)
    
    ws.send(json.dumps({
        "type": "zones.update",
        "requestId": "zones-churn-5",
        "zoneId": 1,
        "effectId": 5,
        "paletteId": 2
    }))
    time.sleep(1.0)
    
    ws.close()
    print("zones_reconnect_churn scenario complete", file=sys.stderr)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: capture_zones_trace_direct.py <scenario> <output_dir> [host]", file=sys.stderr)
        print("  scenario: zones_happy_path | zones_reconnect_mid_update | zones_reconnect_churn", file=sys.stderr)
        print("  output_dir: directory for JSONL output", file=sys.stderr)
        print("  host: WebSocket host (default: lightwaveos.local)", file=sys.stderr)
        sys.exit(1)
    
    scenario = sys.argv[1]
    output_dir = Path(sys.argv[2])
    host = sys.argv[3] if len(sys.argv) > 3 else "lightwaveos.local"
    ws_url = f"ws://{host}/ws"
    
    output_dir.mkdir(parents=True, exist_ok=True)
    raw_output = output_dir / f"{scenario}_raw.txt"
    jsonl_output = output_dir / f"{scenario}.jsonl"
    
    # Validate scenario name
    valid_scenarios = ["zones_happy_path", "zones_reconnect_mid_update", "zones_reconnect_churn"]
    if scenario not in valid_scenarios:
        print(f"ERROR: Invalid scenario '{scenario}'. Must be one of: {', '.join(valid_scenarios)}", file=sys.stderr)
        sys.exit(1)
    
    # Start serial capture in background thread
    print(f"Starting serial capture from /dev/cu.usbmodem1101 to {raw_output}", file=sys.stderr)
    lines_captured = [0]
    capture_error = [None]
    
    def capture_thread():
        try:
            lines_captured[0] = capture_serial_direct('/dev/cu.usbmodem1101', str(raw_output), 25)
        except Exception as e:
            capture_error[0] = e
            print(f"Capture thread error: {e}", file=sys.stderr)
    
    thread = threading.Thread(target=capture_thread, daemon=False)  # Not daemon so it completes
    thread.start()
    
    time.sleep(2)  # Let serial capture start
    
    # Run scenario
    print(f"Running scenario: {scenario}", file=sys.stderr)
    try:
        ws = websocket.create_connection(ws_url, timeout=5)
        print(f"WebSocket connected to {ws_url}", file=sys.stderr)
        
        if scenario == "zones_happy_path":
            run_zones_happy_path(ws)
            ws.close()
        elif scenario == "zones_reconnect_mid_update":
            run_zones_reconnect_mid_update(ws, ws_url)
        elif scenario == "zones_reconnect_churn":
            run_zones_reconnect_churn(ws, ws_url)
        
    except Exception as e:
        print(f"WebSocket error: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc(file=sys.stderr)
    
    time.sleep(2)  # Let events flush
    
    # Wait for capture thread to complete
    thread.join(timeout=30)
    
    if capture_error[0]:
        print(f"ERROR: Capture failed: {capture_error[0]}", file=sys.stderr)
        sys.exit(1)
    
    if not raw_output.exists() or raw_output.stat().st_size == 0:
        print(f"WARNING: Raw capture file is empty or not created: {raw_output}", file=sys.stderr)
        print(f"  This may indicate:", file=sys.stderr)
        print(f"  1. Device not connected at /dev/cu.usbmodem1101", file=sys.stderr)
        print(f"  2. Serial port in use by another process", file=sys.stderr)
        print(f"  3. Device not outputting telemetry events", file=sys.stderr)
        print(f"  Check: ls -la /dev/cu.usbmodem*", file=sys.stderr)
        if lines_captured[0] == 0:
            print(f"  No lines were captured from serial port.", file=sys.stderr)
            sys.exit(1)
    
    print(f"Done. Captured {lines_captured[0]} lines to {raw_output}", file=sys.stderr)
    
    # Post-process with extract_jsonl.py
    print(f"Extracting JSONL events from raw capture...", file=sys.stderr)
    try:
        import subprocess
        with open(raw_output, 'r') as f_in, open(jsonl_output, 'w') as f_out:
            result = subprocess.run(
                [sys.executable, str(Path(__file__).parent / 'extract_jsonl.py')],
                stdin=f_in,
                stdout=f_out,
                stderr=subprocess.PIPE,
                text=True,
                check=True
            )
            if result.stderr:
                print(result.stderr, file=sys.stderr)
        
        # Count extracted events
        event_count = 0
        with open(jsonl_output, 'r') as f:
            event_count = sum(1 for line in f if line.strip())
        
        print(f"Extracted {event_count} JSONL events to {jsonl_output}", file=sys.stderr)
    except subprocess.CalledProcessError as e:
        print(f"ERROR: Failed to extract JSONL: {e}", file=sys.stderr)
        print(f"  stderr: {e.stderr}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"ERROR: Failed to extract JSONL: {e}", file=sys.stderr)
        sys.exit(1)
    
    print(f"\nNext steps:", file=sys.stderr)
    print(f"  1. Convert to ITF: python3 tools/jsonl_to_itf.py {jsonl_output} {output_dir}/{scenario}.itf.json", file=sys.stderr)
    print(f"  2. Validate ADR-015: python3 tools/validate_itf_bigint.py {output_dir}/{scenario}.itf.json", file=sys.stderr)
    print(f"  3. Check conformance: python3 tools/check_conformance.py {jsonl_output}", file=sys.stderr)
