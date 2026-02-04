#!/bin/bash
# Flash and monitor script for LightwaveOS P4 bring-up

set -e

PORT="${P4_PORT:-/dev/tty.usbmodem5AAF2781791}"
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [ -n "$LW_PROJECT_DIR" ]; then
    PROJECT_DIR="$LW_PROJECT_DIR"
fi
cd "$PROJECT_DIR"

# Auto-detect IDF_PATH if not set
if [ -z "$IDF_PATH" ] || [ "$IDF_PATH" = "/path/to/esp-idf-v5.5.2" ]; then
    if [ -d "$HOME/esp/esp-idf-v5.5.2" ]; then
        export IDF_PATH="$HOME/esp/esp-idf-v5.5.2"
    else
        echo "ERROR: IDF_PATH not set and v5.5.2 not found at ~/esp/esp-idf-v5.5.2"
        exit 1
    fi
fi

PYTHON_ENV_V55=$(ls -d $HOME/.espressif/python_env/idf5.5* 2>/dev/null | head -1)
if [ -z "$PYTHON_ENV_V55" ]; then
    echo "ERROR: No ESP-IDF v5.5.x Python environment found. Run $IDF_PATH/install.sh first."
    exit 1
fi

PYTHON="$PYTHON_ENV_V55/bin/python"

BIN_NAME="lightwaveos_p4.bin"
BIN_PATH="$PROJECT_DIR/build/$BIN_NAME"

if [ ! -f "$BIN_PATH" ]; then
    echo "ERROR: Binary not found: $BIN_PATH"
    echo "Run build first: firmware/v2/p4_build.sh"
    exit 1
fi

echo "=== LightwaveOS P4 - Flash and Monitor ==="
echo "Project: $PROJECT_DIR"
echo "Port: $PORT"
echo "Binary: $BIN_PATH"
echo ""

echo "Step 1: Put device into bootloader mode"
echo "  - Press and HOLD the BOOT button"
echo "  - Press and RELEASE the RESET button (while holding BOOT)"
echo "  - RELEASE the BOOT button"
echo ""
echo "Press ENTER when ready to flash..."
read -r

echo ""
echo "Step 2: Flashing firmware..."
$PYTHON "$IDF_PATH/components/esptool_py/esptool/esptool.py" \
  --chip esp32p4 \
  --port "$PORT" \
  --baud 115200 \
  write_flash \
  0x20000 "$BIN_PATH"

echo ""
echo "✓ Flash successful!"
echo ""
echo "Step 3: Monitoring serial output (Ctrl+C to exit)..."
echo ""

$PYTHON -c "
import serial
import sys
import time
try:
    ser = serial.Serial('$PORT', 115200, timeout=1)
    print('Connected. Waiting for output...\n')
    while True:
        if ser.in_waiting:
            data = ser.read(ser.in_waiting)
            sys.stdout.write(data.decode('utf-8', errors='replace'))
            sys.stdout.flush()
        time.sleep(0.1)
except KeyboardInterrupt:
    print('\\n\\nMonitoring stopped.')
    ser.close()
except Exception as e:
    print(f'Error: {e}')
    sys.exit(1)
"
