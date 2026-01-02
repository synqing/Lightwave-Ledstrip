# CI/CD Documentation

Continuous Integration and Continuous Deployment documentation for LightwaveOS.

## Overview

LightwaveOS uses GitHub Actions for automated testing and quality assurance. The CI/CD system ensures code quality, performance stability, and prevents regressions.

## Available Pipelines

### Audio Benchmark Regression Testing

**Status**: ✅ Active

**Purpose**: Detect performance regressions in audio processing code

**Triggers**:
- Pull requests modifying `v2/src/audio/**`
- Pushes to `main` or `feature/audio-*` branches
- Manual workflow dispatch

**Workflow**: `.github/workflows/audio_benchmark.yml`

**Documentation**:
- [Complete Guide](AUDIO_BENCHMARK_CI.md) - Full pipeline documentation
- [Quick Start](QUICK_START.md) - Fast reference for developers

**Key Features**:
- Automated performance testing on every audio code change
- Statistical regression detection with configurable thresholds
- PR comments with detailed benchmark results
- 30-day artifact retention for historical analysis
- Zero external dependencies (pure Python stdlib)

**Metrics Tracked**:
- Signal-to-Noise Ratio (SNR)
- False Trigger Rate
- CPU Load
- Processing Latency
- Memory Usage

## Future Pipelines

### Planned CI/CD Enhancements

1. **ESP32 Hardware Testing**
   - Automated firmware upload to test devices
   - Real hardware validation
   - LED strip connectivity testing

2. **Web Interface Testing**
   - Selenium/Playwright tests for web UI
   - Cross-browser compatibility
   - WebSocket integration tests

3. **Documentation Generation**
   - Automated API docs from code
   - Changelog generation
   - Release notes automation

4. **Release Automation**
   - Semantic versioning
   - Binary builds for multiple platforms
   - OTA update package generation

## Directory Structure

```
docs/ci_cd/
├── README.md                    # This file - CI/CD overview
├── AUDIO_BENCHMARK_CI.md        # Complete audio benchmark guide
└── QUICK_START.md               # Quick reference for developers

.github/workflows/
└── audio_benchmark.yml          # Audio benchmark workflow

v2/test/
├── baseline/
│   └── benchmark_baseline.json  # Reference metrics
├── tools/
│   ├── parse_benchmark_serial.py    # Metric extraction
│   ├── detect_regressions.py        # Regression detection
│   ├── requirements.txt             # Python dependencies (none)
│   ├── README.md                    # Tools documentation
│   ├── test_tools.sh                # Validation script
│   └── sample_test_output.log       # Example output
└── test_audio/
    ├── test_pipeline_benchmark.cpp  # Benchmark tests
    ├── AudioPipelineBenchmark.h     # Benchmark utilities
    └── TestSignalGenerator.h        # Signal generation
```

## Getting Started

### For Developers

If you're modifying audio code:

1. **Read**: [Quick Start Guide](QUICK_START.md)
2. **Run locally**: See quick commands in guide
3. **Push PR**: CI runs automatically
4. **Review results**: Check PR comment
5. **Fix regressions**: If detected

### For CI/CD Maintainers

1. **Read**: [Complete Guide](AUDIO_BENCHMARK_CI.md)
2. **Understand**: Architecture, thresholds, customization
3. **Monitor**: Workflow runs and success rates
4. **Maintain**: Update baselines after verified improvements

## CI/CD Philosophy

### Principles

1. **Fast Feedback** - Results in 3-5 minutes
2. **Clear Reports** - Human-readable PR comments
3. **Fail Fast** - Block merges on regressions
4. **Developer-Friendly** - Easy to run locally
5. **Zero Trust** - Verify performance, don't assume
6. **Transparent** - All metrics and thresholds documented

### Best Practices

- **Test locally first** - Faster iteration
- **Document trade-offs** - Explain intentional regressions
- **Update baselines carefully** - Only for genuine improvements
- **Monitor trends** - Watch for gradual degradation
- **Review artifacts** - Always check logs on failures

## Tool Validation

Before deploying CI/CD changes:

```bash
# Validate all tools work correctly
cd firmware/v2/test/tools
chmod +x test_tools.sh
./test_tools.sh

# Should output: ✓ All tests passed!
```

## Metrics and Monitoring

### Workflow Success Rate

Target: >95% success rate (excluding genuine regressions)

Monitor via: GitHub Actions dashboard

### Performance Baselines

Current baselines (as of 2025-12-27):
- SNR: 45.0 dB
- False Trigger Rate: 2.5%
- CPU Load: 35.0%
- Latency: 8.5 ms
- Memory Usage: 128.0 KB

Review quarterly or after major optimizations.

### CI Resource Usage

- **Free tier limit**: 2,000 minutes/month
- **Estimated usage**: ~400 minutes/month
- **Headroom**: 1,600 minutes (80%)

## Troubleshooting

### Common Issues

| Issue | Solution |
|-------|----------|
| Workflow doesn't trigger | Check file paths in `on.paths` |
| No metrics found | Verify test output format |
| False positives | Adjust thresholds or reduce variance |
| PR comment not posted | Check GitHub token permissions |
| Slow builds | Verify PlatformIO cache hit rate |

See [Complete Guide - Troubleshooting](AUDIO_BENCHMARK_CI.md#troubleshooting) for details.

## Contributing

### Adding New Metrics

1. Update test code to output metric
2. Add parse pattern to `parse_benchmark_serial.py`
3. Define threshold in `detect_regressions.py`
4. Add baseline value to `benchmark_baseline.json`
5. Document in this README

See [Customization Guide](AUDIO_BENCHMARK_CI.md#customization) for details.

### Modifying Workflows

1. Edit workflow file locally
2. Test with `act` (GitHub Actions local runner) if possible
3. Test in fork or feature branch first
4. Document changes in commit message
5. Monitor first few runs after merge

## Security

### Token Permissions

All workflows use minimal required permissions:
- `contents: read` - Read repository
- `issues: write` - Post PR comments
- `pull-requests: write` - Update PR status

No elevated privileges required.

### Dependency Security

Audio benchmark tools use **Python standard library only** - no external dependencies to audit or update.

### Secret Management

No secrets required for audio benchmark pipeline.

Future pipelines may use:
- `DISCORD_WEBHOOK` - Build notifications
- `OTA_DEPLOY_KEY` - Firmware deployment

## Performance

### Optimization Strategies

1. **Caching**: PlatformIO dependencies cached (~5 min savings)
2. **Parallel Steps**: Independent steps run concurrently
3. **Fail Fast**: Early exit on critical failures
4. **Minimal Artifacts**: Only essential files retained
5. **Local Testing**: Encourage pre-push validation

### Benchmark

Average workflow timings:
- Setup: 30-60s
- Build: 60-120s
- Test: 30-60s
- Analysis: 10-20s
- **Total**: 3-5 minutes

## Support

### Documentation

- [Audio Benchmark CI Guide](AUDIO_BENCHMARK_CI.md)
- [Quick Start Guide](QUICK_START.md)
- [Testing Tools README](../../v2/test/tools/README.md)

### GitHub Resources

- [Actions Documentation](https://docs.github.com/en/actions)
- [Workflow Syntax](https://docs.github.com/en/actions/using-workflows/workflow-syntax-for-github-actions)
- [PlatformIO CI Guide](https://docs.platformio.org/en/latest/integration/ci/index.html)

### Project Contact

For CI/CD questions or issues:
1. Check documentation first
2. Review workflow logs and artifacts
3. Create issue with `ci/cd` label
4. Tag maintainer if urgent

## Changelog

### 2025-12-27
- Initial audio benchmark CI/CD pipeline
- Parse and regression detection tools
- Comprehensive documentation
- Local validation scripts

---

**Note**: This is a living document. Update as CI/CD pipelines evolve.
