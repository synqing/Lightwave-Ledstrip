---
abstract: "Detailed handover for ongoing onset detector hardening on K1v2. Covers detector intent, landed code, native and hardware evidence, MabuTrace instrumentation, trace findings, and the exact next resume steps."
---

# Session Handover — 2026-03-25 Onset Hardening

## Scope

This handover is only for the standalone FFT onset detector work on the ESV11
audio path. It is the state needed for Claude Code to resume hardening,
validation, and any remaining onset-related implementation.

The current target is:

- bullet-proof `onsetEnv` and `onsetEvent`;
- reliable behaviour across multiple music fixtures;
- low false-trigger rate on K1v2 idle/silence;
- preserved runtime and LED transport safety.

## Project / Hardware Identity

- Repo: `Lightwave-Ledstrip`
- Working directory: `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip`
- Firmware dir: `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3`
- Board under test: K1v2 on `/dev/tty.usbmodem1101` and `/dev/cu.usbmodem1101`
- Verified MAC: `b4:3a:45:a5:87:f8`
- Production env for onset work: `esp32dev_audio_esv11_k1v2_32khz`
- Trace env for onset work: `esp32dev_audio_esv11_k1v2_32khz_trace`

Guardrails from `AGENTS.md` still apply:

- verify device by MAC before upload;
- K1v2 must keep `FEATURE_STATUS_STRIP_TOUCH=0`;
- treat `FastLED.show()` wire time as a safety invariant;
- after any flash, run serial status checks and confirm no panics, sane show
  timing, `showSkips=0`, stable heap/stack.

## What This Detector Is For

The detector is meant to provide a backend-agnostic onset semantics layer:

- `onsetFlux`: raw novelty signal;
- `onsetEnv`: continuous onset-strength envelope;
- `onsetEvent`: sparse transient pulse stream;
- `onsetBassFlux` / `onsetMidFlux` / `onsetHighFlux`: band-aware onset energy;
- `kickTrigger` / `snareTrigger` / `hihatTrigger`: percussion-like discrete triggers.

The objective is not “interesting debug numbers”. The detector exists so the
rest of the visual system can consume reliable transient semantics.

## Landed Work Before This Handover

### 1. Standalone FFT onset detector added and wired into ESV11

Relevant files:

- `src/audio/onset/OnsetDetector.h`
- `src/audio/onset/OnsetDetector.cpp`
- `src/audio/AudioActor.h`
- `src/audio/AudioActor.cpp`

Core model:

- 1024-point FFT, every hop
- log-spectral flux
- median-based adaptive threshold
- causal peak picker
- per-band bass/mid/high flux with kick/snare/hihat triggers

### 2. Early detector fixes already landed

Important changes already made before the final unvalidated patch:

- switched from additive-only thresholding to multiplicative thresholding;
- reduced threshold floor from `2.5` to `0.01` after live K1v2 proved real
  `onsetFlux` is about `0.0-0.1`, not `18-50`;
- corrected hi-hat band from `1-4 kHz` to `6-12 kHz`;
- gated onset detection using raw hop RMS from `sample_history[]`, not the
  perceptually expanded contract RMS;
- added a detector-local ambient noise follower;
- added detector activity gating;
- kept the previous spectrum current while gated closed, removing stale-spectrum
  bursts on reopen;
- added per-hop self-timing and merged onset outputs into `ControlBusFrame`.

### 3. Native regression coverage added

Relevant files:

- `test/test_onset/test_onset_detector.cpp`
- `test/test_onset_corpus/test_onset_music_corpus.cpp`
- `platformio.ini`

Native test targets:

- `native_test_onset`
- `native_test_onset_corpus`

### 4. Hardware onset capture path added

Relevant files:

- `tools/led_capture/onset_capture_suite.py`
- `docs/ONSET_CAPTURE_WORKFLOW.md`
- `docs/CAPTURE_TEST_SUITES.md`

This provides fixed silence/click/music fixtures for K1 hardware validation.

### 5. MabuTrace instrumentation added

Relevant files:

- `src/audio/onset/OnsetDetector.h`
- `src/audio/onset/OnsetDetector.cpp`
- `src/audio/AudioActor.h`
- `src/audio/AudioActor.cpp`
- `platformio.ini`
- `docs/debugging/MABUTRACE_GUIDE.md`

Instrumented onset state:

- `input_rms`
- `noise_floor`
- `activity`
- `gate_flags`
- `flux`
- `onset_env`
- `onset_event`
- `bass/mid/high flux`
- `process_us`

Trace signals:

- span: `onset_detect`
- counters:
  - `onset_input_rms`
  - `onset_noise_floor`
  - `onset_activity`
  - `onset_gate_flags`
  - `onset_flux`
  - `onset_env`
  - `onset_event_strength`
  - `onset_bass_flux`
  - `onset_mid_flux`
  - `onset_high_flux`
  - `onset_process_us`
- instants:
  - `ONSET_EVENT`
  - `ONSET_KICK`
  - `ONSET_SNARE`
  - `ONSET_HIHAT`

Trace env:

- `esp32dev_audio_esv11_k1v2_32khz_trace`

## Verified Results Before The Final Patch

### Native tests

These passed after the detector hardening and trace instrumentation:

```bash
cd firmware-v3
pio test -e native_test_onset
pio test -e native_test_onset_corpus
```

Corpus result from the stricter 36-track gate:

- `eventful=36/36`
- `dead=0`
- `saturated=0`
- `maxEventRate=5.00 Hz`
- `maxActive=8.67%`

### Hardware results before trace-guided retune

On K1v2, the detector improved materially but was not yet closed:

- silence false triggers dropped from roughly `3.28 Hz` to `1.79 Hz`;
- silence active fraction dropped from roughly `23.27%` to `7.88%`;
- click fixture remained clearly eventful and usable;
- transport stayed healthy:
  - sane `show_time`
  - `showSkips=0`
  - stable heap
  - no panics
  - no RMT failures

### Important harness finding

The metadata-only serial capture path did not honour requested `120 FPS` onset
capture. A nominal `120 FPS` request collapsed to roughly `15 FPS` actual, so
interval/CV judgement on one-hop onset pulses is not trustworthy from metadata
capture alone.

That is why MabuTrace was the correct next step.

## MabuTrace Evidence Captured In This Session

Trace firmware was built and uploaded successfully to the real K1v2:

```bash
cd firmware-v3
pio run -e esp32dev_audio_esv11_k1v2_32khz_trace -t upload --upload-port /dev/tty.usbmodem1101
```

Upload confirmed the expected MAC:

- `b4:3a:45:a5:87:f8`

### Idle trace

Captured to:

- `/tmp/mabutrace_onset/idle_trace_raw.txt`
- `/tmp/mabutrace_onset/idle_trace.json`

Key summary from the decoded trace:

- onset counter samples: about `150`
- `onset_detect` span count: `301`
- gate flags distribution:
  - `2` (`ONSET_GATE_ACTIVITY`) on `142` frames
  - `0` on `8` frames
- `ONSET_EVENT` instants during idle: `8`

Idle trace statistics:

- `onset_input_rms`
  - median `3755` (`0.003755`)
  - p95 `6221` (`0.006221`)
  - max `7442` (`0.007442`)
- `onset_noise_floor`
  - median `3640` (`0.003640`)
  - p95 `4067` (`0.004067`)
- `onset_activity`
  - median `0`
  - p95 `68` (`0.068`)
  - max `284` (`0.284`)
- `onset_flux`
  - p95 `150` (`0.150`)
  - max `3458` (`3.458`)
- `onset_env`
  - p95 `45` (`0.045`)
  - max `3025` (`3.025`)

Per-event idle snapshots showed:

- all `8` idle onset events happened with gate flags `0`, not while activity-gated shut;
- idle event activity values ranged from `0.068` to `0.284`;
- the detector was not “always open”, but it reopened on a small number of
  real frontend noise excursions and produced large full-band/high-band spikes.

Interpretation:

- the activity gate is doing useful work;
- the remaining false positives are not a stale-spectrum bug any more;
- K1v2 idle RMS still sits too close to the current activity opening threshold.

### Click120 trace

Fixture audio used:

- `/tmp/mabutrace_onset/click120.wav`

Captured to:

- `/tmp/mabutrace_onset/click120_trace_raw.txt`
- `/tmp/mabutrace_onset/click120_trace.json`

Key summary:

- `ONSET_EVENT`: `12`
- `ONSET_KICK`: `4`
- `ONSET_SNARE`: `3`
- `ONSET_HIHAT`: `3`

Click trace statistics:

- `onset_input_rms`
  - median `5797` (`0.005797`)
  - p95 `11312` (`0.011312`)
  - max `20518` (`0.020518`)
- `onset_noise_floor`
  - median `4777` (`0.004777`)
  - p95 `5528` (`0.005528`)
- `onset_activity`
  - median `0`
  - p95 `517` (`0.517`)
  - max `1000` (`1.0`)
- `onset_flux`
  - p95 `7295` (`7.295`)
  - max `235321` (`235.321`)
- `onset_env`
  - p95 `6143` (`6.143`)
  - max `208270` (`208.270`)

Per-event click snapshots showed a clear separation from idle on stronger event
frames:

- event activity commonly `0.20+`, up to `1.0`;
- strongest event frames had RMS `0.010-0.020`;
- a few weaker click-related events still occurred in the `0.10-0.25` activity
  range, which suggests some over-firing or room/ring-down behaviour.

Interpretation:

- trace evidence supports tightening the activity gate;
- there is enough margin between the strongest real click frames and the idle
  reopenings to justify a conservative retune;
- the click trace also suggests onset events may still be a bit too eager, but
  the first correction should be gate tightening, not a major redesign.

## Unvalidated Patch Applied Right Before This Handover

These edits were made but were **not yet built, tested, or flashed** because
the work switched to preparing this handover.

### A. Tightened detector activity gate

File:

- `src/audio/onset/OnsetDetector.h`

Changes:

- `activityGateK`: `1.5f -> 1.8f`
- `activityCutoff`: `0.05f -> 0.10f`

Rationale:

- idle trace showed K1v2 idle p95 RMS sitting too close to the `1.5x` gate start;
- click trace showed legitimate transient frames still clearing materially
  higher activity levels;
- this should reduce idle reopenings and likely reduce click over-firing.

### B. Fixed ESV11 one-shot status output

File:

- `src/audio/AudioActor.cpp`

Change:

- `AudioActor::printStatus()` for the **ESV11 branch** now prints the onset
  gate/debug line, not just the old 3-line RMS/BPM summary.

Rationale:

- during this session it became clear that the new onset status line existed in
  the Goertzel branch but not in the ESV11 branch actually being tuned.

## Exact Resume Plan For The Next Claude Code Session

### Phase 1: Validate the untested patch

Run in this order:

```bash
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3
pio test -e native_test_onset
pio test -e native_test_onset_corpus
pio run -e esp32dev_audio_esv11_k1v2_32khz_trace
python -m esptool --port /dev/tty.usbmodem1101 read-mac
pio run -e esp32dev_audio_esv11_k1v2_32khz_trace -t upload --upload-port /dev/tty.usbmodem1101
```

Then confirm serial:

```bash
pio device monitor -b 115200 -p /dev/cu.usbmodem1101
```

Use:

- `adbg status`
- `trace`

Expected:

- ESV11 `adbg status` now includes the full onset line;
- no panics;
- sane LED show timing;
- `showSkips=0`.

### Phase 2: Re-run trace comparison

Reproduce the same two traces:

1. true idle
2. `click120`

Decode and compare against the baseline numbers above.

Primary success criteria for the retune:

- idle `ONSET_EVENT` count drops materially below `8` in the same trace window;
- idle `gate_flags=0` openings become rarer than the baseline `8/150`;
- click trace remains clearly eventful;
- click event strength stays materially above idle.

### Phase 3: Re-baseline the hardware onset suite

After the trace retune looks sane, re-run:

```bash
python3 tools/led_capture/onset_capture_suite.py --serial /dev/cu.usbmodem1101 --fixtures silence,click120
```

Then inspect:

- `summary.json`
- `report.txt`
- saved `.npz`
- saved dashboard PNGs

Do not trust interval CV unless the capture cadence issue has been fixed or
explicitly compensated for.

### Phase 4: If idle still reopens too often

Use this decision order:

1. stay trace-first;
2. inspect `onset_input_rms`, `onset_noise_floor`, `onset_activity`,
   `onset_gate_flags`, `onset_flux`, `onset_env` around each idle event;
3. only then retune.

Most likely next knobs, in order:

1. `activityGateK`
2. `activityCutoff`
3. `activityRangeK`
4. `noiseFloorRise` / `noiseFloorFall`

Avoid jumping straight to:

- threshold multiplier retunes;
- major peak-picker redesign;
- per-band trigger redesign;
- backend-wide AGC changes.

Those are not the current bottleneck.

### Phase 5: If click remains too eager after idle is fixed

Then inspect whether the remaining problem is:

- multiple onset peaks per acoustic click;
- room/speaker/mic ring-down;
- detector event refractory too permissive.

At that point it becomes reasonable to evaluate:

- `peakWait`
- a modest event-only activity minimum
- event-strength floor independent of continuous `onsetEnv`

Do that only after confirming idle has improved.

## Files Most Relevant To Resume

- `src/audio/onset/OnsetDetector.h`
- `src/audio/onset/OnsetDetector.cpp`
- `src/audio/AudioActor.h`
- `src/audio/AudioActor.cpp`
- `test/test_onset/test_onset_detector.cpp`
- `test/test_onset_corpus/test_onset_music_corpus.cpp`
- `tools/led_capture/onset_capture_suite.py`
- `docs/ONSET_CAPTURE_WORKFLOW.md`
- `docs/debugging/MABUTRACE_GUIDE.md`
- `platformio.ini`
- `task_plan.md`
- `findings.md`
- `progress.md`

## Important Local Artifacts From This Session

These are not repo files, but they are the best short-term evidence set if they
still exist:

- `/tmp/mabutrace_onset/idle_trace_raw.txt`
- `/tmp/mabutrace_onset/idle_trace.json`
- `/tmp/mabutrace_onset/click120.wav`
- `/tmp/mabutrace_onset/click120_trace_raw.txt`
- `/tmp/mabutrace_onset/click120_trace.json`

If they are gone, regenerate them. Do not spend time trying to recover them.

## Current Known Risks

- The repo is dirty well beyond onset work. Do not revert unrelated changes.
- The latest activity-gate retune and ESV11 `adbg status` patch are not yet
  validated.
- Metadata-only serial capture is still not trustworthy for hop-level interval
  analysis.
- Click fixtures may still over-fire slightly even after idle is improved.

## Short Version

The detector is no longer dead and no longer obviously broken by stale state.
The remaining issue is that K1v2 idle noise still reopens the detector
occasionally. MabuTrace proved that directly. The next agent should validate
the untested gate-tightening patch, compare fresh idle/click traces against the
baseline above, then re-run the hardware onset suite. Keep the loop:

trace -> interpret -> retune -> validate

Do not go back to blind threshold guessing.
