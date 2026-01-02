# Audio Benchmark CI/CD Pipeline

Comprehensive documentation for the automated audio performance regression testing pipeline in LightwaveOS.

## Overview

The audio benchmark CI/CD pipeline provides automated performance regression detection for audio processing code changes. It runs on every pull request that modifies audio-related code and posts detailed results as PR comments.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│ GitHub Actions Trigger                                       │
│ - PR with audio/** changes                                   │
│ - Push to main/feature branches                              │
│ - Manual workflow dispatch                                   │
└────────────────┬────────────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────────────┐
│ Build & Test Phase                                           │
│ - Setup Python 3.11                                          │
│ - Install PlatformIO                                         │
│ - Build native_test environment                              │
│ - Run test_pipeline_benchmark                                │
└────────────────┬────────────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────────────┐
│ Parse Results                                                │
│ parse_benchmark_serial.py                                    │
│ - Extract metrics from test output                           │
│ - Generate current_benchmark.json                            │
└────────────────┬────────────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────────────┐
│ Regression Detection                                         │
│ detect_regressions.py                                        │
│ - Compare vs baseline metrics                                │
│ - Apply statistical thresholds                               │
│ - Generate regression_report.md                              │
└────────────────┬────────────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────────────┐
│ Report & Artifacts                                           │
│ - Post PR comment with results                               │
│ - Upload artifacts (30-day retention)                        │
│ - Fail build if regressions detected                         │
└─────────────────────────────────────────────────────────────┘
```

## Trigger Conditions

The workflow triggers on:

1. **Pull Requests** modifying:
   - `v2/src/audio/**`
   - `v2/test/test_audio/**`
   - `v2/test/baseline/**`
   - `v2/platformio.ini`
   - `.github/workflows/audio_benchmark.yml`

2. **Push to branches**:
   - `main`
   - `feature/audio-*`

3. **Manual Trigger**:
   - Workflow dispatch from Actions tab

## Performance Metrics

### Tracked Metrics

| Metric | Unit | Description | Ideal Direction |
|--------|------|-------------|-----------------|
| SNR | dB | Signal-to-Noise Ratio | Higher is better |
| False Trigger Rate | % | Incorrect beat/onset detection | Lower is better |
| CPU Load | % | Processing utilization | Lower is better |
| Latency | ms | Input-to-output delay | Lower is better |
| Memory Usage | KB | Heap allocation | Lower is better |

### Regression Thresholds

Each metric has two threshold levels:

| Metric | Warning | Failure | Notes |
|--------|---------|---------|-------|
| SNR | -5% | -10% | Small decreases acceptable |
| False Trigger Rate | +50% | +100% | Strict to maintain quality |
| CPU Load | +20% | +40% | Room for optimization |
| Latency | +15% | +30% | Real-time constraint |
| Memory Usage | +25% | +50% | ESP32-S3 has limited heap |

**Threshold Behavior**:
- **Pass**: Metric improved or within tolerances
- **Warning**: Metric approaching regression threshold (workflow passes but warns)
- **Failure**: Metric exceeds failure threshold (workflow fails, blocks merge)

## Workflow Jobs

### Job 1: `audio-benchmark`

Main testing and analysis job.

**Steps**:

1. **Checkout repository** - Full git history for context
2. **Setup Python 3.11** - With pip cache
3. **Cache PlatformIO** - Speeds up builds (~5 min → ~1 min)
4. **Install PlatformIO** - Latest stable version
5. **Install analysis tools** - Python dependencies (currently none)
6. **Build native test** - Compile without running tests
7. **Run benchmark tests** - Execute `test_pipeline_benchmark`
8. **Parse results** - Extract metrics to JSON
9. **Detect regressions** - Compare against baseline
10. **Upload artifacts** - Save results (30 days)
11. **Comment on PR** - Post formatted report
12. **Check regression failures** - Fail build if thresholds exceeded
13. **Generate job summary** - GitHub Actions summary page

**Artifacts**:
- `current_benchmark.json` - Latest metrics
- `regression_report.md` - Markdown report
- `benchmark_output.log` - Raw test output

### Job 2: `update-baseline` (Manual Only)

Updates baseline metrics after verified improvements.

**Triggers**: Only via workflow dispatch on `main` branch

**Steps**:
1. Download benchmark results from `audio-benchmark` job
2. Copy `current_benchmark.json` to baseline
3. Update metadata (timestamp, git commit, etc.)
4. Create PR with changes

**Safety**: Requires manual PR review to prevent masking regressions!

## Local Development Workflow

### Running Tests Locally

```bash
# Navigate to v2 directory
cd v2

# Run benchmark test
pio test -e native_test -f test_pipeline_benchmark --verbose > benchmark_output.log

# View raw output
cat benchmark_output.log

# Parse results
python test/tools/parse_benchmark_serial.py \
  benchmark_output.log \
  --platformio \
  --format summary

# Generate JSON
python test/tools/parse_benchmark_serial.py \
  benchmark_output.log \
  --platformio \
  -o test/results/current_benchmark.json

# Check for regressions
python test/tools/detect_regressions.py \
  test/baseline/benchmark_baseline.json \
  test/results/current_benchmark.json

# Generate markdown report
python test/tools/detect_regressions.py \
  test/baseline/benchmark_baseline.json \
  test/results/current_benchmark.json \
  --format markdown \
  -o test/results/regression_report.md
```

### Validating Tools

Before pushing changes to the CI/CD scripts:

```bash
cd firmware/v2/test/tools

# Run validation tests
chmod +x test_tools.sh
./test_tools.sh

# Should output:
# ✓ All tests passed!
```

## Pull Request Flow

1. **Developer pushes PR** with audio code changes
2. **Workflow triggers** automatically
3. **Tests run** (~3-5 minutes)
4. **Bot comments** on PR with results:
   ```
   ## Audio Benchmark Results ✅

   **Status**: PASS - All metrics within acceptable ranges

   | Metric | Baseline | Current | Change | Status |
   |--------|----------|---------|--------|--------|
   | snr_db | 45.00 | 45.20 | +0.4% | ✅ |
   ...
   ```
5. **Reviewer checks** metrics before approval
6. **Merge** if passing (or investigate regressions)

### Regression Scenario

If regressions are detected:

```
## Audio Benchmark Results ❌

**Status**: REGRESSION DETECTED - Performance degraded beyond acceptable thresholds

| Metric | Baseline | Current | Change | Status |
|--------|----------|---------|--------|--------|
| snr_db | 45.00 | 38.00 | -15.6% | ❌ |
| cpu_load_percent | 35.00 | 52.00 | +48.6% | ❌ |
...

### Details
- **snr_db**: Performance regression: -15.6% decrease (threshold: -10.0%)
- **cpu_load_percent**: Performance regression: +48.6% increase (threshold: +40.0%)
```

**Developer Actions**:
1. Review code changes causing regression
2. Optimize or justify performance impact
3. Update tests/thresholds if intentional trade-off
4. Request threshold adjustment if baseline unrealistic

## Updating Baseline Metrics

### When to Update

Only update baseline after:
- ✅ Verified performance improvements
- ✅ Hardware/platform changes (e.g., new ESP32 model)
- ✅ Major refactoring with intentional trade-offs
- ❌ NEVER to mask regressions

### Update Process

**Option 1: Manual Edit** (Preferred for small changes)

1. Edit `v2/test/baseline/benchmark_baseline.json`
2. Update metric values
3. Update `_metadata.updated` timestamp
4. Commit and create PR
5. Get thorough review

**Option 2: Automated Update** (For systematic updates)

1. Ensure `main` branch has desired metrics
2. Go to Actions → Audio Benchmark Regression Testing
3. Click "Run workflow" → Select `main` branch
4. Wait for workflow to complete
5. Review auto-generated PR
6. Verify changes are improvements
7. Merge if appropriate

## Customization

### Adding New Metrics

To track additional performance metrics:

**1. Update Test Code**

Ensure benchmark test outputs metric in parseable format:

```cpp
// In v2/test/test_audio/test_pipeline_benchmark.cpp
Serial.printf("New Metric: %.2f units\n", value);
```

**2. Add Parse Pattern**

In `v2/test/tools/parse_benchmark_serial.py`:

```python
PATTERNS = {
    'new_metric': r'New\s+Metric:\s*([\d.]+)\s*units',
    # ...
}
```

**3. Define Threshold**

In `v2/test/tools/detect_regressions.py`:

```python
DEFAULT_THRESHOLDS = {
    'new_metric': Threshold(
        warning_percent=10.0,
        failure_percent=20.0,
        higher_is_better=False  # or True
    ),
    # ...
}
```

**4. Update Baseline**

Add to `v2/test/baseline/benchmark_baseline.json`:

```json
{
  "new_metric": 42.0,
  ...
}
```

### Adjusting Thresholds

Edit `DEFAULT_THRESHOLDS` in `detect_regressions.py`:

```python
'cpu_load_percent': Threshold(
    warning_percent=25.0,   # Changed from 20.0
    failure_percent=50.0,   # Changed from 40.0
    higher_is_better=False
),
```

Commit changes and push - next run will use new thresholds.

## Troubleshooting

### Workflow Fails: "No benchmark metrics found"

**Cause**: Test output format doesn't match regex patterns

**Fix**:
1. Check `benchmark_output.log` artifact
2. Verify test actually ran (look for test function name)
3. Ensure output format matches patterns in `parse_benchmark_serial.py`
4. Add debug prints to test code if needed

### False Positive Regressions

**Cause**: Statistical variance or unrealistic baseline

**Fix**:
1. Run tests multiple times locally
2. Calculate average and variance
3. Adjust thresholds if variance is high
4. Update baseline if consistently different

### PR Comment Not Posted

**Cause**: GitHub token permissions or missing report file

**Fix**:
1. Check workflow logs for `actions/github-script` errors
2. Verify `regression_report.md` artifact exists
3. Check repository settings → Actions → Workflow permissions
4. Ensure "Read and write permissions" enabled

### Build Timeout

**Cause**: PlatformIO cache miss or slow network

**Fix**:
1. Check cache hit rate in workflow logs
2. Increase `timeout-minutes` if necessary
3. Investigate slow test execution

## Performance Optimization

### Workflow Speed

Current average runtime: **3-5 minutes**

Breakdown:
- Setup (checkout, Python, cache): 30-60s
- PlatformIO install: 20-30s
- Build native test: 60-120s
- Run tests: 30-60s
- Analysis & reporting: 10-20s

**Optimization tips**:
- Keep PlatformIO cache hit rate high (>90%)
- Minimize test duration while maintaining accuracy
- Use `continue-on-error` for non-blocking steps

### Cost Optimization

GitHub Actions free tier: 2,000 minutes/month

Estimated usage:
- 5 min/run × 20 PRs/week × 4 weeks = 400 min/month
- Well within free tier

## Security Considerations

### Token Permissions

Workflow uses `GITHUB_TOKEN` with:
- `contents: read` - Read repository
- `issues: write` - Post PR comments
- `pull-requests: write` - Update PR status

**No elevated permissions required.**

### Dependency Security

All Python tools use **standard library only** - no external dependencies to manage or audit.

Optional dependencies in `requirements.txt` are commented out.

## Future Enhancements

Potential improvements:

1. **Historical Trending**
   - Track metrics over time
   - Visualize performance trends
   - Detect gradual degradation

2. **Platform-Specific Baselines**
   - ESP32-S3 vs native test baselines
   - Compiler-specific thresholds

3. **Automatic Threshold Tuning**
   - Statistical analysis of variance
   - Self-adjusting thresholds

4. **Performance Profiling**
   - Detailed breakdown by component
   - Flamegraph generation

5. **Comparison Comments**
   - Compare against target branch
   - Show improvements since last release

## References

- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [PlatformIO Testing Guide](https://docs.platformio.org/en/latest/advanced/unit-testing/)
- [LightwaveOS Audio API](../api/AUDIO_CONTROL_API.md)
- [Testing Tools README](../../v2/test/tools/README.md)
