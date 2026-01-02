# Audio Benchmark CI/CD - Quick Start Guide

Fast reference for developers working with the audio benchmark regression testing pipeline.

## TL;DR

When you modify audio code (`v2/src/audio/**`), the CI will:
1. Run performance benchmarks
2. Compare against baseline metrics
3. Post results on your PR
4. Fail if performance regresses beyond thresholds

## Quick Commands

### Run Locally Before Pushing

```bash
# In v2/ directory
pio test -e native_test -f test_pipeline_benchmark --verbose > benchmark_output.log

# Parse and check
python test/tools/parse_benchmark_serial.py benchmark_output.log --platformio --format summary
python test/tools/detect_regressions.py test/baseline/benchmark_baseline.json <(python test/tools/parse_benchmark_serial.py benchmark_output.log --platformio)
```

### One-Liner Local Test

```bash
cd v2 && pio test -e native_test -f test_pipeline_benchmark --verbose 2>&1 | tee /tmp/bench.log && python test/tools/parse_benchmark_serial.py /tmp/bench.log --platformio --format summary && python test/tools/detect_regressions.py test/baseline/benchmark_baseline.json <(python test/tools/parse_benchmark_serial.py /tmp/bench.log --platformio)
```

## What Gets Tested

| Metric | Baseline | Warning | Failure | What It Means |
|--------|----------|---------|---------|---------------|
| SNR | 45 dB | -5% | -10% | Audio quality - keep high |
| False Triggers | 2.5% | +50% | +100% | Detection accuracy - keep low |
| CPU Load | 35% | +20% | +40% | Processing overhead - keep low |
| Latency | 8.5 ms | +15% | +30% | Response time - keep low |
| Memory | 128 KB | +25% | +50% | Heap usage - keep low |

## Reading PR Comments

### Pass ✅

```
## Audio Benchmark Results ✅

Status: PASS - All metrics within acceptable ranges

| Metric | Change | Status |
|--------|--------|--------|
| snr_db | +0.4%  | ✅     |
```

**Action**: None needed, you're good to merge!

### Warning ⚠️

```
## Audio Benchmark Results ⚠️

Status: WARNING - Performance approaching regression thresholds

| Metric | Change | Status |
|--------|--------|--------|
| cpu_load | +22% | ⚠️      |
```

**Action**: Review the change, consider optimization. Can still merge but be aware.

### Failure ❌

```
## Audio Benchmark Results ❌

Status: REGRESSION DETECTED - Performance degraded beyond acceptable thresholds

| Metric | Change | Status |
|--------|--------|--------|
| snr_db | -15.6% | ❌     |
```

**Action**: Fix the regression or justify the trade-off. Cannot merge until resolved.

## Common Scenarios

### "My PR added a feature and CPU load increased slightly"

**Expected**: Small increases OK if within warning threshold

**If exceeds warning**: Document why in PR description, consider optimization

**If exceeds failure**: Must optimize or adjust algorithm

### "The baseline seems wrong for my platform"

**Short-term**: Add comment in PR explaining discrepancy

**Long-term**: Update baseline via workflow dispatch after verification

### "Tests are flaky/inconsistent"

**Check**:
1. Run tests 3-5 times locally
2. Calculate variance
3. If high variance, adjust thresholds or improve test stability

### "I need to add a new performance metric"

See [Customization Guide](AUDIO_BENCHMARK_CI.md#adding-new-metrics) for step-by-step.

## Debugging Failed Tests

### Step 1: Download Artifacts

Go to Actions → Failed workflow → Scroll to bottom → Download `benchmark-results`

### Step 2: Check Raw Output

Look at `benchmark_output.log` - did test actually run?

### Step 3: Parse Locally

```bash
python v2/test/tools/parse_benchmark_serial.py benchmark_output.log --platformio --format summary
```

### Step 4: Compare

```bash
python v2/test/tools/detect_regressions.py \
  v2/test/baseline/benchmark_baseline.json \
  extracted_metrics.json \
  --format console
```

## Bypassing CI (Emergency Only)

If you absolutely must bypass for emergency hotfix:

1. **Don't modify audio code in hotfix** (triggers different CI)
2. **Or**: Add `[skip ci]` to commit message (use sparingly!)
3. **Or**: Temporarily adjust thresholds (requires review)

**Never merge with failing benchmarks without investigation!**

## Getting Help

- **Full docs**: [docs/ci_cd/AUDIO_BENCHMARK_CI.md](AUDIO_BENCHMARK_CI.md)
- **Tools README**: [v2/test/tools/README.md](../../v2/test/tools/README.md)
- **Tool validation**: Run `v2/test/tools/test_tools.sh`

## Workflow File Locations

```
.github/workflows/audio_benchmark.yml          # Main CI workflow
v2/test/tools/parse_benchmark_serial.py        # Metric extraction
v2/test/tools/detect_regressions.py            # Regression detection
v2/test/baseline/benchmark_baseline.json       # Reference metrics
v2/test/test_audio/test_pipeline_benchmark.cpp # Actual test code
```

## Environment Setup

Works with:
- Python 3.11+ (local) or 3.11 (CI)
- PlatformIO latest
- No external Python dependencies
- Standard library only

## Tips

1. **Run locally first** - Saves CI minutes and iteration time
2. **Check artifacts** - Always download and review on failures
3. **Document trade-offs** - If performance changes are intentional
4. **Update baseline carefully** - Only after verified improvements
5. **Watch trends** - Small regressions accumulate over time

## Performance Expectations

### Workflow Runtime

- **Typical**: 3-5 minutes
- **With cache miss**: 8-10 minutes
- **First run**: 10-15 minutes

### Local Test Runtime

- **Native test**: 10-30 seconds
- **Full analysis**: <1 minute total

## Thresholds Quick Reference

```python
# Higher is better
SNR:     Warning: -5%,  Failure: -10%

# Lower is better
False Triggers: Warning: +50%,  Failure: +100%
CPU Load:       Warning: +20%,  Failure: +40%
Latency:        Warning: +15%,  Failure: +30%
Memory:         Warning: +25%,  Failure: +50%
```

## Example PR Flow

```
1. You: Push PR with audio optimizations
2. CI:  Runs benchmarks (3 min)
3. CI:  Posts comment with results
4. You: Review metrics in PR comment
5. You: Address any regressions if needed
6. Reviewer: Checks code + performance
7. You: Merge when passing ✅
```

That's it! Questions? Check the [full documentation](AUDIO_BENCHMARK_CI.md).
