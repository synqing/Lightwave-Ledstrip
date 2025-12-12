#!/usr/bin/env python3
import serial
import time
import sys

port = '/dev/cu.usbmodem1401'
baud = 115200

print("Opening serial port...")
try:
    with serial.Serial(port, baud, timeout=0.1) as ser:
        print(f"Connected to {port}")
        print("Starting DC Calibration Test...")
        print("-" * 60)
        
        # Read continuously until test completes
        while True:
            if ser.in_waiting:
                data = ser.read(ser.in_waiting)
                text = data.decode('utf-8', errors='ignore')
                print(text, end='', flush=True)
                
                # Check if test is complete
                if "CALIBRATION TEST RESULTS" in text:
                    # Continue reading for a bit more to get full results
                    time.sleep(2)
                    while ser.in_waiting:
                        data = ser.read(ser.in_waiting)
                        print(data.decode('utf-8', errors='ignore'), end='', flush=True)
                    break
                    
except KeyboardInterrupt:
    print("\n\nTest interrupted by user")
except Exception as e:
    print(f"\nError: {e}")
    
print("\n\nTest complete!")