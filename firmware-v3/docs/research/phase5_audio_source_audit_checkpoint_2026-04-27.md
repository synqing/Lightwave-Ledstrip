---
abstract: "Checkpoint for the 2026-04-27 Phase 5 audio-source cross-check. Captures source-grounded findings on ControlBus spectra population and current effect consumption of chroma, bands, bins64, and bins256."
---

# Phase 5 Audio Source Audit Checkpoint (2026-04-27)

## Scope

Read-only checkpoint for validating whether current audio-reactive effects use `bands[8]`, `chroma[12]`, `heavy_*`, `bins64`, and `bins256`.

## Initial Source Findings

- Canonical build is `esp32dev_audio_esv11_k1v2_32khz`; `firmware-v3/platformio.ini` sets it as `default_envs` and enables `FEATURE_AUDIO_BACKEND_ESV11_32KHZ`.
- `ControlBusFrame` contains all relevant arrays: `bands[8]`, `chroma[12]`, `heavy_bands[8]`, `heavy_chroma[12]`, `bins64[64]`, `bins64Adaptive[64]`, and `bins256[256]`.
- ESV11 adapter populates `bins64`, `bins64Adaptive`, then derives `bands[8]` by averaging 8-bin blocks; it also populates `chroma[12]` and heavy-smoothed band/chroma arrays.
- Production ESV11 path in `AudioActor.cpp` also builds a local 256-bin FFT for STM and copies it into `frame.bins256`, but the public `AudioContext::bins256()` accessor is compiled only for `FEATURE_AUDIO_BACKEND_PIPELINECORE`.
- Current direct effect consumers of `bins64`/`bins64Adaptive` found by text search:
  - `JuggleEffect.cpp`
  - `LGPSpectrumDetailEffect.cpp`
  - `LGPSpectrumDetailEnhancedEffect.cpp`
  - `esv11_reference/EsSpectrumRefEffect.cpp`
  - `sensorybridge_reference/SbRawWaveformScopeEffect.cpp`
- Current direct effect consumer of `bins256()` found by text search:
  - `LGPSpectrumDetailEffect.cpp`, guarded by `FEATURE_AUDIO_BACKEND_PIPELINECORE`; the effect is commented out of registration as dead/broken.
- Current effect corpus contains many more `chroma`/`heavy_chroma` and `bands`/`heavy_bands` consumers than direct `bins64`/`bins256` consumers.
- Multiple effects explicitly document migration from hardcoded `bins64` ranges to backend-agnostic `bands[8]` access.

## Pending

SSA-Audio, SSA-Effects, and SSA-Research-Reconcile are running parallel read-only audits to cross-check this checkpoint.
