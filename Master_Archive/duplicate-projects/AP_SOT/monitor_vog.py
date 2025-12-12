#!/usr/bin/env python3
import serial
import time

# Configure serial connection
ser = serial.Serial('/dev/cu.usbmodem1401', 115200, timeout=1)
print("Connected to /dev/cu.usbmodem1401 at 115200 baud")
print("Monitoring VoG output...")
print("Press Ctrl+C to exit\n")

# Clear any existing data
ser.reset_input_buffer()

try:
    # Monitor for 30 seconds
    start_time = time.time()
    while time.time() - start_time < 30:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                # Highlight important lines
                if "VoG DEBUG" in line:
                    print(f">>> {line}")
                elif "VoG SPEAKS" in line:
                    print(f"*** {line} ***")
                elif "BEAT TIMEOUT" in line:
                    print(f"!!! {line}")
                elif "Beat:" in line and "VoG:" in line:
                    print(f"==> {line}")
                else:
                    print(line)
        time.sleep(0.001)
        
except KeyboardInterrupt:
    print("\nMonitoring stopped by user")
finally:
    ser.close()
    print("Serial connection closed")