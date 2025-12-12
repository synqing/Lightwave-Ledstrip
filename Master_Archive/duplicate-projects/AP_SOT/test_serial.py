#!/usr/bin/env python3
import serial
import time
import sys

# Try multiple times to connect
for attempt in range(5):
    try:
        print(f"Attempt {attempt + 1} to connect...")
        ser = serial.Serial('/dev/cu.usbmodem1401', 115200, timeout=1)
        ser.dtr = True
        time.sleep(0.5)
        
        print("Connected! Reading data...")
        empty_reads = 0
        
        while empty_reads < 10:
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                print(data.decode('utf-8', errors='ignore'), end='')
                empty_reads = 0
            else:
                empty_reads += 1
                time.sleep(0.1)
        
        ser.close()
        break
        
    except serial.SerialException as e:
        print(f"Connection failed: {e}")
        time.sleep(1)

print("\nTest complete")