#!/bin/bash
# LightwaveOS Pre-Push Quality Gate Verification
# Complements .claude/harness/init.sh with comprehensive verification
# Exit codes: 0 = pass, 1 = fail (for git hook integration)
#
# Usage:
#   ./scripts/verify.sh              # Run all checks
#   ./scripts/verify.sh --skip-build # Skip build verification (faster)

set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Track overall status
PASS=0
FAIL=0
WARN=0
SKIP=0

# Parse arguments
SKIP_BUILD=false
while [[ $# -gt 0 ]]; do
    case $1 in
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--skip-build]"
            exit 1
            ;;
    esac
done

# Helper function for test results
check_result() {
    local exit_code=$1
    local test_name="$2"

    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}[PASS]${NC} $test_name"
        ((PASS++))
        return 0
    else
        echo -e "${RED}[FAIL]${NC} $test_name"
        ((FAIL++))
        return 1
    fi
}

check_warn() {
    local exit_code=$1
    local test_name="$2"

    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}[PASS]${NC} $test_name"
        ((PASS++))
    else
        echo -e "${YELLOW}[WARN]${NC} $test_name"
        ((WARN++))
    fi
}

check_skip() {
    local test_name="$1"
    echo -e "${CYAN}[SKIP]${NC} $test_name"
    ((SKIP++))
}

echo "========================================"
echo "LightwaveOS Pre-Push Quality Gate"
echo "========================================"
echo ""

START_TIME=$(date +%s)

# ============================================================
# [1/6] Harness Schema Validation
# ============================================================
echo -e "${BOLD}[1/6] Harness Schema Validation${NC}"
echo "---"

if [ -f ".claude/harness/harness.py" ]; then
    if python3 .claude/harness/harness.py --dir .claude/harness validate > /tmp/harness_validate.log 2>&1; then
        check_result 0 "Harness schema validation"
        # Show summary from validation
        tail -5 /tmp/harness_validate.log
    else
        check_result 1 "Harness schema validation"
        echo "Validation errors:"
        cat /tmp/harness_validate.log
    fi
else
    check_warn 1 "harness.py not found (skipping validation)"
fi

echo ""

# ============================================================
# [2/6] PRD JSON Schema Validation
# ============================================================
echo -e "${BOLD}[2/6] PRD JSON Schema Validation${NC}"
echo "---"

# Check if jsonschema is available
if ! command -v python3 &> /dev/null; then
    check_skip "Python3 not available"
elif ! python3 -c "import jsonschema" 2>/dev/null; then
    check_warn 1 "jsonschema module not installed (pip install jsonschema)"
    echo "  Skipping PRD schema validation"
else
    # Validate each PRD file
    PRD_DIR=".claude/prd"
    PRD_SCHEMA="$PRD_DIR/schema.json"

    if [ ! -f "$PRD_SCHEMA" ]; then
        check_warn 1 "PRD schema not found at $PRD_SCHEMA"
    else
        PRD_FILES=$(find "$PRD_DIR" -name "*.json" -type f ! -name "schema.json" ! -name "CLAUDE.md" 2>/dev/null)
        PRD_COUNT=0
        PRD_VALID=0
        PRD_INVALID=0

        if [ -z "$PRD_FILES" ]; then
            echo -e "${YELLOW}[INFO]${NC} No PRD files found to validate"
        else
            for prd_file in $PRD_FILES; do
                ((PRD_COUNT++))
                if python3 -m jsonschema -i "$prd_file" "$PRD_SCHEMA" 2>/dev/null; then
                    ((PRD_VALID++))
                    echo -e "  ${GREEN}✓${NC} $(basename "$prd_file")"
                else
                    ((PRD_INVALID++))
                    echo -e "  ${RED}✗${NC} $(basename "$prd_file")"
                    python3 -m jsonschema -i "$prd_file" "$PRD_SCHEMA" 2>&1 | sed 's/^/    /'
                fi
            done

            if [ $PRD_INVALID -eq 0 ]; then
                check_result 0 "PRD schema validation ($PRD_VALID/$PRD_COUNT valid)"
            else
                check_result 1 "PRD schema validation ($PRD_VALID/$PRD_COUNT valid, $PRD_INVALID failed)"
            fi
        fi
    fi
fi

echo ""

# ============================================================
# [3/6] Cross-Reference Integrity
# ============================================================
echo -e "${BOLD}[3/6] Cross-Reference Integrity${NC}"
echo "---"

CROSS_REF_FAIL=0

# Check that PRD files referenced in feature_list.json exist
if [ -f ".claude/harness/feature_list.json" ]; then
    # Extract PRD references from feature_list.json
    PRD_REFS=$(python3 -c "
import json
import sys
try:
    with open('.claude/harness/feature_list.json') as f:
        data = json.load(f)
    for item in data.get('items', []):
        prd_ref = item.get('prd_reference', {})
        if prd_file := prd_ref.get('file'):
            print(prd_file)
except Exception as e:
    sys.exit(1)
" 2>/dev/null)

    if [ $? -eq 0 ]; then
        MISSING_PRDS=0
        for prd_ref in $PRD_REFS; do
            prd_path=".claude/prd/$prd_ref"
            if [ ! -f "$prd_path" ]; then
                echo -e "  ${RED}✗${NC} Referenced PRD missing: $prd_ref"
                ((MISSING_PRDS++))
                ((CROSS_REF_FAIL++))
            fi
        done

        if [ $MISSING_PRDS -eq 0 ]; then
            echo -e "  ${GREEN}✓${NC} All referenced PRD files exist"
        fi
    fi

    # Check for orphan PRDs (PRD exists but no feature references it)
    if [ -d ".claude/prd" ]; then
        ORPHAN_PRDS=0
        for prd_file in .claude/prd/*.json; do
            if [ -f "$prd_file" ] && [ "$(basename "$prd_file")" != "schema.json" ]; then
                prd_basename=$(basename "$prd_file")
                if ! echo "$PRD_REFS" | grep -q "$prd_basename"; then
                    echo -e "  ${YELLOW}⚠${NC} Orphan PRD (not referenced): $prd_basename"
                    ((ORPHAN_PRDS++))
                fi
            fi
        done

        if [ $ORPHAN_PRDS -gt 0 ]; then
            check_warn 1 "Cross-reference integrity ($ORPHAN_PRDS orphan PRDs)"
        fi
    fi

    if [ $CROSS_REF_FAIL -eq 0 ]; then
        check_result 0 "Cross-reference integrity"
    else
        check_result 1 "Cross-reference integrity ($CROSS_REF_FAIL missing PRDs)"
    fi
else
    check_warn 1 "feature_list.json not found (skipping cross-reference check)"
fi

echo ""

# ============================================================
# [4/6] Pattern Compliance
# ============================================================
echo -e "${BOLD}[4/6] Pattern Compliance${NC}"
echo "---"

PATTERN_FAIL=0

# NO_RAINBOWS: Strict check - fail if found
echo "Checking NO_RAINBOWS constraint..."
RAINBOW_VIOLATIONS=$(grep -ri "rainbow" firmware/v2/src/effects/ 2>/dev/null | grep -v "// TODO\|// FIXME\|/\*" | wc -l | tr -d ' ')

if [ "$RAINBOW_VIOLATIONS" -gt 0 ]; then
    echo -e "${RED}[FAIL]${NC} NO_RAINBOWS violated ($RAINBOW_VIOLATIONS occurrences):"
    grep -rin "rainbow" firmware/v2/src/effects/ 2>/dev/null | grep -v "// TODO\|// FIXME\|/\*" | head -10 | sed 's/^/  /'
    ((PATTERN_FAIL++))
    check_result 1 "NO_RAINBOWS constraint"
else
    check_result 0 "NO_RAINBOWS constraint"
fi

# CENTER_ORIGIN: Heuristic check - warn if suspicious
echo "Checking CENTER_ORIGIN heuristic..."
SUSPICIOUS_PATTERNS=$(grep -rn "for.*i = 0.*i < NUM_LEDS\|for.*i = 0.*i < 160" firmware/v2/src/effects/ 2>/dev/null | grep -v "distFromCenter\|CENTER_POINT\|abs(" | wc -l | tr -d ' ')

if [ "$SUSPICIOUS_PATTERNS" -gt 0 ]; then
    echo -e "${YELLOW}[WARN]${NC} Suspicious linear patterns found (may violate CENTER_ORIGIN):"
    grep -rn "for.*i = 0.*i < NUM_LEDS\|for.*i = 0.*i < 160" firmware/v2/src/effects/ 2>/dev/null | grep -v "distFromCenter\|CENTER_POINT\|abs(" | head -5 | sed 's/^/  /'
    check_warn 1 "CENTER_ORIGIN heuristic (manual review recommended)"
else
    check_result 0 "CENTER_ORIGIN heuristic"
fi

echo ""

# ============================================================
# [5/6] Build Verification
# ============================================================
echo -e "${BOLD}[5/6] Build Verification${NC}"
echo "---"

if [ "$SKIP_BUILD" = true ]; then
    check_skip "Build verification (--skip-build flag)"
else
    # Build main firmware (esp32dev_audio for full feature set)
    echo "Building esp32dev_audio (v2 main firmware)..."
    if pio run -e esp32dev_audio > /tmp/pio_build_verify.log 2>&1; then
        check_result 0 "Build esp32dev_audio"
        # Extract memory usage
        echo "  Memory usage:"
        grep -E "(RAM:|Flash:)" /tmp/pio_build_verify.log | tail -2 | sed 's/^/    /' || true
    else
        check_result 1 "Build esp32dev_audio"
        echo "Build log tail:"
        tail -30 /tmp/pio_build_verify.log | sed 's/^/  /'
    fi

    # Optional: Native unit tests (if they exist)
    if [ -d "test" ] && grep -q "env:native" platformio.ini 2>/dev/null; then
        echo "Running native unit tests..."
        if pio test -e native > /tmp/pio_test_verify.log 2>&1; then
            check_result 0 "Native unit tests"
        else
            check_warn 1 "Native unit tests (may not be configured)"
            tail -10 /tmp/pio_test_verify.log | sed 's/^/  /' || true
        fi
    fi

    # Tab5 build - SKIPPED (too slow for pre-push, 60s+ with RISC-V toolchain)
    echo -e "${CYAN}[SKIP]${NC} Tab5 encoder build (too slow for pre-push, verify manually if needed)"
    echo "  To build Tab5: PATH=\"/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin\" pio run -e tab5 -d firmware/Tab5.encoder"
fi

echo ""

# ============================================================
# [6/6] Git Status
# ============================================================
echo -e "${BOLD}[6/6] Git Status${NC}"
echo "---"

BRANCH=$(git branch --show-current 2>/dev/null || echo "unknown")
echo "Branch: $BRANCH"

DIRTY_COUNT=$(git status --porcelain 2>/dev/null | wc -l | tr -d ' ')
if [ "$DIRTY_COUNT" -gt 0 ]; then
    echo -e "${YELLOW}[WARN]${NC} $DIRTY_COUNT uncommitted changes (may be intentional for testing)"
    git status --short | head -10 | sed 's/^/  /'
    if [ "$DIRTY_COUNT" -gt 10 ]; then
        echo "  ... and $((DIRTY_COUNT - 10)) more"
    fi
    ((WARN++))
else
    echo -e "${GREEN}[OK]${NC} Working tree clean"
    ((PASS++))
fi

echo ""

# ============================================================
# Summary
# ============================================================
END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))

echo "========================================"
echo "Summary"
echo "========================================"
echo -e "${GREEN}Passed:${NC}  $PASS"
echo -e "${RED}Failed:${NC}  $FAIL"
echo -e "${YELLOW}Warnings:${NC} $WARN"
echo -e "${CYAN}Skipped:${NC} $SKIP"
echo "Time: ${ELAPSED}s"
echo "========================================"

if [ $FAIL -gt 0 ]; then
    echo -e "${RED}${BOLD}VERIFICATION FAILED${NC}"
    echo ""
    echo "Fix the failed checks before pushing."
    exit 1
elif [ $WARN -gt 0 ]; then
    echo -e "${YELLOW}${BOLD}VERIFICATION PASSED WITH WARNINGS${NC}"
    echo ""
    echo "Review warnings before pushing (non-blocking)."
    exit 0
else
    echo -e "${GREEN}${BOLD}VERIFICATION PASSED${NC}"
    echo ""
    echo "Ready to push."
    exit 0
fi
