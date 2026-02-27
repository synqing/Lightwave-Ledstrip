#!/usr/bin/env python3
"""
Capture trace from hardware - runs scenario and captures serial output
"""
import subprocess
import sys
import time
import json
import websocket
import threading
from pathlib import Path

def capture_serial(port, output_file, duration=15):
    """Capture serial output for specified duration"""
    cmd = f"pio device monitor -p {port} -b 115200 -q --filter direct".split()
    try:
        with open(output_file, 'w') as f:
            proc = subprocess.Popen(cmd, stdout=f, stderr=subprocess.PIPE, text=True)
            time.sleep(duration)
            proc.terminate()
            proc.wait(timeout=2)
    except Exception as e:
        print(f"Serial capture error: {e}", file=sys.stderr)

def run_scenario(scenario_name):
    """Run WebSocket scenario"""
    ws_url = "ws://lightwaveos.local/ws"
    
    try:
        ws = websocket.create_connection(ws_url, timeout=5)
        print(f"Connected for {scenario_name}")
        
        if scenario_name == "happy_path":
            ws.send(json.dumps({"type": "device.getStatus"}))
            time.sleep(0.5)
            ws.send(json.dumps({"type": "color.enableBlend", "enable": True}))
            time.sleep(0.5)
            ws.send(json.dumps({"type": "color.getStatus"}))
            time.sleep(1)
        elif scenario_name == "validation_negative":
            ws.send(json.dumps({"type": "device.getStatus"}))
            time.sleep(0.5)
            # Send invalid - missing required field
            ws.send(json.dumps({"type": "color.enableBlend"}))  # Missing 'enable'
            time.sleep(1)
        elif scenario_name == "reconnect_churn":
            ws.send(json.dumps({"type": "device.getStatus"}))
            time.sleep(0.5)
            ws.close()
            time.sleep(2)
            ws = websocket.create_connection(ws_url, timeout=5)
            ws.send(json.dumps({"type": "device.getStatus"}))
            time.sleep(1)
        
        ws.close()
        print(f"Scenario {scenario_name} complete")
    except Exception as e:
        print(f"WebSocket error: {e}", file=sys.stderr)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: capture_trace.py <scenario> <output_dir>", file=sys.stderr)
        sys.exit(1)
    
    scenario = sys.argv[1]
    output_dir = Path(sys.argv[2])
    output_dir.mkdir(parents=True, exist_ok=True)
    
    serial_output = output_dir / f"{scenario}_raw.txt"
    jsonl_output = output_dir / f"{scenario}.jsonl"
    
    # Start serial capture in background
    print(f"Starting serial capture to {serial_output}")
    capture_thread = threading.Thread(
        target=capture_serial,
        args=("/dev/cu.usbmodem1101", str(serial_output), 15),
        daemon=True
    )
    capture_thread.start()
    
    time.sleep(2)  # Let serial capture start
    
    # Run scenario
    print(f"Running scenario: {scenario}")
    run_scenario(scenario)
    
    time.sleep(2)  # Let events flush
    
    # Extract JSONL
    print(f"Extracting JSONL to {jsonl_output}")
    with open(serial_output, 'r') as f_in, open(jsonl_output, 'w') as f_out:
        for line in f_in:
            if '"event":"msg.recv"' in line or '"event":"msg.send"' in line or '"event":"ws.connect"' in line or '"event":"ws.connected"' in line or '"event":"ws.disconnect"' in line:
                try:
                    json.loads(line.strip())  # Validate JSON
                    f_out.write(line)
                except:
                    pass
    
    print(f"Done. Check {jsonl_output}")
