#!/usr/bin/env python3
"""
Hardware WebSocket health check for Tab5 encoder device

Monitors Tab5 WS connection stability and validates:
- Successful WS connection to v2 device
- No rapid disconnect loops
- Hello message exchange works
- Connection remains stable over test duration
"""

import serial
import sys
import time
import threading
import argparse
import re
from pathlib import Path
from typing import List, Dict, Optional, Tuple
from collections import deque

# Failure signatures (regression indicators)
FAILURE_SIGNATURES = [
    "origin_not_allowed",
    "Connection lost",
    "Reconnecting",
    "ERROR",
    "Failed to connect",
]

# Success signatures (expected behavior)
SUCCESS_SIGNATURES = [
    "[WS] Connected to server",
    "[WS] Sending hello",
    "[WS] Server IP:",
]

# Disconnect pattern (to detect rapid reconnects)
DISCONNECT_PATTERN = re.compile(r'\[WS\]\s+Disconnected')

class SerialMonitor:
    """Monitor serial output and detect WS connection patterns"""
    def __init__(self, port: str, duration: float = 60.0):
        self.port = port
        self.duration = duration
        self.lines: List[str] = []
        self.capture_error: Optional[Exception] = None
        self.thread: Optional[threading.Thread] = None
        self.running = False
        self.disconnect_times: deque = deque(maxlen=10)  # Track last 10 disconnects
        self.connect_times: deque = deque(maxlen=10)  # Track last 10 connects
        
    def _monitor_thread(self):
        """Background thread to monitor serial output"""
        try:
            ser = serial.Serial(self.port, 115200, timeout=1, write_timeout=1)
            ser.reset_input_buffer()
            start_time = time.time()
            self.running = True
            
            print(f"[Serial] Monitoring {self.port} for {self.duration}s...", file=sys.stderr)
            
            while time.time() - start_time < self.duration and self.running:
                if ser.in_waiting:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        self.lines.append(line)
                        self._analyze_line(line, time.time())
                else:
                    time.sleep(0.1)
            
            ser.close()
            print(f"[Serial] Monitored {len(self.lines)} lines", file=sys.stderr)
            
        except serial.SerialException as e:
            self.capture_error = e
            print(f"[Serial] ERROR: {e}", file=sys.stderr)
        except Exception as e:
            self.capture_error = e
            print(f"[Serial] ERROR: {e}", file=sys.stderr)
    
    def _analyze_line(self, line: str, timestamp: float):
        """Analyze line for WS connection/disconnect events"""
        if "[WS] Connected" in line or "Connected to server" in line:
            self.connect_times.append(timestamp)
        elif DISCONNECT_PATTERN.search(line):
            self.disconnect_times.append(timestamp)
    
    def start(self):
        """Start serial monitoring in background"""
        self.thread = threading.Thread(target=self._monitor_thread, daemon=False)
        self.thread.start()
        time.sleep(1)  # Let serial port open
    
    def stop(self):
        """Stop serial monitoring"""
        self.running = False
        if self.thread:
            self.thread.join(timeout=5)
    
    def get_lines(self) -> List[str]:
        """Get monitored lines"""
        return self.lines
    
    def check_failures(self) -> List[str]:
        """Check for failure signatures in monitored logs"""
        failures = []
        for line in self.lines:
            for sig in FAILURE_SIGNATURES:
                if sig in line:
                    failures.append(f"FAILURE: {sig} found in: {line[:100]}")
        return failures
    
    def check_rapid_reconnects(self, threshold_seconds: float = 5.0) -> List[str]:
        """Check for rapid disconnect/reconnect cycles (connection instability)"""
        issues = []
        
        # Check for rapid disconnects (multiple disconnects within threshold)
        if len(self.disconnect_times) >= 3:
            recent_disconnects = list(self.disconnect_times)[-3:]
            for i in range(len(recent_disconnects) - 1):
                time_diff = recent_disconnects[i+1] - recent_disconnects[i]
                if time_diff < threshold_seconds:
                    issues.append(
                        f"RAPID_RECONNECT: Disconnects within {time_diff:.1f}s "
                        f"(threshold: {threshold_seconds}s)"
                    )
        
        # Check for connect/disconnect churn
        if len(self.connect_times) >= 2 and len(self.disconnect_times) >= 2:
            # If we have multiple connects and disconnects, check if they're alternating rapidly
            all_events = sorted(
                [(t, 'connect') for t in self.connect_times] +
                [(t, 'disconnect') for t in self.disconnect_times]
            )[-6:]  # Last 6 events
            
            if len(all_events) >= 4:
                # Check for pattern: connect -> disconnect -> connect -> disconnect
                for i in range(len(all_events) - 3):
                    e1, e2, e3, e4 = all_events[i:i+4]
                    if (e1[1] == 'connect' and e2[1] == 'disconnect' and
                        e3[1] == 'connect' and e4[1] == 'disconnect'):
                        time_span = e4[0] - e1[0]
                        if time_span < threshold_seconds * 2:
                            issues.append(
                                f"CONNECTION_CHURN: Connect/disconnect cycle in {time_span:.1f}s"
                            )
        
        return issues
    
    def get_connection_stats(self) -> Dict:
        """Get connection statistics"""
        return {
            "total_connects": len(self.connect_times),
            "total_disconnects": len(self.disconnect_times),
            "connect_times": list(self.connect_times),
            "disconnect_times": list(self.disconnect_times),
        }

def main():
    parser = argparse.ArgumentParser(
        description="Hardware WS health check for Tab5 encoder device",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Monitor Tab5 WS connection for 60 seconds
  python3 ws_hardware_check.py

  # Monitor for longer duration
  python3 ws_hardware_check.py --duration 120

  # Save logs to file
  python3 ws_hardware_check.py --output tab5_ws_log.txt
        """
    )
    parser.add_argument(
        "--serial-port",
        default="/dev/cu.usbmodem101",
        help="Serial port for Tab5 device (default: /dev/cu.usbmodem101)"
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=60.0,
        help="Monitoring duration in seconds (default: 60.0)"
    )
    parser.add_argument(
        "--output",
        help="Output file for serial logs (default: stdout)"
    )
    parser.add_argument(
        "--reconnect-threshold",
        type=float,
        default=5.0,
        help="Threshold in seconds for rapid reconnect detection (default: 5.0)"
    )
    
    args = parser.parse_args()
    
    # Start serial monitoring
    monitor = SerialMonitor(args.serial_port, args.duration)
    monitor.start()
    
    print(f"[Monitor] Monitoring Tab5 WS connection for {args.duration}s...", file=sys.stderr)
    print(f"[Monitor] Watching for connection stability and regression indicators", file=sys.stderr)
    
    # Let monitoring run
    time.sleep(args.duration)
    monitor.stop()
    
    # Analyze results
    print("\n" + "="*70, file=sys.stderr)
    print("TAB5 WS CONNECTION HEALTH CHECK RESULTS", file=sys.stderr)
    print("="*70, file=sys.stderr)
    
    all_passed = True
    
    # Check for failure signatures
    failures = monitor.check_failures()
    if failures:
        print("\n[FAILURES] Regression indicators found:", file=sys.stderr)
        for failure in failures:
            print(f"  ✗ {failure}", file=sys.stderr)
        all_passed = False
    else:
        print("\n[PASS] No regression indicators in serial logs", file=sys.stderr)
    
    # Check for rapid reconnects
    reconnect_issues = monitor.check_rapid_reconnects(args.reconnect_threshold)
    if reconnect_issues:
        print("\n[FAILURES] Connection instability detected:", file=sys.stderr)
        for issue in reconnect_issues:
            print(f"  ✗ {issue}", file=sys.stderr)
        all_passed = False
    else:
        print("\n[PASS] No rapid reconnect cycles detected", file=sys.stderr)
    
    # Check connection statistics
    stats = monitor.get_connection_stats()
    print("\n[CONNECTION STATISTICS]", file=sys.stderr)
    print(f"  Total connects: {stats['total_connects']}", file=sys.stderr)
    print(f"  Total disconnects: {stats['total_disconnects']}", file=sys.stderr)
    
    if stats['total_connects'] == 0:
        print("  ⚠ WARNING: No WS connections detected during monitoring period", file=sys.stderr)
        all_passed = False
    elif stats['total_connects'] == 1 and stats['total_disconnects'] == 0:
        print("  ✓ Stable connection (connected once, no disconnects)", file=sys.stderr)
    elif stats['total_disconnects'] > stats['total_connects']:
        print("  ⚠ WARNING: More disconnects than connects (possible connection issues)", file=sys.stderr)
        all_passed = False
    
    # Check for success signatures
    lines = monitor.get_lines()
    success_count = 0
    for line in lines:
        for sig in SUCCESS_SIGNATURES:
            if sig in line:
                success_count += 1
                break
    
    if success_count > 0:
        print(f"\n[PASS] Found {success_count} success indicators in serial logs", file=sys.stderr)
    else:
        print(f"\n[WARNING] No success indicators found in serial logs", file=sys.stderr)
        if stats['total_connects'] == 0:
            all_passed = False
    
    # Output serial logs if requested
    if args.output:
        with open(args.output, 'w') as f:
            for line in lines:
                f.write(line + '\n')
        print(f"\n[INFO] Serial logs written to {args.output}", file=sys.stderr)
    
    # Final summary
    print("\n" + "="*70, file=sys.stderr)
    if all_passed and success_count > 0:
        print("OVERALL: PASS - Tab5 WS connection is stable", file=sys.stderr)
        sys.exit(0)
    else:
        print("OVERALL: FAIL - Issues detected with Tab5 WS connection", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
