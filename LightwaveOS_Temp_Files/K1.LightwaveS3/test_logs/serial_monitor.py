#!/usr/bin/env python3
"""
Serial Monitor for ESP32-S3 Device Validation
Captures boot logs and runtime diagnostics for Phase 0-5 validation
"""

import serial
import sys
import time
from datetime import datetime

def monitor_device(port, baudrate=115200, duration=60):
    """
    Monitor serial port for specified duration

    Args:
        port: Serial port path (e.g., /dev/cu.usbmodem101)
        baudrate: Serial baud rate (default 115200)
        duration: Monitoring duration in seconds (default 60)
    """
    try:
        # Open serial connection
        ser = serial.Serial(port, baudrate, timeout=1)
        print(f"[{datetime.now().isoformat()}] Connected to {port} at {baudrate} baud", file=sys.stderr)
        print(f"[{datetime.now().isoformat()}] Monitoring for {duration} seconds...", file=sys.stderr)

        start_time = time.time()
        line_count = 0

        # Monitor loop
        while (time.time() - start_time) < duration:
            if ser.in_waiting > 0:
                try:
                    line = ser.readline().decode('utf-8', errors='replace').rstrip()
                    if line:
                        timestamp = datetime.now().isoformat()
                        print(f"[{timestamp}] {line}")
                        sys.stdout.flush()
                        line_count += 1
                except UnicodeDecodeError:
                    # Skip lines that can't be decoded
                    pass
            else:
                # Small delay to prevent CPU spinning
                time.sleep(0.01)

        # Close connection
        ser.close()
        print(f"\n[{datetime.now().isoformat()}] Monitoring complete. Captured {line_count} lines.", file=sys.stderr)

    except serial.SerialException as e:
        print(f"[{datetime.now().isoformat()}] Serial error: {e}", file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        print(f"\n[{datetime.now().isoformat()}] Interrupted by user", file=sys.stderr)
        if 'ser' in locals() and ser.is_open:
            ser.close()
        sys.exit(0)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: serial_monitor.py <port> [baudrate] [duration]", file=sys.stderr)
        print("Example: serial_monitor.py /dev/cu.usbmodem101 115200 60", file=sys.stderr)
        sys.exit(1)

    port = sys.argv[1]
    baudrate = int(sys.argv[2]) if len(sys.argv) > 2 else 115200
    duration = int(sys.argv[3]) if len(sys.argv) > 3 else 60

    monitor_device(port, baudrate, duration)
