# Onset Quarantine Matrix — 2026-03-25

## Purpose

Separate approved detector changes from reverted tuning knobs, and record the
current verification state of the remaining onset patch set.

## Reverted

These were treated as unapproved live-tuning edits and reverted:

- `OnsetDetector::Config::activityGateK`
  - reverted from `1.8f` back to `1.5f`
- `OnsetDetector::Config::activityCutoff`
  - reverted from `0.10f` back to `0.05f`

Rationale:

- these are trace-driven tuning knobs, not part of the research-backed detector
  architecture change;
- bundling them with the median/normalisation patch made attribution unclear.

## Quarantined And Kept For Verification

### Detector-model changes

File:

- `src/audio/onset/OnsetDetector.h`
- `src/audio/onset/OnsetDetector.cpp`

Changes:

- full-band adaptive threshold changed from mean/SMA semantics to
  median-based thresholding;
- threshold config renamed to `thresholdFrames`;
- threshold multiplier now tuned for median semantics;
- per-band trigger inputs are normalised by bin count before EMA/thresholding;
- per-band threshold constants changed:
  - kick `1.2 -> 0.8`
  - snare `1.4 -> 1.0`
  - hihat `1.0 -> 1.2`
- per-band EMA alphas split by instrument:
  - kick `0.015`
  - snare `0.010`
  - hihat `0.005`

### Non-behavioural visibility / tooling changes

Files:

- `src/audio/AudioActor.cpp`
- `src/audio/AudioActor.h`
- `platformio.ini`
- `docs/debugging/MABUTRACE_GUIDE.md`

Changes:

- ESV11 `adbg status` now prints onset gate/debug state;
- onset MabuTrace env remains available:
  - `esp32dev_audio_esv11_k1v2_32khz_trace`
- onset trace counters/instants remain documented.

### Harness compatibility fix

File:

- `test/test_onset_corpus/test_onset_music_corpus.cpp`

Change:

- updated stale `preAvgFrames` diagnostic print to `thresholdFrames` so the
  corpus harness builds against the median-threshold detector.

## Verification Results

### Passed

```bash
cd firmware-v3
pio test -e native_test_onset_corpus
pio run -e esp32dev_audio_esv11_k1v2_32khz_trace
```

Results:

- `native_test_onset_corpus`: passed
- `esp32dev_audio_esv11_k1v2_32khz_trace`: build passed

### Failed

```bash
cd firmware-v3
pio test -e native_test_onset
```

Failure:

- `test_bursts_produce_sparse_onset_events`
- failing assertion:
  - `summary.positiveEnvCount < 24`
  - message: `Thresholding should not leave onset_env high on most frames`

Interpretation:

- the quarantined detector patch is not dead or uncompilable;
- it clears the broader music-corpus gate;
- but it no longer satisfies the existing synthetic sparse-burst liveliness
  regression, which means either:
  - the detector became genuinely too sticky on bursts, or
  - the synthetic sparsity expectation needs recalibration for the new median
    threshold model.

That question is unresolved. Do not treat the quarantined patch as production-
ready until this is resolved.

## Recommended Next Step

Measure the failing native sparse-burst case before changing code again:

1. print or inspect `eventCount`, `positiveEnvCount`, `maxFlux`, `maxEnv`, and
   event hop positions for `fillCarrierBurstHop`;
2. decide whether the regression expectation is still valid under the new
   median-threshold detector;
3. only then either:
   - adjust detector behaviour, or
   - update the regression if the new behaviour is demonstrably better.
