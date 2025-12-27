# Audio Benchmark CI/CD Setup Checklist

Use this checklist to verify the audio benchmark regression testing pipeline is properly configured.

## Pre-Deployment Checklist

### ✅ Files Created

- [ ] `.github/workflows/audio_benchmark.yml` - Main workflow
- [ ] `v2/test/tools/parse_benchmark_serial.py` - Metric parser
- [ ] `v2/test/tools/detect_regressions.py` - Regression detector
- [ ] `v2/test/baseline/benchmark_baseline.json` - Reference metrics
- [ ] `v2/test/tools/README.md` - Tools documentation
- [ ] `v2/test/tools/requirements.txt` - Dependencies (empty)
- [ ] `v2/test/tools/test_tools.sh` - Validation script
- [ ] `v2/test/tools/sample_test_output.log` - Example output
- [ ] `docs/ci_cd/AUDIO_BENCHMARK_CI.md` - Complete guide
- [ ] `docs/ci_cd/QUICK_START.md` - Developer quick reference
- [ ] `docs/ci_cd/README.md` - CI/CD overview

### ✅ File Permissions

```bash
# Make scripts executable
chmod +x v2/test/tools/parse_benchmark_serial.py
chmod +x v2/test/tools/detect_regressions.py
chmod +x v2/test/tools/test_tools.sh
```

### ✅ Local Tool Validation

```bash
# Run validation tests
cd v2/test/tools
./test_tools.sh

# Expected output: ✓ All tests passed!
```

### ✅ Baseline Metrics

- [ ] Baseline values are realistic for current platform
- [ ] All required metrics present in baseline
- [ ] Metadata fields populated (`_metadata`, `_thresholds`)

Required metrics:
- `snr_db`
- `false_trigger_rate`
- `cpu_load_percent`
- `latency_ms`
- `memory_usage_kb`

### ✅ GitHub Repository Settings

- [ ] Actions enabled (Settings → Actions → Allow all actions)
- [ ] Workflow permissions set (Settings → Actions → Workflow permissions)
  - [x] Read and write permissions
  - [ ] Read repository contents and packages permissions only
- [ ] Issues enabled (for PR comments)
- [ ] Pull requests enabled

### ✅ Test Code

- [ ] `v2/test/test_audio/test_pipeline_benchmark.cpp` exists
- [ ] Test outputs metrics in parseable format
- [ ] Test runs successfully: `pio test -e native_test -f test_pipeline_benchmark`

### ✅ Workflow Syntax

```bash
# Validate workflow YAML syntax (requires act or online validator)
# Option 1: Using act (local GitHub Actions runner)
act -n -W .github/workflows/audio_benchmark.yml

# Option 2: Manual validation
# Go to: https://rhysd.github.io/actionlint/
# Paste workflow contents
```

## Post-Deployment Checklist

### ✅ First Workflow Run

- [ ] Trigger workflow manually (Actions → Audio Benchmark → Run workflow)
- [ ] Workflow completes successfully
- [ ] Artifacts uploaded (benchmark-results)
- [ ] Job summary generated

### ✅ Artifact Verification

Download `benchmark-results` artifact and verify:
- [ ] `current_benchmark.json` contains all metrics
- [ ] `regression_report.md` formatted correctly
- [ ] `benchmark_output.log` has test output

### ✅ PR Comment Testing

Create test PR modifying audio code:
- [ ] Workflow triggers automatically
- [ ] Bot posts comment on PR
- [ ] Comment includes formatted results table
- [ ] Comment updates if re-run
- [ ] No duplicate comments created

### ✅ Regression Detection

Create PR with intentional regression (modify baseline temporarily):
- [ ] Workflow detects regression
- [ ] Workflow fails with exit code 1
- [ ] PR comment shows failure status
- [ ] Metrics exceeding thresholds highlighted

### ✅ Performance

- [ ] Workflow completes in <10 minutes
- [ ] PlatformIO cache working (check logs)
- [ ] No timeout errors

## Validation Commands

### Test Parser Locally

```bash
cd v2/test/tools

# Test with sample output
python3 parse_benchmark_serial.py sample_test_output.log --platformio --format summary

# Expected: Summary with all metrics displayed
```

### Test Regression Detector Locally

```bash
cd v2/test/tools

# Test with matching metrics (should pass)
python3 detect_regressions.py \
  ../baseline/benchmark_baseline.json \
  ../baseline/benchmark_baseline.json

# Expected: Exit 0, all metrics PASS
```

### Test End-to-End Locally

```bash
cd v2

# Run benchmark
pio test -e native_test -f test_pipeline_benchmark --verbose > benchmark_output.log

# Parse results
python test/tools/parse_benchmark_serial.py \
  benchmark_output.log \
  --platformio \
  -o test/results/current_benchmark.json

# Detect regressions
python test/tools/detect_regressions.py \
  test/baseline/benchmark_baseline.json \
  test/results/current_benchmark.json

# Expected: Exit 0 if no regressions
```

## Common Issues and Fixes

### Issue: Workflow doesn't trigger on PR

**Check**:
- [ ] File paths in `on.paths` are correct
- [ ] Branch protection rules allow Actions
- [ ] Actions are enabled in repo settings

**Fix**: Update workflow triggers or repository settings

### Issue: "No benchmark metrics found"

**Check**:
- [ ] Test actually ran (check for test name in logs)
- [ ] Output format matches regex patterns
- [ ] `--platformio` flag used when parsing

**Fix**: Add debug output to test or update regex patterns

### Issue: PR comment not posted

**Check**:
- [ ] GitHub token has write permissions
- [ ] Issues are enabled in repo
- [ ] Workflow has `pull-requests: write` permission

**Fix**: Update repository settings or workflow permissions

### Issue: False positive regressions

**Check**:
- [ ] Baseline appropriate for platform
- [ ] Tests have low variance (run 3-5 times)
- [ ] Thresholds reasonable

**Fix**: Update baseline or adjust thresholds

## Rollback Plan

If CI/CD causes issues:

1. **Disable workflow**:
   ```bash
   # Rename workflow file
   mv .github/workflows/audio_benchmark.yml .github/workflows/audio_benchmark.yml.disabled
   ```

2. **Revert changes**:
   ```bash
   git revert <commit-hash>
   git push
   ```

3. **Investigate**: Review logs, artifacts, and error messages

4. **Fix and re-enable**: Correct issues and rename workflow back

## Monitoring

### Regular Checks

**Weekly**:
- [ ] Review workflow success rate (target: >95%)
- [ ] Check for increasing failure patterns
- [ ] Verify artifact retention working

**Monthly**:
- [ ] Review baseline metrics
- [ ] Update documentation if needed
- [ ] Check CI resource usage vs limits

**Quarterly**:
- [ ] Comprehensive baseline review
- [ ] Threshold tuning based on historical data
- [ ] Performance optimization review

## Sign-Off

- [ ] All checklist items completed
- [ ] Local validation passed
- [ ] First workflow run successful
- [ ] PR comment testing passed
- [ ] Documentation reviewed
- [ ] Team notified of new CI/CD pipeline

**Date**: _____________

**Validated by**: _____________

**Notes**:

---

## Quick Validation

Run this one-liner to validate everything:

```bash
cd v2/test/tools && \
./test_tools.sh && \
echo "✅ Tools validated" && \
cd ../../.. && \
pio test -e native_test -f test_pipeline_benchmark --verbose > /tmp/benchmark_output.log 2>&1 && \
python3 v2/test/tools/parse_benchmark_serial.py /tmp/benchmark_output.log --platformio --format summary && \
python3 v2/test/tools/detect_regressions.py v2/test/baseline/benchmark_baseline.json <(python3 v2/test/tools/parse_benchmark_serial.py /tmp/benchmark_output.log --platformio) && \
echo "✅ End-to-end validation passed"
```

If all steps pass, the CI/CD pipeline is ready for deployment!
