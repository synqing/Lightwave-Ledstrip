#!/bin/bash

# Light Crystals Monitor Script
# Monitors serial output from /dev/cu.usbmodem1101

echo "Light Crystals Serial Monitor"
echo "============================"
echo "Port: /dev/cu.usbmodem1101"
echo "Speed: 115200 baud"
echo ""

# Check if device is connected
if [ -e /dev/cu.usbmodem1101 ]; then
    echo "✓ Device found"
    echo ""
    echo "Starting monitor... (Press Ctrl+C to exit)"
    echo "========================================="
    echo ""
    pio device monitor -p /dev/cu.usbmodem1101 -b 115200
else
    echo "✗ Device NOT found at /dev/cu.usbmodem1101"
    echo ""
    echo "Available USB devices:"
    ls /dev/cu.usb* 2>/dev/null || echo "No USB devices found"
    echo ""
    echo "Make sure your ESP32 is connected and powered on."
    exit 1
fi