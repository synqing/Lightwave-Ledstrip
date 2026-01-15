#!/usr/bin/env python3
"""
Capture device boot sequence by resetting ESP32-S3
"""

import serial
import sys
import time
from datetime import datetime

def capture_boot(port, baudrate=115200, duration=30):
    """
    Reset device and capture boot sequence
    
    Args:
        port: Serial port path
        baudrate: Serial baud rate
        duration: Capture duration in seconds
    """
    try:
        # Open serial connection
        ser = serial.Serial(port, baudrate, timeout=1)
        print(f"[{datetime.now().isoformat()}] Connected to {port}", file=sys.stderr)
        
        # Send reset command (DTR toggle typically resets ESP32)
        print(f"[{datetime.now().isoformat()}] Triggering device reset...", file=sys.stderr)
        ser.setDTR(False)
        time.sleep(0.1)
        ser.setDTR(True)
        time.sleep(0.5)
        
        # Clear any buffered data
        ser.reset_input_buffer()
        
        print(f"[{datetime.now().isoformat()}] Capturing boot logs for {duration} seconds...", file=sys.stderr)
        
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
                    pass
            else:
                time.sleep(0.01)
        
        # Close connection
        ser.close()
        print(f"\n[{datetime.now().isoformat()}] Boot capture complete. Captured {line_count} lines.", file=sys.stderr)
        
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
        print("Usage: capture_boot.py <port> [baudrate] [duration]", file=sys.stderr)
        sys.exit(1)
    
    port = sys.argv[1]
    baudrate = int(sys.argv[2]) if len(sys.argv) > 2 else 115200
    duration = int(sys.argv[3]) if len(sys.argv) > 3 else 30
    
    capture_boot(port, baudrate, duration)
