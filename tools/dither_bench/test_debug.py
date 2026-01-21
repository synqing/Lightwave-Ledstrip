#!/usr/bin/env python3
"""Debug script to see what's actually being received."""
import serial
import time

port = '/dev/cu.usbmodem212401'
baud = 115200

print(f"Connecting to {port}...")
ser = serial.Serial(port, baud, timeout=2)
time.sleep(2)
ser.reset_input_buffer()

# Test both variants
tests = [
    "capture send 1",
    "capture stream 1",
]

for cmd in tests:
    print(f"\nSending: '{cmd}'")
    ser.write(f"{cmd}\n".encode())
    time.sleep(0.3)
    
    while ser.in_waiting:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if line:
            print(f"  {line}")

ser.close()
