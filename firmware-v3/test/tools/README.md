# Audio Benchmark Testing Tools

This directory contains Python scripts for automated audio performance regression testing in the LightwaveOS CI/CD pipeline.

## Overview

The audio benchmark testing system provides:

- **Automated metric extraction** from test output
- **Statistical regression detection** with configurable thresholds
- **CI/CD integration** via GitHub Actions
- **PR comments** with benchmark results and regression warnings

## Tools

### parse_benchmark_serial.py

Extracts performance metrics from serial logs or test output.

**Usage:**
```bash
# Parse PlatformIO test output
python parse_benchmark_serial.py test_output.log --platformio -o results.json

# Parse raw serial output
python parse_benchmark_serial.py serial.log -o results.json

# Print summary to console
python parse_benchmark_serial.py test_output.log --format summary
```

**Supported Metrics:**
- `snr_db` - Signal-to-Noise Ratio (dB)
- `false_trigger_rate` - False positive detection rate (%)
- `cpu_load_percent` - CPU utilization (%)
- `latency_ms` - Processing latency (ms)
- `memory_usage_kb` - Memory usage (KB)
- `sample_rate_hz` - Audio sample rate (Hz)
- `buffer_size` - Audio buffer size (samples)

**Output Format (JSON):**
```json
{
  "snr_db": 45.0,
  "false_trigger_rate": 2.5,
  "cpu_load_percent": 35.0,
  "latency_ms": 8.5,
  "memory_usage_kb": 128.0,
  "sample_rate_hz": 44100,
  "buffer_size": 512
}
```

### detect_regressions.py

Compares current benchmark results against baseline with statistical thresholds.

**Usage:**
```bash
# Console report
python detect_regressions.py baseline.json current.json

# Markdown report for PR comments
python detect_regressions.py baseline.json current.json --format markdown -o report.md

# JSON output for automation
python detect_regressions.py baseline.json current.json --format json -o results.json

# Fail on warnings (strict mode)
python detect_regressions.py baseline.json current.json --fail-on-warning
```

**Regression Thresholds:**

| Metric | Warning | Failure | Direction |
|--------|---------|---------|-----------|
| SNR | -5% | -10% | Higher is better |
| False Trigger Rate | +50% | +100% | Lower is better |
| CPU Load | +20% | +40% | Lower is better |
| Latency | +15% | +30% | Lower is better |
| Memory Usage | +25% | +50% | Lower is better |

**Exit Codes:**
- `0` - All tests pass
- `1` - Regression detected (or warning with `--fail-on-warning`)

## Baseline Metrics

The baseline metrics are stored in `v2/test/baseline/benchmark_baseline.json`.

**Updating Baseline:**

1. **Manual Update**: Edit `benchmark_baseline.json` directly
2. **Automated Update**: Trigger the `update-baseline` workflow job (requires main branch)

Only update the baseline after verified performance improvements. Never update to mask regressions!

## GitHub Actions Workflow

The `audio_benchmark.yml` workflow runs automatically on:

- Pull requests modifying `v2/src/audio/**` or `v2/test/test_audio/**`
- Pushes to `main` or `feature/audio-*` branches
- Manual workflow dispatch

**Workflow Steps:**

1. Build native test environment
2. Run audio benchmark tests
3. Parse benchmark output
4. Detect regressions vs baseline
5. Post results as PR comment
6. Upload artifacts
7. Fail if regressions exceed thresholds

**Artifacts:**

Benchmark results are uploaded as workflow artifacts (30-day retention):
- `current_benchmark.json` - Latest test results
- `regression_report.md` - Markdown regression report
- `benchmark_output.log` - Raw test output

## Local Testing

Run benchmarks locally before pushing:

```bash
# Build and run tests
cd v2
pio test -e native_test -f test_pipeline_benchmark --verbose > benchmark_output.log

# Parse results
python test/tools/parse_benchmark_serial.py \
  benchmark_output.log \
  --platformio \
  --format summary

# Check for regressions
python test/tools/detect_regressions.py \
  test/baseline/benchmark_baseline.json \
  test/results/current_benchmark.json
```

## Adding New Metrics

To add a new benchmark metric:

1. **Update test code** to output metric in format: `Metric Name: VALUE unit`
2. **Add regex pattern** to `parse_benchmark_serial.py` in `PATTERNS` dict
3. **Define threshold** in `detect_regressions.py` in `DEFAULT_THRESHOLDS`
4. **Update baseline** with initial value in `benchmark_baseline.json`

**Example:**

```python
# In parse_benchmark_serial.py
PATTERNS = {
    'new_metric': r'New\s+Metric:\s*([\d.]+)\s*unit',
    # ...
}

# In detect_regressions.py
DEFAULT_THRESHOLDS = {
    'new_metric': Threshold(
        warning_percent=10.0,   # +10% warning
        failure_percent=20.0,   # +20% failure
        higher_is_better=False  # Lower values preferred
    ),
    # ...
}
```

## Integration with PlatformIO

The scripts expect benchmark output in Unity test framework format:

```
test/test_audio/test_pipeline_benchmark.cpp:
test_audio_snr_measurement
SNR: 45.2 dB
False Trigger Rate: 2.3%
CPU Load: 34.8%
Latency: 8.4 ms
OK
```

Use the `AudioPipelineBenchmark` class in your test code to generate compatible output.

## Troubleshooting

**No metrics found in output:**
- Check that test output includes metric lines matching regex patterns
- Verify benchmark test actually ran (check for test function name in output)
- Try `--format summary` to see what was parsed

**Regression false positives:**
- Adjust thresholds in `detect_regressions.py` if too sensitive
- Check baseline is appropriate for current platform/compiler
- Consider statistical variance in measurements

**Workflow fails to comment on PR:**
- Check GitHub token permissions
- Verify `regression_report.md` was generated
- Check workflow logs for `actions/github-script` errors

## Development

The tools use only Python standard library (no external dependencies) for maximum portability and CI/CD compatibility.

**Code Style:**
- Type hints for all functions
- Dataclasses for structured data
- Docstrings for public APIs
- Exit codes: 0 = success, 1 = failure

**Testing:**
```bash
# Lint
pylint parse_benchmark_serial.py detect_regressions.py

# Type check
mypy parse_benchmark_serial.py detect_regressions.py

# Test with sample data
python detect_regressions.py \
  ../baseline/benchmark_baseline.json \
  ../baseline/benchmark_baseline.json  # Should pass with 0% change
```

## See Also

- [Audio Pipeline Documentation](../../docs/api/AUDIO_STREAMING.md)
- [Audio Control API](../../docs/api/AUDIO_CONTROL_API.md)
- [PlatformIO Testing Guide](https://docs.platformio.org/en/latest/advanced/unit-testing/index.html)
