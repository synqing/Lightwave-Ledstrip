#!/usr/bin/env python3
"""Minimal pyserial reader. Usage: capture.py <port> <log> <seconds>"""
import serial, sys, time

port, log_path, seconds = sys.argv[1], sys.argv[2], int(sys.argv[3])
ser = serial.Serial(port, 115200, timeout=1)
deadline = time.time() + seconds
with open(log_path, 'wb') as f:
    while time.time() < deadline:
        data = ser.read(4096)
        if data:
            f.write(data)
            f.flush()
ser.close()
print(f"[capture] done — {log_path}")
