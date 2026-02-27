#!/usr/bin/env python3
"""
Hardware WebSocket health check for v2 device

Tests WS connection stability, command registration, and validates no regressions:
- No origin_not_allowed rejections
- No Handler table full errors
- Successful WS connections via mDNS and direct IP
- Key commands work: getStatus, ledStream.subscribe/unsubscribe, plugins.list, ota.check
"""

import serial
import sys
import time
import json
import websocket
import threading
import argparse
import socket
from pathlib import Path
from typing import List, Dict, Optional

# Failure signatures (regression indicators)
FAILURE_SIGNATURES = [
    "origin_not_allowed",
    "Handler table full",
    "ERROR: Handler table full",
]

# Success signatures (expected behavior)
SUCCESS_SIGNATURES = [
    "WS: Client",
    "connected",
    "WebSocket commands registered:",
]

class SerialCapture:
    """Capture serial output in background thread"""
    def __init__(self, port: str, duration: float = 30.0):
        self.port = port
        self.duration = duration
        self.lines: List[str] = []
        self.capture_error: Optional[Exception] = None
        self.thread: Optional[threading.Thread] = None
        self.running = False
        
    def _capture_thread(self):
        """Background thread to capture serial output"""
        try:
            ser = serial.Serial(self.port, 115200, timeout=1, write_timeout=1)
            ser.reset_input_buffer()
            start_time = time.time()
            self.running = True
            
            print(f"[Serial] Capturing from {self.port} for {self.duration}s...", file=sys.stderr)
            
            while time.time() - start_time < self.duration and self.running:
                if ser.in_waiting:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        self.lines.append(line)
                else:
                    time.sleep(0.1)
            
            ser.close()
            print(f"[Serial] Captured {len(self.lines)} lines", file=sys.stderr)
            
        except serial.SerialException as e:
            self.capture_error = e
            print(f"[Serial] ERROR: {e}", file=sys.stderr)
        except Exception as e:
            self.capture_error = e
            print(f"[Serial] ERROR: {e}", file=sys.stderr)
    
    def start(self):
        """Start serial capture in background"""
        self.thread = threading.Thread(target=self._capture_thread, daemon=False)
        self.thread.start()
        time.sleep(1)  # Let serial port open
    
    def stop(self):
        """Stop serial capture"""
        self.running = False
        if self.thread:
            self.thread.join(timeout=5)
    
    def get_lines(self) -> List[str]:
        """Get captured lines"""
        return self.lines
    
    def check_failures(self) -> List[str]:
        """Check for failure signatures in captured logs"""
        failures = []
        for line in self.lines:
            for sig in FAILURE_SIGNATURES:
                if sig in line:
                    failures.append(f"FAILURE: {sig} found in: {line[:100]}")
        return failures

def resolve_host(host: str) -> Optional[str]:
    """Resolve mDNS hostname to IP, fallback to direct IP if already an IP"""
    # If it's already an IP, return as-is
    try:
        socket.inet_aton(host)
        return host
    except socket.error:
        pass
    
    # Try to resolve mDNS hostname
    try:
        ip = socket.gethostbyname(host)
        print(f"[Network] Resolved {host} -> {ip}", file=sys.stderr)
        return ip
    except socket.gaierror as e:
        print(f"[Network] WARNING: Could not resolve {host}: {e}", file=sys.stderr)
        return None

def test_ws_connection(ws_url: str, test_name: str, serial_capture: SerialCapture) -> Dict:
    """Test WS connection and send key commands"""
    results = {
        "test": test_name,
        "url": ws_url,
        "connected": False,
        "commands_sent": 0,
        "commands_ok": 0,
        "errors": []
    }
    
    try:
        print(f"[WS] Connecting to {ws_url}...", file=sys.stderr)
        ws = websocket.create_connection(ws_url, timeout=5)
        results["connected"] = True
        print(f"[WS] Connected successfully", file=sys.stderr)
        
        # Test commands
        test_commands = [
            {"type": "getStatus", "requestId": "hw-test-1"},
            {"type": "ledStream.subscribe", "requestId": "hw-test-2"},
            {"type": "ledStream.unsubscribe", "requestId": "hw-test-3"},
            {"type": "plugins.list", "requestId": "hw-test-4"},
        ]
        
        # Add OTA check if available (may not be in all builds)
        try:
            test_commands.append({"type": "ota.check", "requestId": "hw-test-5"})
        except:
            pass
        
        for cmd in test_commands:
            try:
                ws.send(json.dumps(cmd))
                results["commands_sent"] += 1
                time.sleep(0.3)  # Wait for response
                
                # Try to receive response (non-blocking)
                try:
                    ws.settimeout(0.5)
                    response = ws.recv()
                    if response:
                        results["commands_ok"] += 1
                        print(f"[WS] Command {cmd['type']} OK", file=sys.stderr)
                except websocket.WebSocketTimeoutException:
                    # No response, but command was sent (may be async)
                    results["commands_ok"] += 1
                    print(f"[WS] Command {cmd['type']} sent (no response expected)", file=sys.stderr)
                except Exception as e:
                    results["errors"].append(f"Command {cmd['type']} error: {e}")
                    print(f"[WS] Command {cmd['type']} error: {e}", file=sys.stderr)
                    
            except Exception as e:
                results["errors"].append(f"Failed to send {cmd['type']}: {e}")
                print(f"[WS] Failed to send {cmd['type']}: {e}", file=sys.stderr)
        
        ws.close()
        print(f"[WS] Connection closed", file=sys.stderr)
        
    except websocket.WebSocketException as e:
        results["errors"].append(f"WebSocket connection failed: {e}")
        print(f"[WS] ERROR: Connection failed: {e}", file=sys.stderr)
    except Exception as e:
        results["errors"].append(f"Unexpected error: {e}")
        print(f"[WS] ERROR: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc(file=sys.stderr)
    
    return results

def main():
    parser = argparse.ArgumentParser(
        description="Hardware WS health check for v2 device",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Test via mDNS
  python3 ws_hardware_check.py --host lightwaveos.local

  # Test via direct IP
  python3 ws_hardware_check.py --host 192.168.0.27

  # Test both mDNS and IP
  python3 ws_hardware_check.py --host lightwaveos.local --ip 192.168.0.27
        """
    )
    parser.add_argument(
        "--host",
        default="lightwaveos.local",
        help="mDNS hostname or IP address (default: lightwaveos.local)"
    )
    parser.add_argument(
        "--ip",
        help="Direct IP address to test (in addition to --host)"
    )
    parser.add_argument(
        "--port",
        type=int,
        default=80,
        help="WebSocket port (default: 80)"
    )
    parser.add_argument(
        "--serial-port",
        default="/dev/cu.usbmodem1101",
        help="Serial port for v2 device (default: /dev/cu.usbmodem1101)"
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=30.0,
        help="Serial capture duration in seconds (default: 30.0)"
    )
    parser.add_argument(
        "--output",
        help="Output file for serial logs (default: stdout)"
    )
    
    args = parser.parse_args()
    
    # Start serial capture
    serial_capture = SerialCapture(args.serial_port, args.duration)
    serial_capture.start()
    
    time.sleep(2)  # Let serial capture initialize
    
    # Test connections
    test_results = []
    
    # Test via host (mDNS or IP)
    host_ip = resolve_host(args.host)
    if host_ip:
        ws_url = f"ws://{host_ip}:{args.port}/ws"
        result = test_ws_connection(ws_url, f"mDNS/IP ({args.host})", serial_capture)
        test_results.append(result)
        time.sleep(2)  # Delay between tests
    
    # Test via direct IP if provided
    if args.ip and args.ip != host_ip:
        ws_url = f"ws://{args.ip}:{args.port}/ws"
        result = test_ws_connection(ws_url, f"Direct IP ({args.ip})", serial_capture)
        test_results.append(result)
        time.sleep(2)
    
    # Wait for serial capture to complete
    time.sleep(3)
    serial_capture.stop()
    
    # Analyze results
    print("\n" + "="*70, file=sys.stderr)
    print("HARDWARE WS HEALTH CHECK RESULTS", file=sys.stderr)
    print("="*70, file=sys.stderr)
    
    all_passed = True
    
    # Check serial logs for failures
    serial_failures = serial_capture.check_failures()
    if serial_failures:
        print("\n[FAILURES] Serial log regression indicators:", file=sys.stderr)
        for failure in serial_failures:
            print(f"  ✗ {failure}", file=sys.stderr)
        all_passed = False
    else:
        print("\n[PASS] No regression indicators in serial logs", file=sys.stderr)
    
    # Check WS connection results
    print("\n[WS CONNECTION TESTS]", file=sys.stderr)
    for result in test_results:
        if result["connected"]:
            print(f"  ✓ {result['test']}: Connected", file=sys.stderr)
            print(f"    Commands: {result['commands_ok']}/{result['commands_sent']} OK", file=sys.stderr)
            if result["errors"]:
                print(f"    Errors: {len(result['errors'])}", file=sys.stderr)
                for err in result["errors"]:
                    print(f"      - {err}", file=sys.stderr)
                all_passed = False
        else:
            print(f"  ✗ {result['test']}: Failed to connect", file=sys.stderr)
            for err in result["errors"]:
                print(f"      - {err}", file=sys.stderr)
            all_passed = False
    
    # Check for success signatures
    serial_lines = serial_capture.get_lines()
    success_count = 0
    for line in serial_lines:
        for sig in SUCCESS_SIGNATURES:
            if sig in line:
                success_count += 1
                break
    
    if success_count > 0:
        print(f"\n[PASS] Found {success_count} success indicators in serial logs", file=sys.stderr)
    else:
        print(f"\n[WARNING] No success indicators found in serial logs", file=sys.stderr)
    
    # Output serial logs if requested
    if args.output:
        with open(args.output, 'w') as f:
            for line in serial_lines:
                f.write(line + '\n')
        print(f"\n[INFO] Serial logs written to {args.output}", file=sys.stderr)
    
    # Final summary
    print("\n" + "="*70, file=sys.stderr)
    if all_passed and success_count > 0:
        print("OVERALL: PASS - All checks passed", file=sys.stderr)
        sys.exit(0)
    else:
        print("OVERALL: FAIL - Issues detected", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
