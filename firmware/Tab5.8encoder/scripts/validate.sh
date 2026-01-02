#!/bin/bash
# Validation script for Tab5.8encoder repository

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC_DIR="$REPO_ROOT/src"
ERRORS=0

echo "=== Tab5.8encoder Validation ==="
echo ""

# Check 1: Forbidden I2C reset patterns
echo "[1/5] Checking for forbidden I2C reset patterns..."
if grep -r "periph_module_reset\|i2cDeinit0\|PERIPH_I2C0_MODULE\|i2cDeinit(0)" "$SRC_DIR" > /dev/null 2>&1; then
    echo "  ❌ ERROR: Found forbidden patterns"
    grep -rn "periph_module_reset\|i2cDeinit0\|PERIPH_I2C0_MODULE\|i2cDeinit(0)" "$SRC_DIR" | head -5
    ERRORS=$((ERRORS + 1))
else
    echo "  ✅ No forbidden patterns found"
fi
echo ""

# Check 2: Verify M5.Ex_I2C usage
echo "[2/5] Checking M5.Ex_I2C usage..."
if grep -q "M5\.Ex_I2C" "$REPO_ROOT/src/main.cpp"; then
    echo "  ✅ M5.Ex_I2C used in main.cpp"
else
    echo "  ❌ ERROR: M5.Ex_I2C not found"
    ERRORS=$((ERRORS + 1))
fi
echo ""

# Check 3: Verify required files
echo "[3/5] Checking required files..."
REQUIRED=("src/main.cpp" "src/config/Config.h" "README.md" "platformio.ini")
MISSING=0
for file in "${REQUIRED[@]}"; do
    [ ! -f "$REPO_ROOT/$file" ] && echo "  ❌ Missing: $file" && MISSING=$((MISSING + 1)) && ERRORS=$((ERRORS + 1))
done
[ $MISSING -eq 0 ] && echo "  ✅ All required files present"
echo ""

# Check 4: Milestones
echo "[4/5] Checking milestones..."
for m in "MILESTONE A" "MILESTONE B" "MILESTONE C" "MILESTONE D"; do
    grep -q "$m" "$REPO_ROOT/src/main.cpp" || (echo "  ❌ Missing: $m" && ERRORS=$((ERRORS + 1)))
done
[ $ERRORS -eq 0 ] && echo "  ✅ All milestones implemented"
echo ""

# Summary
echo "=== Summary ==="
[ $ERRORS -eq 0 ] && echo "✅ Validation passed!" && exit 0 || echo "❌ Validation failed with $ERRORS error(s)." && exit 1
