#!/bin/bash
# Clean all ESP-IDF v6.0-dev artifacts and traces from p4_audio_producer

set -e

echo "Cleaning all ESP-IDF v6.0-dev artifacts..."

cd "$(dirname "$0")"

# Remove build artifacts
rm -rf build
rm -rf sdkconfig
rm -rf sdkconfig.old
rm -rf .sdkconfig
rm -rf .sdkconfig.old

# Remove any cached CMake files
find . -name "CMakeCache.txt" -delete
find . -name "cmake_install.cmake" -delete
find . -type d -name "CMakeFiles" -exec rm -rf {} + 2>/dev/null || true

# Remove any Python cache
find . -type d -name "__pycache__" -exec rm -rf {} + 2>/dev/null || true
find . -name "*.pyc" -delete

echo "Cleanup complete. All build artifacts removed."
echo ""
echo "Next steps:"
echo "  1. Set IDF_PATH to ESP-IDF v5.5.2: export IDF_PATH=\$HOME/esp/esp-idf-v5.5.2"
echo "  2. Verify version: ./idf_version_pin.sh"
echo "  3. Source IDF: . \$IDF_PATH/export.sh"
echo "  4. Build: idf.py set-target esp32p4 && idf.py build"
