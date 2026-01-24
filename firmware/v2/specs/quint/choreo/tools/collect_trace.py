#!/usr/bin/env python3
"""
Curated Trace Collection Script

Connects to LightwaveOS device via WebSocket and executes curated scenarios
for Choreo conformance checking. Captures JSONL telemetry events from serial output.

Usage:
    python3 collect_trace.py --host 192.168.0.27 --scenario happy_path --output traces/curated/
"""

import argparse
import json
import time
import websocket
import threading
import sys
from pathlib import Path

def on_message(ws, message):
    """Handle WebSocket messages (responses)"""
    pass  # Responses logged by firmware telemetry

def on_error(ws, error):
    print(f"WS ERROR: {error}", file=sys.stderr)

def on_close(ws, close_status_code, close_msg):
    pass

def run_happy_path(ws):
    """Scenario 1: Happy path - connect → getStatus → color.enableBlend → color.getStatus"""
    print("Executing happy_path scenario...")
    
    # Get status
    ws.send(json.dumps({"type": "device.getStatus", "requestId": "trace-1"}))
    time.sleep(0.5)
    
    # Enable blend
    ws.send(json.dumps({"type": "color.enableBlend", "enable": True, "requestId": "trace-2"}))
    time.sleep(0.5)
    
    # Get color status
    ws.send(json.dumps({"type": "color.getStatus", "requestId": "trace-3"}))
    time.sleep(0.5)
    
    print("Happy path scenario complete")

def run_validation_negative(ws):
    """Scenario 2: Validation negative - send color.enableBlend missing required field"""
    print("Executing validation_negative scenario...")
    
    # Send command missing required 'enable' field
    ws.send(json.dumps({"type": "color.enableBlend", "requestId": "trace-error-1"}))
    time.sleep(0.5)
    
    print("Validation negative scenario complete")

def run_reconnect_churn(ws, ws_url):
    """Scenario 3: Reconnect churn - connect → disconnect → reconnect → getStatus"""
    print("Executing reconnect_churn scenario...")
    
    # Get status on first connection
    ws.send(json.dumps({"type": "device.getStatus", "requestId": "trace-reconnect-1"}))
    time.sleep(0.5)
    
    # Disconnect
    ws.close()
    time.sleep(1.5)
    
    # Reconnect
    print("Reconnecting...")
    ws = websocket.WebSocketApp(
        ws_url,
        on_message=on_message,
        on_error=on_error,
        on_close=on_close,
    )
    thread = threading.Thread(target=ws.run_forever, daemon=True)
    thread.start()
    time.sleep(2)  # Wait for connection
    
    # Get status on reconnected session
    ws.send(json.dumps({"type": "device.getStatus", "requestId": "trace-reconnect-2"}))
    time.sleep(0.5)
    
    ws.close()
    print("Reconnect churn scenario complete")

def main():
    parser = argparse.ArgumentParser(description="Collect curated WebSocket traces for Choreo conformance")
    parser.add_argument("--host", default="lightwaveos.local", help="Device hostname or IP")
    parser.add_argument("--port", type=int, default=80, help="WebSocket port")
    parser.add_argument("--scenario", choices=["happy_path", "validation_negative", "reconnect_churn", "all"],
                       default="all", help="Scenario to execute")
    parser.add_argument("--output", type=Path, default=Path("traces/curated"),
                       help="Output directory for JSONL files")
    
    args = parser.parse_args()
    
    ws_url = f"ws://{args.host}:{args.port}/ws"
    args.output.mkdir(parents=True, exist_ok=True)
    
    scenarios_to_run = ["happy_path", "validation_negative", "reconnect_churn"] if args.scenario == "all" else [args.scenario]
    
    for scenario in scenarios_to_run:
        print(f"\n{'='*60}")
        print(f"Scenario: {scenario}")
        print(f"{'='*60}\n")
        
        # Connect to WebSocket
        ws = websocket.WebSocketApp(
            ws_url,
            on_message=on_message,
            on_error=on_error,
            on_close=on_close,
        )
        
        thread = threading.Thread(target=ws.run_forever, daemon=True)
        thread.start()
        time.sleep(2)  # Wait for connection
        
        # Execute scenario
        if scenario == "happy_path":
            run_happy_path(ws)
        elif scenario == "validation_negative":
            run_validation_negative(ws)
        elif scenario == "reconnect_churn":
            run_reconnect_churn(ws, ws_url)
        
        # Note: JSONL events are logged by firmware to serial output
        # Use extract_jsonl.py to filter and save from serial monitor
        print(f"\nScenario {scenario} complete. JSONL events are logged to serial output.")
        print(f"To extract: pio device monitor | python3 extract_jsonl.py > {args.output}/{scenario}.jsonl")
        
        if scenario != "reconnect_churn":  # Already closed in function
            time.sleep(0.5)
            ws.close()
        
        time.sleep(1)  # Pause between scenarios
    
    print("\n✓ All scenarios complete")
    print(f"\nTo extract telemetry events from serial output:")
    print(f"  pio device monitor | python3 extract_jsonl.py > {args.output}/trace.jsonl")

if __name__ == "__main__":
    main()
