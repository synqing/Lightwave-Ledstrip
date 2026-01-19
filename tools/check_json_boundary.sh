#!/bin/bash
#
# @file check_json_boundary.sh
# @brief JSON boundary enforcement: ensures JSON key access only occurs in codec modules
#
# This script enforces the JSON codec boundary pattern by scanning source code
# for direct JSON key access (e.g., doc["key"], obj["x"]) outside approved codec paths.
#
# Allowed paths:
# - firmware/*/src/codec/** (all codec modules)
# - firmware/v2/src/network/RequestValidator.h (if retained as boundary tool)
#
# Usage:
#   ./tools/check_json_boundary.sh                    # Scan entire repo
#   TARGET_DIR=firmware/v2 ./tools/check_json_boundary.sh  # Scan specific directory
#   --baseline [file]                                 # Generate baseline of current violations
#   --check-baseline [file]                           # Compare against baseline (CI mode)
#
# Exit codes:
#   0 = No violations found (or no new violations when using --check-baseline)
#   1 = Violations found or script error
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

# Parse command-line arguments
BASELINE_MODE=false
CHECK_BASELINE_MODE=false
BASELINE_FILE=""
TARGET_DIR="${TARGET_DIR:-.}"

while [[ $# -gt 0 ]]; do
    case $1 in
        --baseline)
            BASELINE_MODE=true
            if [ -n "$2" ] && [[ ! "$2" =~ ^- ]]; then
                BASELINE_FILE="$2"
                shift
            fi
            shift
            ;;
        --check-baseline)
            CHECK_BASELINE_MODE=true
            if [ -n "$2" ] && [[ ! "$2" =~ ^- ]]; then
                BASELINE_FILE="$2"
                shift
            fi
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--baseline [file]] [--check-baseline [file]]"
            exit 1
            ;;
    esac
done

# Default baseline file if mode is set but file not specified
if [ "$BASELINE_MODE" = true ] && [ -z "$BASELINE_FILE" ]; then
    BASELINE_FILE="$REPO_ROOT/tools/json_boundary_baseline_firmware_v2.txt"
fi

if [ "$CHECK_BASELINE_MODE" = true ] && [ -z "$BASELINE_FILE" ]; then
    BASELINE_FILE="$REPO_ROOT/tools/json_boundary_baseline_firmware_v2.txt"
fi

# Determine search directory
if [ "$TARGET_DIR" != "." ]; then
    SEARCH_DIR="$REPO_ROOT/$TARGET_DIR"
    if [ ! -d "$SEARCH_DIR" ]; then
        echo "Error: TARGET_DIR '$TARGET_DIR' does not exist in repository"
        exit 1
    fi
else
    SEARCH_DIR="$REPO_ROOT"
fi

# Colours for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Colour

# Phase 1 Pattern: Detect request-side JSON reads only (not response writes)
# Matches read patterns: doc["key"].as<T>(), root["key"] | default, params["key"].is<T>()
# Matches: doc["key"].as<int>(), root["version"].is<const char*>(), params["value"] | 0
# Also matches: doc["key"].as<JsonObjectConst>(), root["effects"].as<JsonArrayConst>()
# Does NOT match: data["key"] = value (response encoding, checked in Phase 2)
JSON_READ_PATTERN='\b(doc|obj|root|params)\s*\[\s*"[^"]*"\s*\].*(\.as<|\.is<|\s*\||\.to<JsonObject|\.to<JsonArray)'

# Phase 2 Pattern: Detect response-side JSON writes
# Matches write patterns: data["key"] = value, obj["x"] = y, response["type"] = "error"
# Matches: data["zoneId"] = zoneId, obj["name"] = name, eventDoc["type"] = "zone.enabledChanged"
JSON_WRITE_PATTERN='\b(data|obj|response|eventDoc)\s*\[\s*"[^"]*"\s*\]\s*='

# Allowed paths (directories where JSON key access is permitted)
ALLOWED_PATHS=(
    "firmware/v2/src/codec"
    "firmware/Tab5.encoder/src/codec"
    # RequestValidator.h is allowed as it's a boundary tool (if kept)
    # Uncomment if RequestValidator is retained:
    # "firmware/v2/src/network/RequestValidator.h"
)

# File extensions to scan
EXTENSIONS="c|cpp|h|hpp|ino"

# Track violations
VIOLATIONS=0
FILES_WITH_VIOLATIONS=()
VIOLATION_LINES=()  # Store violation signatures for baseline comparison

echo "================================================================================"
echo "JSON Boundary Check: Enforcing codec pattern (Phase 1 & Phase 2)"
if [ "$TARGET_DIR" != "." ]; then
    echo "Target directory: $TARGET_DIR"
fi
if [ "$BASELINE_MODE" = true ]; then
    echo "Mode: GENERATE BASELINE"
    echo "Baseline file: $BASELINE_FILE"
elif [ "$CHECK_BASELINE_MODE" = true ]; then
    echo "Mode: CHECK AGAINST BASELINE"
    echo "Baseline file: $BASELINE_FILE"
fi
echo "================================================================================"
echo ""
echo "Phase 1: Scanning for request-side JSON read patterns outside codec modules..."
echo "Pattern: ${JSON_READ_PATTERN}"
echo ""
echo "Phase 2: Scanning for response-side JSON write patterns outside codec modules..."
echo "Pattern: ${JSON_WRITE_PATTERN}"
echo ""

# Function to check if a file path is allowed
is_allowed_path() {
    local filepath="$1"
    for allowed in "${ALLOWED_PATHS[@]}"; do
        # Check if file path starts with allowed path (handles subdirectories)
        # When TARGET_DIR is set, filepath is relative to TARGET_DIR, so check against both
        if [[ "$filepath" == *"$allowed"* ]] || [[ "$filepath" == "$allowed"* ]] || [[ *"$allowed"* == "$filepath" ]]; then
            return 0
        fi
        # Also check if allowed path is within TARGET_DIR
        if [ "$TARGET_DIR" != "." ] && [[ "$allowed" == "$TARGET_DIR"/* ]]; then
            local allowed_relative="${allowed#$TARGET_DIR/}"
            if [[ "$filepath" == *"$allowed_relative"* ]] || [[ "$filepath" == "$allowed_relative"* ]]; then
                return 0
            fi
        fi
    done
    return 1
}

# Function to normalize text for stable baseline signatures
# Trims leading/trailing whitespace and collapses whitespace runs to single spaces
normalize_text() {
    local text="$1"
    # Remove leading/trailing whitespace
    text="${text#"${text%%[![:space:]]*}"}"
    text="${text%"${text##*[![:space:]]}"}"
    # Collapse whitespace runs to single spaces
    echo "$text" | sed 's/[[:space:]]\+/ /g'
}

# Find all C/C++ source files (exclude library dependencies, build artifacts, and test files)
# Use SEARCH_DIR instead of . to support TARGET_DIR scoping
# Exclude test files: tests are allowed to touch JSON directly for validation
FILES=$(find "$SEARCH_DIR" -type f \( -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.ino" \) \
    ! -path "*/.git/*" \
    ! -path "*/.pio/*" \
    ! -path "*/node_modules/*" \
    ! -path "*/build/*" \
    ! -path "*/libdeps/*" \
    ! -path "*/examples/*" \
    ! -path "*/test_native/mocks/*" \
    ! -path "*/test/**" \
    ! -path "*/tests/**" \
    ! -path "*/data/WebServerAudioSynq.cpp" \
    | sort)

# Check each file
for file in $FILES; do
    # Convert absolute path to relative path from REPO_ROOT for comparison
    # When TARGET_DIR is set, paths from find are relative to TARGET_DIR
    if [ "$TARGET_DIR" != "." ]; then
        # File path is relative to SEARCH_DIR, convert to relative from REPO_ROOT
        file_relative="$TARGET_DIR/${file#$SEARCH_DIR/}"
        file_relative="${file_relative#./}"
    else
        file_relative="${file#$REPO_ROOT/}"
        file_relative="${file_relative#./}"
    fi
    
    # Skip if in allowed path
    if is_allowed_path "$file_relative"; then
        continue
    fi

    # Search for JSON read patterns (Phase 1: request parsing)
    VIOLATION_FOUND=false
    if grep -qE "$JSON_READ_PATTERN" "$file" 2>/dev/null; then
        # Extract matching lines (limit to first 3 for brevity)
        MATCHES=$(grep -nE "$JSON_READ_PATTERN" "$file" | head -3)

        if [ -n "$MATCHES" ]; then
            echo -e "${RED}PHASE 1 VIOLATION (request read):${NC} $file_relative"
            while IFS= read -r line; do
                [ -z "$line" ] && continue
                echo "  $line"
                # Extract line text (everything after line number and colon)
                line_text="${line#*:}"
                line_text=$(normalize_text "$line_text")
                # Store stable violation signature: file|P1|normalized_text
                VIOLATION_LINES+=("$file_relative|P1|$line_text")
            done <<< "$MATCHES"
            echo ""

            VIOLATIONS=$((VIOLATIONS + 1))
            FILES_WITH_VIOLATIONS+=("$file_relative")
            VIOLATION_FOUND=true
        fi
    fi
    
    # Search for JSON write patterns (Phase 2: response encoding)
    if grep -qE "$JSON_WRITE_PATTERN" "$file" 2>/dev/null; then
        # Extract matching lines (limit to first 3 for brevity)
        MATCHES=$(grep -nE "$JSON_WRITE_PATTERN" "$file" | head -3)

        if [ -n "$MATCHES" ]; then
            echo -e "${RED}PHASE 2 VIOLATION (response write):${NC} $file_relative"
            while IFS= read -r line; do
                [ -z "$line" ] && continue
                echo "  $line"
                # Extract line text (everything after line number and colon)
                line_text="${line#*:}"
                line_text=$(normalize_text "$line_text")
                # Store stable violation signature: file|P2|normalized_text
                VIOLATION_LINES+=("$file_relative|P2|$line_text")
            done <<< "$MATCHES"
            echo ""

            if [ "$VIOLATION_FOUND" = false ]; then
                VIOLATIONS=$((VIOLATIONS + 1))
                FILES_WITH_VIOLATIONS+=("$file_relative")
            fi
        fi
    fi
done

# Handle baseline modes
if [ "$BASELINE_MODE" = true ]; then
    # Generate baseline: write current violations to file
    echo "================================================================================"
    if [ ${#VIOLATION_LINES[@]} -eq 0 ]; then
        echo -e "${GREEN}SUCCESS:${NC} No violations found - baseline file will be empty"
        echo "" > "$BASELINE_FILE"
    else
        # Sort without -u to preserve duplicates (multiset behavior)
        # Each identical violation becomes a separate baseline entry
        printf '%s\n' "${VIOLATION_LINES[@]}" | sort > "$BASELINE_FILE"
        echo -e "${YELLOW}BASELINE GENERATED:${NC} $BASELINE_FILE"
        echo "  Contains ${#VIOLATION_LINES[@]} violation(s)"
        echo ""
        echo "Commit this file to track baseline violations."
        echo "Future runs with --check-baseline will fail only on NEW violations."
    fi
    exit 0
fi

if [ "$CHECK_BASELINE_MODE" = true ]; then
    # Check baseline: compare current violations against baseline
    if [ ! -f "$BASELINE_FILE" ]; then
        echo -e "${RED}ERROR:${NC} Baseline file not found: $BASELINE_FILE"
        echo "Generate baseline first with: $0 --baseline"
        exit 1
    fi
    
    # Generate sorted current violations for comparison
    CURRENT_VIOLATIONS=$(mktemp)
    printf '%s\n' "${VIOLATION_LINES[@]}" | sort > "$CURRENT_VIOLATIONS"
    
    # Use comm to find new violations (in current but not in baseline)
    # comm -13: lines only in file1 (current), not in file2 (baseline)
    NEW_VIOLATIONS=$(comm -13 "$BASELINE_FILE" "$CURRENT_VIOLATIONS")
    
    # Clean up temp file
    rm -f "$CURRENT_VIOLATIONS"
    
    # Summary
    echo "================================================================================"
    if [ -z "$NEW_VIOLATIONS" ]; then
        if [ $VIOLATIONS -eq 0 ]; then
            echo -e "${GREEN}SUCCESS:${NC} No violations found (baseline empty or all violations fixed)"
        else
            echo -e "${GREEN}SUCCESS:${NC} No NEW violations found"
            echo "  Total violations: $VIOLATIONS (all in baseline)"
            echo "  Baseline file: $BASELINE_FILE"
        fi
        echo ""
        echo "Allowed paths:"
        for allowed in "${ALLOWED_PATHS[@]}"; do
            echo "  - $allowed"
        done
        exit 0
    else
        # Count new violations (lines in NEW_VIOLATIONS)
        NEW_COUNT=$(echo "$NEW_VIOLATIONS" | grep -c '^' || echo "0")
        echo -e "${RED}FAILURE:${NC} Found $NEW_COUNT NEW violation(s) not in baseline"
        echo ""
        echo "New violations:"
        echo "$NEW_VIOLATIONS" | while IFS= read -r violation; do
            [ -n "$violation" ] && echo "  - $violation"
        done
        echo ""
        echo "Baseline file: $BASELINE_FILE"
        echo ""
        echo "Resolution:"
        echo "  1. Fix new violations by moving JSON parsing/encoding to codec modules"
        echo "  2. Or update baseline if violations are intentional: $0 --baseline"
        echo "  3. See docs/manifest_schema.md for codec implementation patterns"
        exit 1
    fi
fi

# Summary (strict mode - no baseline)
echo "================================================================================"
if [ $VIOLATIONS -eq 0 ]; then
    echo -e "${GREEN}SUCCESS:${NC} No JSON read or write violations found outside codec modules"
    echo ""
    echo "Allowed paths:"
    for allowed in "${ALLOWED_PATHS[@]}"; do
        echo "  - $allowed"
    done
    exit 0
else
    echo -e "${RED}FAILURE:${NC} Found $VIOLATIONS file(s) with JSON reads or writes outside codec modules"
    echo ""
    echo "Files with violations:"
    for file in "${FILES_WITH_VIOLATIONS[@]}"; do
        echo "  - $file"
    done
    echo ""
    echo "Resolution:"
    echo "  1. Move JSON request parsing to a codec module (e.g., firmware/v2/src/codec/)"
    echo "  2. Use typed structs instead of direct JSON key reads (doc[\"key\"] | default)"
    echo "  3. Move JSON response encoding to a codec module (use encoder functions)"
    echo "  4. Use encoder functions instead of direct JSON key writes (data[\"key\"] = value)"
    echo "  5. See docs/manifest_schema.md for codec implementation patterns"
    exit 1
fi
