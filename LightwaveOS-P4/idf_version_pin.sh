#!/bin/bash
# ESP-IDF Version Pinning Script for p4_audio_producer
# This script ensures ESP-IDF v5.5.2 is used and fails if wrong version detected

set -e

REQUIRED_IDF_VERSION="v5.5.2"
REQUIRED_IDF_COMMIT="v5.5.2"  # Use tag or specific commit

if [ -z "$IDF_PATH" ]; then
    echo "ERROR: IDF_PATH not set. Set it to ESP-IDF v5.5.2 installation."
    echo "Example: export IDF_PATH=\$HOME/esp/esp-idf"
    exit 1
fi

if [ ! -d "$IDF_PATH" ]; then
    echo "ERROR: IDF_PATH=$IDF_PATH does not exist"
    exit 1
fi

# Check ESP-IDF version
IDF_VERSION=$(cd "$IDF_PATH" && git describe --tags --exact-match 2>/dev/null || git describe --tags 2>/dev/null || echo "unknown")

if [[ "$IDF_VERSION" != *"5.5.2"* ]] && [[ "$IDF_VERSION" != *"v5.5.2"* ]]; then
    echo "ERROR: Wrong ESP-IDF version detected: $IDF_VERSION"
    echo "Required: $REQUIRED_IDF_VERSION"
    echo "IDF_PATH: $IDF_PATH"
    echo ""
    echo "To fix:"
    echo "  cd $IDF_PATH"
    echo "  git checkout $REQUIRED_IDF_VERSION"
    echo "  git submodule update --init --recursive"
    exit 1
fi

echo "ESP-IDF version OK: $IDF_VERSION"
echo "IDF_PATH: $IDF_PATH"

# Source ESP-IDF environment
. "$IDF_PATH/export.sh"

# Verify version after sourcing
ACTUAL_VERSION=$(python "$IDF_PATH/tools/idf.py" --version 2>/dev/null || echo "unknown")
echo "idf.py reports: $ACTUAL_VERSION"
