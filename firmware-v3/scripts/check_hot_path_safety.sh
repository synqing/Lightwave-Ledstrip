#!/usr/bin/env bash
# check_hot_path_safety.sh — CI guardrail for hot-path safety violations.
#
# Fails if dangerous patterns appear in render/network/LED/audio broadcast
# code paths. Run from firmware-v3/ root.
#
# Usage: ./scripts/check_hot_path_safety.sh
# Exit code: 0 = clean, 1 = violations found

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FW_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
VIOLATIONS=0

red()   { printf '\033[1;31m%s\033[0m\n' "$*"; }
green() { printf '\033[1;32m%s\033[0m\n' "$*"; }
warn()  { printf '\033[1;33m%s\033[0m\n' "$*"; }

# ── Check 1: Expensive heap introspection in hot paths ──────────────────────
# heap_caps_get_largest_free_block() traverses the heap free list under mutex.
# Calling it in WebServer::update(), render loops, or broadcast paths causes
# cross-core mutex contention with the Core 1 renderer.
HOT_PATH_FILES=(
    "$FW_ROOT/src/network/WebServer.cpp"
    "$FW_ROOT/src/core/actors/RendererActor.cpp"
    "$FW_ROOT/src/hal/esp32s3/LedDriver_S3.cpp"
)

for f in "${HOT_PATH_FILES[@]}"; do
    if [ -f "$f" ]; then
        if grep -n 'heap_caps_get_largest_free_block\|heap_caps_walk\|multi_heap_walk' "$f" 2>/dev/null; then
            red "VIOLATION: Expensive heap introspection in hot path: $f"
            VIOLATIONS=$((VIOLATIONS + 1))
        fi
    fi
done

# ── Check 2: Large stack-local arrays in loop/WebServer update paths ────────
# CRGB arrays, ControlBusFrame, MusicalGridSnapshot on stack in update()/loop()
# risk stack overflow on loopTask (8KB budget, 93% used).
STACK_DANGER_FILES=(
    "$FW_ROOT/src/network/WebServer.cpp"
    "$FW_ROOT/src/main.cpp"
)

for f in "${STACK_DANGER_FILES[@]}"; do
    if [ -f "$f" ]; then
        # Match stack-local CRGB arrays (e.g. "CRGB leds[320]" or "CRGB buf[160]")
        if grep -nE 'CRGB\s+\w+\[' "$f" 2>/dev/null; then
            warn "WARNING: Stack-local CRGB array in $f (review for hot-path safety)"
        fi
    fi
done

# ── Check 3: Raw ControlBus reference access from network code ──────────────
# getControlBusRef()/getControlBusMut() bypass mutex protection.
# All network code should use snapshot accessors instead.
NETWORK_DIR="$FW_ROOT/src/network"
if [ -d "$NETWORK_DIR" ]; then
    MATCHES=$(grep -rn 'getControlBusRef\|getControlBusMut' "$NETWORK_DIR" 2>/dev/null || true)
    if [ -n "$MATCHES" ]; then
        red "VIOLATION: Raw ControlBus reference access in network code:"
        echo "$MATCHES"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi
fi

# ── Check 4: STA mode violations ────────────────────────────────────────────
# K1 is AP-only. WIFI_MODE_APSTA or WIFI_MODE_STA should not appear in
# production WiFiManager boot path (startSoftAP is the STA escape hatch).
WIFI_BOOT="$FW_ROOT/src/network/WiFiManager.cpp"
if [ -f "$WIFI_BOOT" ]; then
    # Check the begin() function (first 100 lines after "bool WiFiManager::begin")
    if grep -A100 'bool WiFiManager::begin' "$WIFI_BOOT" | grep -q 'WIFI_MODE_APSTA\|WIFI_MODE_STA'; then
        red "VIOLATION: STA mode in WiFiManager::begin() — K1 is AP-ONLY"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi
fi

# ── Summary ─────────────────────────────────────────────────────────────────
echo ""
if [ "$VIOLATIONS" -gt 0 ]; then
    red "FAILED: $VIOLATIONS hot-path safety violation(s) found"
    exit 1
else
    green "PASSED: No hot-path safety violations detected"
    exit 0
fi
