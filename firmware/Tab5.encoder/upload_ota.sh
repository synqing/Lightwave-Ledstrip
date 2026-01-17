#!/bin/bash
# OTA Firmware Upload Script for Tab5.encoder
# Usage: ./upload_ota.sh <device-ip>
# Example: ./upload_ota.sh 192.168.1.100

set -e

if [ $# -eq 0 ]; then
    echo "Usage: $0 <device-ip>"
    echo "Example: $0 192.168.1.100"
    exit 1
fi

DEVICE_IP="$1"
FIRMWARE_BIN=".pio/build/tab5/firmware.bin"
OTA_TOKEN="LW-OTA-2024-SecureUpdate"
OTA_URL="http://${DEVICE_IP}/api/v1/firmware/update"

if [ ! -f "$FIRMWARE_BIN" ]; then
    echo "Error: Firmware binary not found at $FIRMWARE_BIN"
    echo "Please build the firmware first: pio run -e tab5"
    exit 1
fi

echo "Uploading firmware to ${DEVICE_IP}..."
echo "Firmware size: $(ls -lh "$FIRMWARE_BIN" | awk '{print $5}')"

# Upload via OTA (using multipart/form-data as expected by ESPAsyncWebServer)
curl -X POST \
    -H "X-OTA-Token: ${OTA_TOKEN}" \
    -F "firmware=@${FIRMWARE_BIN};type=application/octet-stream" \
    "${OTA_URL}" \
    --progress-bar \
    --write-out "\nHTTP Status: %{http_code}\n" \
    --max-time 120

echo ""
echo "OTA upload complete. Device should reboot automatically."

