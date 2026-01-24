#!/usr/bin/env python3
"""
Direct serial capture - reads serial port directly
"""
import serial
import sys
import time
import json
import websocket
import threading
from pathlib import Path

def capture_serial_direct(port, output_file, duration=10):
    """Capture serial output directly"""
    try:
        ser = serial.Serial(port, 115200, timeout=1)
        events = []
        start_time = time.time()
        
        while time.time() - start_time < duration:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line and ('"event":"' in line or '{' in line):
                    try:
                        # Try to parse as JSON
                        event = json.loads(line)
                        if event.get('event') in ('msg.recv', 'msg.send', 'ws.connect', 'ws.connected', 'ws.disconnect'):
                            events.append(line)
                    except:
                        # Not valid JSON, but might be part of event
                        if '"event":"' in line:
                            events.append(line)
            time.sleep(0.1)
        
        ser.close()
        
        # Write captured events
        with open(output_file, 'w') as f:
            for event in events:
                f.write(event + '\n')
        
        print(f"Captured {len(events)} events", file=sys.stderr)
        return len(events)
    except Exception as e:
        print(f"Serial error: {e}", file=sys.stderr)
        return 0

def run_scenario(scenario_name):
    """Run WebSocket scenario"""
    ws_url = "ws://lightwaveos.local/ws"
    
    try:
        ws = websocket.create_connection(ws_url, timeout=5)
        print(f"Connected for {scenario_name}", file=sys.stderr)
        
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
        print(f"Scenario {scenario_name} complete", file=sys.stderr)
    except Exception as e:
        print(f"WebSocket error: {e}", file=sys.stderr)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: capture_trace_direct.py <scenario> <output_dir>", file=sys.stderr)
        sys.exit(1)
    
    scenario = sys.argv[1]
    output_dir = Path(sys.argv[2])
    output_dir.mkdir(parents=True, exist_ok=True)
    
    jsonl_output = output_dir / f"{scenario}.jsonl"
    
    # Start serial capture in background thread
    print(f"Starting serial capture from /dev/cu.usbmodem1101", file=sys.stderr)
    events_captured = [0]
    
    def capture_thread():
        events_captured[0] = capture_serial_direct('/dev/cu.usbmodem1101', str(jsonl_output), 12)
    
    thread = threading.Thread(target=capture_thread, daemon=True)
    thread.start()
    
    time.sleep(2)  # Let serial capture start
    
    # Run scenario
    print(f"Running scenario: {scenario}", file=sys.stderr)
    run_scenario(scenario)
    
    time.sleep(2)  # Let events flush
    
    thread.join(timeout=3)
    
    print(f"Done. Captured {events_captured[0]} events to {jsonl_output}", file=sys.stderr)
