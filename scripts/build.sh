#!/bin/bash

# LightwaveOS Build Script
# Compiles the project without uploading

echo "LightwaveOS Build Script"
echo "========================="
echo ""

# Clean build
if [ "$1" == "clean" ]; then
    echo "Performing clean build..."
    cd firmware/v2 && pio run -t clean
fi

# Compile
echo "Compiling project..."
cd firmware/v2 && pio run

# Show build size if successful
if [ $? -eq 0 ]; then
    echo ""
    echo "Build successful!"
    echo ""
    echo "Memory usage:"
    cd firmware/v2 && pio run -t size
else
    echo ""
    echo "Build failed!"
    exit 1
fi