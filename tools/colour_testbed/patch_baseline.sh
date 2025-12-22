#!/bin/bash
# Patch baseline commit 9734cca with capture instrumentation only (no color correction)

set -e

BASELINE_DIR="baseline_9734cca"
if [ ! -d "$BASELINE_DIR" ]; then
    echo "Error: $BASELINE_DIR not found. Run: git archive 9734cca | tar -x -C baseline_9734cca"
    exit 1
fi

echo "Patching baseline with capture instrumentation..."

# Copy capture infrastructure from current to baseline
cp v2/src/core/actors/RendererActor.h "$BASELINE_DIR/v2/src/core/actors/RendererActor.h"
cp v2/src/core/actors/RendererActor.cpp "$BASELINE_DIR/v2/src/core/actors/RendererActor.cpp"

# Copy main.cpp (has capture commands + effect <id>)
cp v2/src/main.cpp "$BASELINE_DIR/v2/src/main.cpp"

# Now remove ColorCorrectionEngine call from baseline RendererActor::onTick()
# In baseline, Tap B should capture the same buffer as Tap A (no correction)
sed -i.bak 's/ColorCorrectionEngine::getInstance().processBuffer(m_leds, LedConfig::TOTAL_LEDS);/\/\/ No color correction in baseline - Tap B same as Tap A/' \
    "$BASELINE_DIR/v2/src/core/actors/RendererActor.cpp"

# Remove the ColorCorrectionEngine include from main.cpp (baseline doesn't have it)
sed -i.bak '/#include.*ColorCorrectionEngine/d' \
    "$BASELINE_DIR/v2/src/main.cpp"

# Remove ColorCorrectionEngine-related commands from main.cpp
sed -i.bak '/ColorCorrectionEngine::getInstance()/d' \
    "$BASELINE_DIR/v2/src/main.cpp"

echo "Baseline patched. Next steps:"
echo "1. cd $BASELINE_DIR/v2"
echo "2. pio run -e esp32dev -t upload --upload-port /dev/tty.usbmodem1101"
echo "3. cd ../.."
echo "4. python3 tools/colour_testbed/capture_test.py --output captures/baseline_gamut"

