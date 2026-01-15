#!/bin/bash
# LightwaveOS Domain Memory Harness - Boot Ritual
# This script verifies the project is in a healthy state before agent work.
# Exit codes: 0 = healthy, 1 = verification failed

# Note: NOT using set -e because we need to handle failures gracefully

echo "========================================"
echo "LightwaveOS Init - Boot Ritual"
echo "========================================"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Track overall status
PASS=0
FAIL=0

# Helper function
check_result() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}[PASS]${NC} $2"
        ((PASS++))
    else
        echo -e "${RED}[FAIL]${NC} $2"
        ((FAIL++))
    fi
}

# 1. Detect toolchain
echo "--- Toolchain Detection ---"
if command -v pio &> /dev/null; then
    PIO_VERSION=$(pio --version 2>&1 | head -1)
    echo -e "${GREEN}[OK]${NC} PlatformIO: $PIO_VERSION"
else
    echo -e "${RED}[MISSING]${NC} PlatformIO not found in PATH"
    echo "  Install: pip install platformio"
    exit 1
fi

if command -v git &> /dev/null; then
    GIT_VERSION=$(git --version)
    echo -e "${GREEN}[OK]${NC} $GIT_VERSION"
else
    echo -e "${RED}[MISSING]${NC} git not found"
    exit 1
fi

echo ""

# 2. Git status check
echo "--- Git Status ---"
BRANCH=$(git branch --show-current 2>/dev/null || echo "unknown")
echo "Branch: $BRANCH"

DIRTY_COUNT=$(git status --porcelain 2>/dev/null | wc -l | tr -d ' ')
if [ "$DIRTY_COUNT" -gt 0 ]; then
    echo -e "${YELLOW}[WARN]${NC} $DIRTY_COUNT uncommitted changes"
else
    echo -e "${GREEN}[OK]${NC} Working tree clean"
fi

echo ""

# 3. Build verification (audio environment for tempo tracking)
echo "--- Build Verification (esp32dev_audio) ---"
echo "Running: pio run -e esp32dev_audio -d firmware/v2"
if pio run -e esp32dev_audio -d firmware/v2 > /tmp/pio_build.log 2>&1; then
    check_result 0 "Build esp32dev_audio"
    # Extract memory usage (ignore grep exit code)
    grep -E "(RAM:|Flash:)" /tmp/pio_build.log | tail -2 || true
else
    check_result 1 "Build esp32dev_audio"
    echo "Build log tail:"
    tail -20 /tmp/pio_build.log
fi

echo ""

# 4. Summary
echo "========================================"
echo "Summary: $PASS passed, $FAIL failed"
echo "========================================"

if [ $FAIL -gt 0 ]; then
    echo -e "${RED}Verification FAILED${NC}"
    exit 1
else
    echo -e "${GREEN}Verification PASSED${NC}"
    exit 0
fi
