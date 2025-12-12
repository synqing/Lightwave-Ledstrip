#!/usr/bin/env python3
import serial
import time
import sys

port = '/dev/cu.usbmodem1401'
baud = 115200

print(f"Monitoring DC offset calibration on {port}...")
print("=" * 60)

try:
    with serial.Serial(port, baud, timeout=1) as ser:
        time.sleep(2)  # Wait for boot
        ser.reset_input_buffer()
        
        start_time = time.time()
        dc_offset_seen = False
        calibration_completed = False
        
        while True:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    timestamp = f"{time.time() - start_time:6.2f}s"
                    print(f"{timestamp}: {line}")
                    
                    # Track DC calibration status
                    if 'DC_cal' in line or 'offset' in line.lower() or 'calibrat' in line.lower():
                        print(f">>> DC CALIBRATION: {line}")
                        if 'YES' in line and 'DC_cal' in line:
                            calibration_completed = True
                            print(">>> CALIBRATION COMPLETED!")
                    
                    # Check for audio data
                    if 'raw_min' in line and 'raw_max' in line:
                        if 'raw_min=-' in line and 'raw_max=-' not in line:
                            print(">>> GOOD: Getting negative AND positive values - full dynamic range!")
                        elif 'raw_min=0' in line or ('raw_min=' in line and 'raw_max=' in line and '-' not in line):
                            print(">>> WARNING: Only positive values - DC offset may be too large!")
                    
except KeyboardInterrupt:
    print("\nMonitoring stopped.")
except Exception as e:
    print(f"Error: {e}")