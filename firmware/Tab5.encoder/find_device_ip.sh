#!/bin/bash
# Find Tab5.encoder device IP by scanning local network and querying OTA endpoint

echo "Scanning for Tab5.encoder device..."
echo "This may take a minute..."

# Get local network interface and IP range
LOCAL_IP=$(ipconfig getifaddr en0 2>/dev/null || ipconfig getifaddr en1 2>/dev/null || echo "192.168.1.1")
NETWORK_BASE=$(echo $LOCAL_IP | cut -d. -f1-3)

echo "Local network: $NETWORK_BASE.0/24"
echo "Scanning for OTA endpoint..."

# Try common IP ranges
for i in {1..254}; do
    IP="${NETWORK_BASE}.${i}"
    
    # Try OTA version endpoint (quick check)
    RESPONSE=$(curl -s -m 1 "http://${IP}/api/v1/firmware/version" 2>/dev/null)
    
    if echo "$RESPONSE" | grep -q "version\|board\|M5Stack"; then
        echo ""
        echo "✓ Found device at: $IP"
        echo "$RESPONSE" | head -5
        exit 0
    fi
    
    # Progress indicator
    if [ $((i % 50)) -eq 0 ]; then
        echo -n "."
    fi
done

echo ""
echo "Device not found. Trying alternative method..."
echo "Please check the device display - the IP should be shown in the header."

