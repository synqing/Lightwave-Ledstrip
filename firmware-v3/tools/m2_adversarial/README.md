---
abstract: "M2 ESV11 32 kHz adversarial robustness measurement campaign. Five signal classes (ambient, rubato, syncopation, sub-bass drone, pure sine) driven through a stand-alone harness that reuses the vendored ESV11 pipeline. Per-frame tempoConfidence and tempoPhase trajectories logged; pass/fail thresholds per Topology_Reconciliation §6 item 4. Read before running M2 measurement passes or proposing changes to the ESV11 tempo selector."
---

# M2 — ESV11 32 kHz adversarial test set

This directory contains the artefacts that let Captain commission the M2 measurement run gating Phase 2 doctrine (per `docs/research/synergy-topology/Topology_Reconciliation.md` §6 item 4).

> **NOTE — measurement-only.** This directory contains zero firmware-source modifications. The harness is a stand-alone runner that reuses the vendored ESV11 translation units (`EsV11Shim.cpp`, `EsV11Buffers.cpp`) plus the 32 kHz shim header, compiled directly with `clang++`. It is NOT registered in `platformio.ini`.

## What this measures

Five adversarial signal classes that stress beat tracking edges that Topology_Reconciliation §6 item 4 calls out as untested:

| Class | Generator | Duration | Ground truth | Pass rule |
|---|---|---|---|---|
| Ambient (Brian Eno class) | LFO-modulated low-pass pink noise | 60 s | no beat | `tempo_confidence < 0.3` in >= 90% of frames |
| Rubato (live recording class) | Click track sweeping 120 -> 80 -> 110 BPM | 60 s | drifting beat | re-acquire phase within <= 4 bars after each tempo change |
| Heavy syncopation (Aphex Twin class) | 110 BPM 16th-grid with 5/16 beats dropped | 60 s | beat at 110 BPM | phase coherence >= 75% |
| Sub-bass drone | Pure 40 Hz sine | 30 s | no beat | 0 beat ticks reported |
| Sine-wave-only | Pure 1 kHz sine | 30 s | no beat | `tempo_confidence < 0.2` in >= 95% of frames |

Full numerical thresholds, the failure-mode interpretation, and known rule-vs-firmware caveats live in `expected_results.md`.

## Layout

```
firmware-v3/tools/m2_adversarial/
  README.md                         (this file)
  expected_results.md               (per-class thresholds + interpretation guide)
  signals/                          (generated WAVs — 32 kHz mono int16 PCM)
    ambient_drone_60s.wav
    rubato_drift_60s.wav
    syncopation_110bpm_60s.wav
    subbass_40hz_30s.wav
    sine_1khz_30s.wav
  scripts/
    m2_harness.cpp                  (stand-alone per-frame trajectory logger)
    m2_run.py                       (orchestrator: gen + build + run + grade)
    m2_harness                      (built binary — gitignored)
  results/                          (generated; gitignored)
    m2_results.json
    m2_results.md
    trajectory_<signal>.json
```

## Verified working environment

`native_test_esv11_music_corpus_32khz` builds and links cleanly on this machine
(2026-04-27, `pio test --without-uploading --without-testing`). `native_test_esv11_music_32khz` also builds. **Both confirmed working.**

The broken environment referenced in task #8 is `native_test` (the platform-native FastLED webserver-mock harness) — it is **unrelated** to ESV11 corpus tests. M2 is unblocked on the build front.

## How to run

Prerequisites: Python 3.10+, `numpy`, `scipy`, `clang++` or `g++` on PATH. The script self-detects the compiler.

```bash
cd firmware-v3/tools/m2_adversarial/scripts

# Full sweep — generate signals (idempotent), build harness, run all 5 signals,
# emit results/m2_results.json + results/m2_results.md.
python3 m2_run.py

# Force-regenerate WAVs (e.g. after editing generators in m2_run.py).
python3 m2_run.py --regen

# Build harness only.
python3 m2_run.py --build-only

# Run a single signal class without rebuilding.
python3 m2_run.py --no-build --signal sine

# Inspect a per-signal trajectory.
jq '.summary' results/trajectory_sine.json
```

The harness binary is **not** the same shape as `pio test` — it takes a single
WAV path on the command line and emits a JSON object on stdout. There is **no**
clean way to feed arbitrary WAVs through the existing `pio test` ESV11 envs
without editing `platformio.ini` or adding a new test source under `test/`,
which the M2 task is scoped to avoid. The stand-alone harness is the correct
escape hatch — it links against the same vendored ESV11 TUs the existing
tests use, so behaviour is byte-identical to `native_test_esv11_music_corpus_32khz`.

## Why a stand-alone harness instead of `pio test`?

1. **Scope constraint.** M2 was scoped tools/-only; `pio test` requires a new
   `[env:...]` block in `platformio.ini`.
2. **Per-frame logging.** The existing `test_esv11_music`/`_corpus` harnesses
   emit only the final BPM + confidence after fork(). The M2 thresholds need
   per-frame `tempo_confidence` and `tempi[bin].phase` trajectories — that
   requires a different output shape.
3. **Compile-once, run-many.** A stand-alone binary takes ~3 s to build and
   subsequent signal runs are instant. `pio test` rebuilds the world per env.

If Captain later wants this folded into the regular `pio test` flow (e.g. as
`native_test_esv11_adversarial_32khz`), the wrapper recipe is in
`expected_results.md` -> "Future work — folding into pio test".

## Build artefacts the harness depends on

- `firmware-v3/src/audio/backends/esv11/vendor/EsV11Shim.cpp`
- `firmware-v3/src/audio/backends/esv11/vendor/EsV11Buffers.cpp`
- `firmware-v3/src/audio/backends/esv11/vendor/{global_defines,microphone,goertzel,vu,tempo,utilities_min}.h`
- `firmware-v3/src/audio/backends/esv11/EsV11_32kHz_Shim.h` (`-include`d for 32 kHz mode)

If any of these change shape (e.g. new globals added to `tempo.h`), the
harness's `esv11_reset_state()` may need to mirror the change. See
`scripts/m2_harness.cpp` lines starting around `// ESV11 init`.

## Hard constraints honoured

- No edits to `firmware-v3/src/`. Harness is `tools/`-only.
- No edits to `platformio.ini`. Stand-alone build via `clang++`.
- British English in all docs.
- Per-frame trajectory storage is bounded (one JSON per signal, ~1 MB each).

## Initial baseline run (2026-04-27, snapshot)

A first dry run was executed to confirm the pipeline end-to-end. Three of five
classes failed against the proposed Topology_Reconciliation thresholds. **At
least two of those failures are believed to be threshold/rule artefacts rather
than firmware regressions.** See `expected_results.md` -> "Baseline observations
and threshold caveats" for the per-class breakdown and the
firmware-vs-measurement triage.

## Document Changelog

| Date | Author | Change |
|------|--------|--------|
| 2026-04-27 | agent:ssa-4 | Created M2 adversarial harness, signal generators, runner, thresholds doc, and initial baseline. Verified `native_test_esv11_music_corpus_32khz` builds cleanly. |
