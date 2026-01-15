#!/usr/bin/env python3
"""Quick test script to verify serial capture integration."""
import serial
import time
import sys

port = '/dev/cu.usbmodem212401'
baud = 115200

print(f"Connecting to {port}...")
ser = serial.Serial(port, baud, timeout=2)
time.sleep(2)

# Flush any startup messages
ser.reset_input_buffer()

# Test commands
commands = [
    ("capture status", "Check initial state"),
    ("capture on 2", "Enable TAP_B capture"),
    ("capture status", "Verify enabled"),
    ("capture send 1", "Send single frame"),
]

for cmd, desc in commands:
    print(f"\n[TEST] {desc}: '{cmd}'")
    ser.write(f"{cmd}\n".encode())
    time.sleep(0.5)
    
    # Read response
    while ser.in_waiting:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if line:
            print(f"  {line}")

print("\n[TEST] Disabling capture")
ser.write(b"capture off\n")
time.sleep(0.3)

ser.close()
print("\n✓ Quick test complete")
