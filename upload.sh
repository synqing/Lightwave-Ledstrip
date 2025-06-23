#!/bin/bash

# Light Crystals Upload Script
# Uses the correct USB port: /dev/cu.usbmodem1101

echo "Light Crystals Upload Script"
echo "=========================="
echo "Target Port: /dev/cu.usbmodem1101"
echo ""

# Check if device is connected
if [ -e /dev/cu.usbmodem1101 ]; then
    echo "✓ Device found at /dev/cu.usbmodem1101"
else
    echo "✗ Device NOT found at /dev/cu.usbmodem1101"
    echo "Available USB devices:"
    ls /dev/cu.usb* 2>/dev/null || echo "No USB devices found"
    exit 1
fi

# Compile and upload
echo ""
echo "Compiling and uploading..."
pio run -t upload

# Start monitor after upload
if [ $? -eq 0 ]; then
    echo ""
    echo "Upload successful! Starting monitor..."
    echo "Press Ctrl+C to exit monitor"
    echo ""
    pio device monitor
else
    echo "Upload failed!"
    exit 1
fi