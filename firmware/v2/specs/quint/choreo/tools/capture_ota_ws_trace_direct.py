#!/usr/bin/env python3
"""
Direct serial capture for OTA WebSocket traces - reads serial port directly and runs OTA WebSocket scenarios

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
import base64
from pathlib import Path

def capture_serial_direct(port, raw_output_file, duration=30):
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

def create_fake_firmware_binary(size_bytes=50000):
    """Create a fake firmware binary for testing (small enough for testing, not real flash)"""
    # Generate a simple pattern that's not all zeros (all zeros might compress)
    # Use a repeating pattern to keep it deterministic
    pattern = b'\x00\x01\x02\x03' * (size_bytes // 4 + 1)
    return pattern[:size_bytes]

def split_into_chunks(data, chunk_size=8192):
    """Split binary data into chunks of specified size"""
    chunks = []
    for i in range(0, len(data), chunk_size):
        chunks.append(data[i:i+chunk_size])
    return chunks

def run_ota_ws_happy_path(ws):
    """Scenario 1: ota_ws_happy_path - getStatus → ota.check → ota.begin → stream chunks → ota.verify (expect reboot)"""
    print("Executing ota_ws_happy_path scenario...", file=sys.stderr)
    
    # Handshake: getStatus
    ws.send(json.dumps({"type": "getStatus", "requestId": "ota-ws-happy-1"}))
    time.sleep(0.8)
    
    # Check OTA availability
    ws.send(json.dumps({"type": "ota.check", "requestId": "ota-ws-happy-2"}))
    time.sleep(0.5)  # Wait for ota.status response
    
    # Begin OTA session (using fake firmware - 50KB)
    firmware_data = create_fake_firmware_binary(50000)
    ws.send(json.dumps({
        "type": "ota.begin",
        "requestId": "ota-ws-happy-3",
        "size": len(firmware_data)
    }))
    time.sleep(0.5)  # Wait for ota.ready response
    
    # Send chunks (base64 encoded)
    chunks = split_into_chunks(firmware_data, 8192)
    offset = 0
    for i, chunk in enumerate(chunks):
        chunk_b64 = base64.b64encode(chunk).decode('ascii')
        ws.send(json.dumps({
            "type": "ota.chunk",
            "requestId": f"ota-ws-happy-chunk-{i}",
            "offset": offset,
            "data": chunk_b64
        }))
        offset += len(chunk)
        time.sleep(0.1)  # Small delay between chunks
    
    # Verify and complete (device will reboot)
    ws.send(json.dumps({
        "type": "ota.verify",
        "requestId": "ota-ws-happy-4"
    }))
    time.sleep(2.0)  # Wait for reboot
    
    print("ota_ws_happy_path scenario complete (device should reboot)", file=sys.stderr)

def run_ota_ws_reconnect_mid_transfer(ws, ws_url):
    """Scenario 2: ota_ws_reconnect_mid_transfer - begin → send some chunks → disconnect → reconnect → restart begin → complete"""
    print("Executing ota_ws_reconnect_mid_transfer scenario...", file=sys.stderr)
    
    # Handshake: getStatus
    ws.send(json.dumps({"type": "getStatus", "requestId": "ota-ws-reconnect-1"}))
    time.sleep(0.8)
    
    # Begin OTA session
    firmware_data = create_fake_firmware_binary(40000)
    ws.send(json.dumps({
        "type": "ota.begin",
        "requestId": "ota-ws-reconnect-2",
        "size": len(firmware_data)
    }))
    time.sleep(0.5)
    
    # Send first few chunks
    chunks = split_into_chunks(firmware_data, 8192)
    offset = 0
    for i in range(min(2, len(chunks))):  # Send first 2 chunks
        chunk = chunks[i]
        chunk_b64 = base64.b64encode(chunk).decode('ascii')
        ws.send(json.dumps({
            "type": "ota.chunk",
            "requestId": f"ota-ws-reconnect-chunk-{i}",
            "offset": offset,
            "data": chunk_b64
        }))
        offset += len(chunk)
        time.sleep(0.1)
    
    # Disconnect mid-transfer (before completing)
    print("Disconnecting mid-transfer...", file=sys.stderr)
    ws.close()
    time.sleep(2.0)  # Wait for disconnect to process
    
    # Reconnect (epoch increments, OTA session should be aborted)
    print("Reconnecting...", file=sys.stderr)
    ws = websocket.create_connection(ws_url, timeout=5)
    
    # Handshake on reconnected session
    ws.send(json.dumps({"type": "getStatus", "requestId": "ota-ws-reconnect-3"}))
    time.sleep(0.8)
    
    # Restart OTA session (new begin)
    ws.send(json.dumps({
        "type": "ota.begin",
        "requestId": "ota-ws-reconnect-4",
        "size": len(firmware_data)
    }))
    time.sleep(0.5)
    
    # Send all chunks this time
    offset = 0
    for i, chunk in enumerate(chunks):
        chunk_b64 = base64.b64encode(chunk).decode('ascii')
        ws.send(json.dumps({
            "type": "ota.chunk",
            "requestId": f"ota-ws-reconnect-chunk2-{i}",
            "offset": offset,
            "data": chunk_b64
        }))
        offset += len(chunk)
        time.sleep(0.1)
    
    # Verify (device will reboot)
    ws.send(json.dumps({
        "type": "ota.verify",
        "requestId": "ota-ws-reconnect-5"
    }))
    time.sleep(2.0)
    
    print("ota_ws_reconnect_mid_transfer scenario complete (device should reboot)", file=sys.stderr)

def run_ota_ws_abort_and_retry(ws, ws_url):
    """Scenario 3: ota_ws_abort_and_retry - begin → send some chunks → ota.abort → begin again → complete"""
    print("Executing ota_ws_abort_and_retry scenario...", file=sys.stderr)
    
    # Handshake: getStatus
    ws.send(json.dumps({"type": "getStatus", "requestId": "ota-ws-abort-1"}))
    time.sleep(0.8)
    
    # First OTA begin
    firmware_data = create_fake_firmware_binary(30000)
    ws.send(json.dumps({
        "type": "ota.begin",
        "requestId": "ota-ws-abort-2",
        "size": len(firmware_data)
    }))
    time.sleep(0.5)
    
    # Send a few chunks
    chunks = split_into_chunks(firmware_data, 8192)
    offset = 0
    for i in range(min(2, len(chunks))):
        chunk = chunks[i]
        chunk_b64 = base64.b64encode(chunk).decode('ascii')
        ws.send(json.dumps({
            "type": "ota.chunk",
            "requestId": f"ota-ws-abort-chunk1-{i}",
            "offset": offset,
            "data": chunk_b64
        }))
        offset += len(chunk)
        time.sleep(0.1)
    
    # Abort the session
    print("Aborting OTA session...", file=sys.stderr)
    ws.send(json.dumps({
        "type": "ota.abort",
        "requestId": "ota-ws-abort-3"
    }))
    time.sleep(0.5)  # Wait for abort response
    
    # Begin again (retry)
    print("Starting OTA session again...", file=sys.stderr)
    ws.send(json.dumps({
        "type": "ota.begin",
        "requestId": "ota-ws-abort-4",
        "size": len(firmware_data)
    }))
    time.sleep(0.5)
    
    # Send all chunks
    offset = 0
    for i, chunk in enumerate(chunks):
        chunk_b64 = base64.b64encode(chunk).decode('ascii')
        ws.send(json.dumps({
            "type": "ota.chunk",
            "requestId": f"ota-ws-abort-chunk2-{i}",
            "offset": offset,
            "data": chunk_b64
        }))
        offset += len(chunk)
        time.sleep(0.1)
    
    # Verify and complete (device will reboot)
    ws.send(json.dumps({
        "type": "ota.verify",
        "requestId": "ota-ws-abort-5"
    }))
    time.sleep(2.0)
    
    print("ota_ws_abort_and_retry scenario complete (device should reboot)", file=sys.stderr)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: capture_ota_ws_trace_direct.py <scenario> <output_dir> [host]", file=sys.stderr)
        print("  scenario: ota_ws_happy_path | ota_ws_reconnect_mid_transfer | ota_ws_abort_and_retry", file=sys.stderr)
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
    valid_scenarios = ["ota_ws_happy_path", "ota_ws_reconnect_mid_transfer", "ota_ws_abort_and_retry"]
    if scenario not in valid_scenarios:
        print(f"ERROR: Invalid scenario '{scenario}'. Must be one of: {', '.join(valid_scenarios)}", file=sys.stderr)
        sys.exit(1)
    
    # Start serial capture in background thread
    print(f"Starting serial capture from /dev/cu.usbmodem1101 to {raw_output}", file=sys.stderr)
    lines_captured = [0]
    capture_error = [None]
    
    def capture_thread():
        try:
            # Longer duration for OTA (reboots take time)
            lines_captured[0] = capture_serial_direct('/dev/cu.usbmodem1101', str(raw_output), 45)
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
        
        if scenario == "ota_ws_happy_path":
            run_ota_ws_happy_path(ws)
        elif scenario == "ota_ws_reconnect_mid_transfer":
            run_ota_ws_reconnect_mid_transfer(ws, ws_url)
        elif scenario == "ota_ws_abort_and_retry":
            run_ota_ws_abort_and_retry(ws, ws_url)
        
        # Note: happy_path and abort_and_retry will reboot device, so ws may close
        
    except Exception as e:
        print(f"WebSocket error: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc(file=sys.stderr)
    
    time.sleep(3)  # Let events flush (especially after reboot)
    
    # Wait for capture thread to complete
    thread.join(timeout=60)  # Longer timeout for OTA scenarios
    
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
    print(f"Extracting JSONL events...", file=sys.stderr)
    script_dir = Path(__file__).parent
    extract_script = script_dir / "extract_jsonl.py"
    
    try:
        result = subprocess.run(
            [sys.executable, str(extract_script), str(raw_output)],
            capture_output=True,
            text=True,
            check=True
        )
        
        # Write extracted JSONL to output file
        with open(jsonl_output, 'w') as f:
            f.write(result.stdout)
        
        # Print extraction stats (from stderr)
        if result.stderr:
            print(result.stderr, file=sys.stderr)
        
        print(f"JSONL extracted to {jsonl_output}", file=sys.stderr)
        
    except subprocess.CalledProcessError as e:
        print(f"ERROR: extract_jsonl.py failed: {e}", file=sys.stderr)
        if e.stderr:
            print(e.stderr, file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"ERROR: Failed to run extract_jsonl.py: {e}", file=sys.stderr)
        sys.exit(1)
