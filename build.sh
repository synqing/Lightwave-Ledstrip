#!/bin/bash

# Light Crystals Build Script
# Compiles the project without uploading

echo "Light Crystals Build Script"
echo "========================="
echo ""

# Clean build
if [ "$1" == "clean" ]; then
    echo "Performing clean build..."
    pio run -t clean
fi

# Compile
echo "Compiling project..."
pio run

# Show build size if successful
if [ $? -eq 0 ]; then
    echo ""
    echo "Build successful!"
    echo ""
    echo "Memory usage:"
    pio run -t size
else
    echo ""
    echo "Build failed!"
    exit 1
fi