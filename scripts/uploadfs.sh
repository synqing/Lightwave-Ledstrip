#!/bin/bash
# Upload LittleFS filesystem to ESP32-S3

PORT="/dev/tty.usbmodem21401"
echo "Uploading LittleFS filesystem to ESP32-S3..."
echo "Port: $PORT"
echo ""

# Check if device is connected
if [ ! -e "$PORT" ]; then
    echo "✗ Device NOT found at $PORT"
    echo "Available USB devices:"
    ls /dev/cu.usb* 2>/dev/null || echo "No USB devices found"
    exit 1
fi

echo "✓ Device found at $PORT"
echo ""

# Upload filesystem
cd firmware/v2 && pio run -e esp32dev_audio -t uploadfs --upload-port "$PORT"

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Filesystem uploaded successfully!"
    echo "You can now access the webapp at http://lightwaveos.local or http://<device-ip>"
else
    echo ""
    echo "✗ Filesystem upload failed!"
    exit 1
fi
