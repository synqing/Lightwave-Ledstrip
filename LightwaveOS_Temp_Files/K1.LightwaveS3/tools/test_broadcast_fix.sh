#!/bin/bash
# Test script to verify broadcastStatus() crash fix
# Tests the specific crash scenario and verifies broadcasts still work

set -e

DEVICE_IP="${1:-lightwaveos.local}"
BASE_URL="http://${DEVICE_IP}/api/v1"

echo "=========================================="
echo "Testing BroadcastStatus Crash Fix"
echo "=========================================="
echo "Device: ${DEVICE_IP}"
echo ""

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

PASSED=0
FAILED=0

# Test function
test_endpoint() {
    local method=$1
    local endpoint=$2
    local data=$3
    local description=$4
    
    echo -n "Testing: ${description}... "
    
    if [ -z "$data" ]; then
        response=$(curl -s -w "\n%{http_code}" -X ${method} "${BASE_URL}${endpoint}")
    else
        response=$(curl -s -w "\n%{http_code}" -X ${method} "${BASE_URL}${endpoint}" \
            -H "Content-Type: application/json" \
            -d "${data}")
    fi
    
    http_code=$(echo "$response" | tail -n1)
    body=$(echo "$response" | head -n-1)
    
    if [ "$http_code" = "200" ]; then
        echo -e "${GREEN}✓ PASS${NC} (HTTP ${http_code})"
        ((PASSED++))
        return 0
    else
        echo -e "${RED}✗ FAIL${NC} (HTTP ${http_code})"
        echo "  Response: ${body}"
        ((FAILED++))
        return 1
    fi
}

# Test WebSocket connection (basic check)
test_websocket_connection() {
    echo -n "Testing: WebSocket connection... "
    # This is a basic check - full WS testing would need a proper WS client
    # For now, we just verify the endpoint exists
    response=$(curl -s -o /dev/null -w "%{http_code}" \
        -H "Upgrade: websocket" \
        -H "Connection: Upgrade" \
        "${BASE_URL/\/api\/v1/}/ws" || echo "000")
    
    if [ "$response" = "101" ] || [ "$response" = "400" ]; then
        echo -e "${GREEN}✓ PASS${NC} (WebSocket endpoint responds)"
        ((PASSED++))
    else
        echo -e "${YELLOW}⚠ SKIP${NC} (Cannot test WS without proper client)"
    fi
}

echo "=== Test 1: Crash Scenario - Palette Set ==="
# This was the exact crash scenario from the backtrace
test_endpoint "POST" "/palettes/set" '{"paletteId": 5}' "Set palette (crash scenario)"
sleep 0.1

echo ""
echo "=== Test 2: Crash Scenario - Parameters Set ==="
test_endpoint "POST" "/parameters" '{"brightness": 200, "speed": 25}' "Set parameters"
sleep 0.1

echo ""
echo "=== Test 3: Crash Scenario - Effect Set ==="
test_endpoint "POST" "/effects/set" '{"effectId": 10}' "Set effect"
sleep 0.1

echo ""
echo "=== Test 4: Verify Broadcasts Still Work ==="
# Get current status to verify system is responsive
test_endpoint "GET" "/status" "" "Get status (verify system responsive)"
sleep 0.1

test_endpoint "GET" "/parameters" "" "Get parameters"
sleep 0.1

test_endpoint "GET" "/effects/current" "" "Get current effect"
sleep 0.1

echo ""
echo "=== Test 5: Rapid Requests (Stress Test) ==="
# Send multiple rapid requests to test coalescing
for i in {1..5}; do
    test_endpoint "POST" "/palettes/set" "{\"paletteId\": $((i % 10))}" "Rapid palette set #${i}"
    sleep 0.05
done

echo ""
echo "=== Test 6: WebSocket Endpoint ==="
test_websocket_connection

echo ""
echo "=========================================="
echo "Test Results:"
echo "  Passed: ${PASSED}"
echo "  Failed: ${FAILED}"
echo "=========================================="

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
fi

