#!/bin/bash
# Verify build environment and prove ESP-IDF v5.5.2 is being used

set -e

echo "=== Build Environment Verification ==="
echo ""

# Check IDF_PATH
if [ -z "$IDF_PATH" ]; then
    echo "❌ FAIL: IDF_PATH not set"
    exit 1
fi
echo "✓ IDF_PATH: $IDF_PATH"

# Check IDF version
IDF_VERSION=$(cd "$IDF_PATH" && git describe --tags --exact-match 2>/dev/null || git describe --tags 2>/dev/null || echo "unknown")
echo "✓ Detected IDF version: $IDF_VERSION"

if [[ "$IDF_VERSION" != *"5.5.2"* ]] && [[ "$IDF_VERSION" != *"v5.5.2"* ]]; then
    echo "❌ FAIL: Wrong ESP-IDF version (required: v5.5.2)"
    exit 1
fi
echo "✓ Version check passed"

# Check esp-dsp
if [ -d "managed_components/espressif__esp-dsp" ]; then
    echo "✓ esp-dsp component found"
else
    echo "⚠ WARNING: esp-dsp not found (run: idf.py add-dependency \"espressif/esp-dsp\")"
fi

# Check build artifacts
if [ -d "build" ]; then
    echo "✓ Build directory exists"
    
    # Check bootloader version
    if [ -f "build/bootloader/bootloader.bin" ]; then
        echo "✓ Bootloader built"
    fi
    
    # Check app version (would need to parse ELF, skip for now)
else
    echo "⚠ No build directory (run build first)"
fi

echo ""
echo "=== Verification Complete ==="
echo "Next: Build and flash, then verify boot log shows v5.5.2"
