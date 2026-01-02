#!/bin/bash
# Clone reference repositories for pattern examination

cd "$(dirname "$0")/.." || exit 1

mkdir -p _ref
cd _ref || exit 1

echo "Cloning reference repositories..."

if [ ! -d "M5Unified" ]; then
    echo "Cloning M5Unified..."
    git clone https://github.com/m5stack/M5Unified.git
else
    echo "M5Unified already exists, skipping..."
fi

if [ ! -d "M5Tab5-UserDemo" ]; then
    echo "Cloning M5Tab5-UserDemo..."
    git clone https://github.com/m5stack/M5Tab5-UserDemo.git
else
    echo "M5Tab5-UserDemo already exists, skipping..."
fi

if [ ! -d "esp-bsp" ]; then
    echo "Cloning esp-bsp..."
    git clone https://github.com/espressif/esp-bsp.git
else
    echo "esp-bsp already exists, skipping..."
fi

if [ ! -d "xiaozhi-esp32" ]; then
    echo "Cloning xiaozhi-esp32..."
    git clone https://github.com/78/xiaozhi-esp32.git
else
    echo "xiaozhi-esp32 already exists, skipping..."
fi

echo "Done!"

