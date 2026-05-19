#!/usr/bin/env python3
"""Pyserial capture with timed command injection.
Usage: capture.py <port> <log> <total_seconds> <cmd> <inject_t1,t2,...>
Example: capture.py /dev/tty.usbmodem2101 /tmp/log 180 "dbg stack async_tcp" "5,30,60,120,170"
"""
import serial, sys, time

port = sys.argv[1]
log_path = sys.argv[2]
seconds = int(sys.argv[3])
cmd = sys.argv[4]
inject_times = sorted(int(x) for x in sys.argv[5].split(","))

ser = serial.Serial(port, 115200, timeout=0.1)
start = time.time()
deadline = start + seconds
injected = set()
with open(log_path, 'wb') as f:
    while time.time() < deadline:
        elapsed = time.time() - start
        for t in inject_times:
            if t not in injected and elapsed >= t:
                injected.add(t)
                ser.write((cmd + "\r\n").encode())
                f.write(f"--- INJECT t={t}s cmd={cmd}\n".encode())
                f.flush()
        data = ser.read(4096)
        if data:
            f.write(data)
            f.flush()
ser.close()
print(f"[capture] done — {log_path}")
