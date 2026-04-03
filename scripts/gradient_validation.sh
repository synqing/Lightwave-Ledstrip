#!/usr/bin/env bash
# ============================================================================
# Gradient Field Validation Script
# ============================================================================
# Runs 5 validation scenes on LGP Gradient Field (0x1F00) via REST API.
# Requires: connected to K1 AP (LightwaveOS-AP), K1 at 192.168.4.1
#
# Usage:
#   1. Connect Mac to LightwaveOS-AP WiFi
#   2. ./scripts/gradient_validation.sh
#
# Each scene sets parameters, waits for visual inspection, and logs results.
# ============================================================================

set -euo pipefail

K1="http://192.168.4.1"
EFFECT_ID=7936  # 0x1F00 in decimal
HOLD=6          # seconds to hold each scene for visual inspection
PASS_COUNT=0
FAIL_COUNT=0
RESULTS=()

# Colours
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# ── Helpers ─────────────────────────────────────────────────────────────────

check_connectivity() {
    printf "${CYAN}Checking K1 connectivity...${NC} "
    if ! curl -s --connect-timeout 3 "$K1/api/v1/device/status" > /dev/null 2>&1; then
        printf "${RED}FAIL${NC}\n"
        echo "Cannot reach K1 at $K1"
        echo "Make sure you're connected to the LightwaveOS-AP WiFi network."
        exit 1
    fi
    printf "${GREEN}OK${NC}\n"
}

set_effect() {
    curl -s -X POST "$K1/api/v1/effects/current" \
        -H "Content-Type: application/json" \
        -d "{\"effectId\": $EFFECT_ID}" > /dev/null 2>&1
}

set_params() {
    local json="$1"
    curl -s -X POST "$K1/api/v1/effects/parameters" \
        -H "Content-Type: application/json" \
        -d "{\"effectId\": $EFFECT_ID, \"parameters\": $json}" > /dev/null 2>&1
}

set_brightness() {
    curl -s -X POST "$K1/api/v1/parameters" \
        -H "Content-Type: application/json" \
        -d "{\"brightness\": $1}" > /dev/null 2>&1
}

get_params() {
    curl -s "$K1/api/v1/effects/parameters?effectId=$EFFECT_ID" 2>/dev/null
}

reset_defaults() {
    set_params '{"basis": 0, "repeatMode": 0, "interpolation": 1, "spread": 1.0, "phaseOffset": 0.0, "edgeAsymmetry": 0.0}'
    set_brightness 128
}

run_scene() {
    local num="$1"
    local name="$2"
    local params="$3"
    local expected="$4"
    local watch_for="$5"

    echo ""
    printf "${BOLD}═══════════════════════════════════════════════════════════${NC}\n"
    printf "${BOLD}Scene $num: $name${NC}\n"
    printf "${BOLD}═══════════════════════════════════════════════════════════${NC}\n"
    echo ""
    printf "  ${CYAN}Parameters:${NC} $params\n"
    printf "  ${CYAN}Expected:${NC}   $expected\n"
    printf "  ${YELLOW}Watch for:${NC}  $watch_for\n"
    echo ""

    # Apply parameters
    set_params "$params"
    sleep 0.5

    # Verify parameters were applied
    printf "  ${CYAN}Verifying params...${NC} "
    local response
    response=$(get_params)
    if echo "$response" | grep -q "basis"; then
        printf "${GREEN}OK${NC}\n"
    else
        printf "${YELLOW}Could not verify (non-blocking)${NC}\n"
    fi

    # Hold for visual inspection
    printf "\n  ${BOLD}>>> LOOK AT THE K1 NOW <<<${NC}\n"
    printf "  Holding for ${HOLD}s..."

    for i in $(seq $HOLD -1 1); do
        printf " $i"
        sleep 1
    done
    echo ""

    # Ask for result
    echo ""
    printf "  ${BOLD}Result? ${NC}[${GREEN}p${NC}]ass / [${RED}f${NC}]ail / [${YELLOW}s${NC}]kip: "
    read -r -n 1 result
    echo ""

    case "$result" in
        p|P)
            PASS_COUNT=$((PASS_COUNT + 1))
            RESULTS+=("Scene $num ($name): ${GREEN}PASS${NC}")
            printf "  ${GREEN}✓ PASS${NC}\n"
            ;;
        f|F)
            FAIL_COUNT=$((FAIL_COUNT + 1))
            printf "  Describe the failure: "
            read -r failure_desc
            RESULTS+=("Scene $num ($name): ${RED}FAIL${NC} — $failure_desc")
            printf "  ${RED}✗ FAIL${NC}\n"
            ;;
        *)
            RESULTS+=("Scene $num ($name): ${YELLOW}SKIP${NC}")
            printf "  ${YELLOW}— SKIP${NC}\n"
            ;;
    esac
}

# ── Main ────────────────────────────────────────────────────────────────────

echo ""
printf "${BOLD}╔═══════════════════════════════════════════════════════════╗${NC}\n"
printf "${BOLD}║     LGP Gradient Field (0x1F00) Validation Suite         ║${NC}\n"
printf "${BOLD}╚═══════════════════════════════════════════════════════════╝${NC}\n"
echo ""

check_connectivity

# Switch to gradient field effect
printf "${CYAN}Switching to LGP Gradient Field (0x1F00)...${NC} "
set_effect
sleep 1
printf "${GREEN}OK${NC}\n"

# Reset to defaults
printf "${CYAN}Resetting parameters to defaults...${NC} "
reset_defaults
sleep 0.5
printf "${GREEN}OK${NC}\n"

# Set brightness to a good viewing level
set_brightness 180

# ── Scene 1: Centre-out two-colour ramp ────────────────────────────────────

run_scene 1 \
    "Centre-out clamped ramp" \
    '{"basis": 0, "repeatMode": 0, "interpolation": 1, "spread": 1.0, "phaseOffset": 0.0, "edgeAsymmetry": 0.0}' \
    "Symmetric gradient radiating from centre pair outward on both strips" \
    "Banding, asymmetric centre pair, both strips identical (edgeAsymmetry=0)"

# ── Scene 2: Repeating hard-stop rings ─────────────────────────────────────

run_scene 2 \
    "Repeating hard-stop rings" \
    '{"basis": 0, "repeatMode": 1, "interpolation": 2, "spread": 2.0, "phaseOffset": 0.0, "edgeAsymmetry": 0.0}' \
    "Discrete concentric colour bands with sharp edges, no bleeding" \
    "Band edge sharpness, ring spacing uniformity, colour bleeding"

# ── Scene 3: Signed asymmetric field ──────────────────────────────────────

run_scene 3 \
    "Signed asymmetric field" \
    '{"basis": 2, "repeatMode": 0, "interpolation": 1, "spread": 1.0, "phaseOffset": 0.0, "edgeAsymmetry": 0.8}' \
    "Left half of strip visually distinct from right half" \
    "Centre transition zone, colour shift direction, left/right differentiation"

# ── Scene 4: Dual-edge complementary split ────────────────────────────────

run_scene 4 \
    "Dual-edge complementary split" \
    '{"basis": 0, "repeatMode": 0, "interpolation": 1, "spread": 1.0, "phaseOffset": 0.0, "edgeAsymmetry": 1.0}' \
    "Edge A and Edge B show completely different colour ranges at same position" \
    "Both strips should have full colour range, not just brightness difference"

# ── Scene 5: Low-brightness stability ─────────────────────────────────────

set_brightness 15
run_scene 5 \
    "Low-brightness stability" \
    '{"basis": 0, "repeatMode": 0, "interpolation": 1, "spread": 1.0, "phaseOffset": 0.0, "edgeAsymmetry": 0.0}' \
    "Stable gradient at very low brightness, no flickering" \
    "Flickering, banding from rounding, colour collapse to black"

# Restore brightness
set_brightness 128

# ── Report ──────────────────────────────────────────────────────────────────

echo ""
printf "${BOLD}═══════════════════════════════════════════════════════════${NC}\n"
printf "${BOLD}                    VALIDATION REPORT${NC}\n"
printf "${BOLD}═══════════════════════════════════════════════════════════${NC}\n"
echo ""

for r in "${RESULTS[@]}"; do
    printf "  $r\n"
done

echo ""
printf "  ${BOLD}Total:${NC} ${GREEN}$PASS_COUNT pass${NC}, ${RED}$FAIL_COUNT fail${NC}, $((5 - PASS_COUNT - FAIL_COUNT)) skip\n"
echo ""

if [ $FAIL_COUNT -eq 0 ] && [ $PASS_COUNT -eq 5 ]; then
    printf "  ${GREEN}${BOLD}ALL SCENES PASSED${NC}\n"
elif [ $FAIL_COUNT -gt 0 ]; then
    printf "  ${RED}${BOLD}FAILURES DETECTED — review before shipping${NC}\n"
else
    printf "  ${YELLOW}${BOLD}INCOMPLETE — rerun skipped scenes${NC}\n"
fi

echo ""

# Reset to defaults on exit
reset_defaults
printf "${CYAN}Parameters reset to defaults.${NC}\n"
echo ""
