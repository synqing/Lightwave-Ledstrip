#!/bin/bash
# Test script for audio benchmark testing tools
# Validates that all tools work correctly before CI/CD deployment

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "======================================================================"
echo "Audio Benchmark Testing Tools - Validation Script"
echo "======================================================================"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

# Test function
run_test() {
    local test_name="$1"
    local test_cmd="$2"
    local expected_exit="${3:-0}"

    echo -n "Testing: $test_name ... "

    if eval "$test_cmd" > /tmp/test_output.log 2>&1; then
        actual_exit=0
    else
        actual_exit=$?
    fi

    if [ "$actual_exit" -eq "$expected_exit" ]; then
        echo -e "${GREEN}PASS${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        echo "  Expected exit code: $expected_exit"
        echo "  Actual exit code: $actual_exit"
        echo "  Output:"
        cat /tmp/test_output.log | head -20
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

echo "Step 1: Checking Python environment"
echo "----------------------------------------------------------------------"
python3 --version
echo ""

echo "Step 2: Testing parse_benchmark_serial.py"
echo "----------------------------------------------------------------------"

run_test "Parse sample output (summary format)" \
    "python3 parse_benchmark_serial.py sample_test_output.log --platformio --format summary"

run_test "Parse sample output (JSON format)" \
    "python3 parse_benchmark_serial.py sample_test_output.log --platformio -o /tmp/test_current.json"

run_test "Verify JSON output exists" \
    "test -f /tmp/test_current.json"

echo ""

echo "Step 3: Testing detect_regressions.py"
echo "----------------------------------------------------------------------"

run_test "Regression detection with good metrics" \
    "python3 detect_regressions.py ../baseline/benchmark_baseline.json /tmp/test_current.json --format console"

run_test "Regression detection (markdown format)" \
    "python3 detect_regressions.py ../baseline/benchmark_baseline.json /tmp/test_current.json --format markdown"

run_test "Regression detection (JSON format)" \
    "python3 detect_regressions.py ../baseline/benchmark_baseline.json /tmp/test_current.json --format json"

# Create a regression test case
cat > /tmp/test_regression.json << EOF
{
  "snr_db": 38.0,
  "false_trigger_rate": 5.5,
  "cpu_load_percent": 52.0,
  "latency_ms": 12.0,
  "memory_usage_kb": 145.0
}
EOF

run_test "Regression detection with failures (should exit 1)" \
    "python3 detect_regressions.py ../baseline/benchmark_baseline.json /tmp/test_regression.json --format console" \
    1

echo ""

echo "Step 4: Testing edge cases"
echo "----------------------------------------------------------------------"

# Create minimal output
cat > /tmp/minimal_output.log << EOF
SNR: 45.0 dB
CPU Load: 35.0%
EOF

run_test "Parse minimal output" \
    "python3 parse_benchmark_serial.py /tmp/minimal_output.log -o /tmp/minimal.json"

# Create warning scenario
cat > /tmp/test_warning.json << EOF
{
  "snr_db": 43.0,
  "false_trigger_rate": 3.0,
  "cpu_load_percent": 40.0,
  "latency_ms": 9.5
}
EOF

run_test "Regression detection with warnings" \
    "python3 detect_regressions.py ../baseline/benchmark_baseline.json /tmp/test_warning.json --format console"

run_test "Regression detection with warnings (fail-on-warning)" \
    "python3 detect_regressions.py ../baseline/benchmark_baseline.json /tmp/test_warning.json --fail-on-warning" \
    1

echo ""

echo "Step 5: Testing help documentation"
echo "----------------------------------------------------------------------"

run_test "Parse script help" \
    "python3 parse_benchmark_serial.py --help"

run_test "Regression script help" \
    "python3 detect_regressions.py --help"

echo ""

echo "======================================================================"
echo "Test Results Summary"
echo "======================================================================"
echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"
if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "${RED}Failed: $TESTS_FAILED${NC}"
    echo ""
    echo "Some tests failed. Please review the output above."
    exit 1
else
    echo -e "${GREEN}All tests passed!${NC}"
    echo ""
    echo "The audio benchmark testing tools are ready for CI/CD deployment."
fi

# Cleanup
rm -f /tmp/test_output.log /tmp/test_current.json /tmp/test_regression.json /tmp/minimal_output.log /tmp/minimal.json /tmp/test_warning.json

exit 0
