# AudioActor Smoothing Coefficient Revert — CC Agent Prompt

## Context

You are working in the `firmware-v3/` directory of an ESP32-S3 LED controller project (LightwaveOS). The codebase uses PlatformIO with two conditional compilation backends: `esp32dev_audio_pipelinecore` (primary) and `esp32dev_audio_esv11` (legacy).

Commit `5c9f4f74` ("perf(audio): aggressive smoothing reduction for sharp visual response") changed 8 smoothing coefficients across two sidecar functions in `src/audio/AudioActor.cpp`. These coefficients feed K1-family effects via ControlBus fields (`sb_chromagram_smooth[]`, `sb_waveform_peak_scaled`, `sb_spectrogram_smooth[]`). The aggressive values cause chromagram normalisation instability — the chroma peak tracker's 20× faster decay collapses the normalisation ceiling on every transient, producing rapid hue jumps and brightness pumping in effects that use additive chromagram colour synthesis (K1 Waveform, K1 Bloom).

Your task is to revert these 8 coefficients to their pre-`5c9f4f74` values. Nothing else changes. Do NOT modify any other file.

## Architecture You Must Understand

`AudioActor.cpp` contains TWO copies of each sidecar function — one compiled under `#if FEATURE_AUDIO_BACKEND_ESVADC11` (ESV11 backend, lines ~1100–1300) and one under `#if FEATURE_AUDIO_BACKEND_PIPELINECORE` (PipelineCore backend, lines ~2700–2900). **Both copies must be reverted identically.** If you only change one copy, the other backend will still have aggressive coefficients and the bug persists when that build environment is selected.

The SbK1BaseEffect.h tau constants (lines 166–176) are derived from the ORIGINAL K1 frame-coupled alphas and are CORRECT. Do NOT modify SbK1BaseEffect.h. Do NOT modify SbK1BaseEffect.cpp. Do NOT modify any file other than `src/audio/AudioActor.cpp`.

## Exact Changes Required

### File: `src/audio/AudioActor.cpp`

There are exactly 8 edits. Each edit changes one numeric literal. Each edit appears in two locations (ESV11 and PipelineCore backend copies) = 8 total edits, 4 per backend.

#### Edit 1: Waveform follower attack (ESV11 backend)
- **Function:** `processSbWaveformSidecar()`
- **Context:** Assignment to `m_sbMaxWaveformValFollower` when `delta > 0` (attack branch)
- **Find:** `delta * 0.6f`
- **Replace with:** `delta * 0.25f`
- **Expected location:** ~line 1179

#### Edit 2: Waveform follower decay (ESV11 backend)
- **Function:** `processSbWaveformSidecar()`
- **Context:** Assignment to `m_sbMaxWaveformValFollower` when `delta < 0` (decay branch)
- **Find:** `delta * 0.03f`
- **Replace with:** `delta * 0.005f`
- **Expected location:** ~line 1182

#### Edit 3: Mood EMA smoothing rate base (ESV11 backend)
- **Function:** `processSbBloomSidecar()`
- **Context:** `smoothingRate` calculation using `moodNorm`
- **Find:** `25.0f + (5.0f * moodNorm)`
- **Replace with:** `1.0f + (10.0f * moodNorm)`
- **Expected location:** ~line 1223

#### Edit 4: Spectrogram follower attack AND decay (ESV11 backend)
- **Function:** `processSbBloomSidecar()`
- **Context:** `m_sbSpectrogramSmooth[i]` update — there are two branches (attack and decay) both using the same coefficient
- **Find:** `distance * 0.95f` (appears TWICE in the same function, once for attack, once for decay)
- **Replace both with:** `distance * 0.75f`
- **Expected locations:** ~lines 1235 and 1238

#### Edit 5: Waveform follower attack (PipelineCore backend)
- **Identical to Edit 1** but in the PipelineCore copy
- **Find:** `delta * 0.6f`
- **Replace with:** `delta * 0.25f`
- **Expected location:** ~line 2766

#### Edit 6: Waveform follower decay (PipelineCore backend)
- **Identical to Edit 2** but in the PipelineCore copy
- **Find:** `delta * 0.03f`
- **Replace with:** `delta * 0.005f`
- **Expected location:** ~line 2769

#### Edit 7: Mood EMA smoothing rate base (PipelineCore backend)
- **Identical to Edit 3** but in the PipelineCore copy
- **Find:** `25.0f + (5.0f * moodNorm)`
- **Replace with:** `1.0f + (10.0f * moodNorm)`
- **Expected location:** ~line 2820

#### Edit 8: Spectrogram follower attack AND decay (PipelineCore backend)
- **Identical to Edit 4** but in the PipelineCore copy
- **Find:** `distance * 0.95f` (appears TWICE)
- **Replace both with:** `distance * 0.75f`
- **Expected locations:** ~lines 2833 and 2836

## What You Must NOT Do

1. **Do NOT modify any file other than `src/audio/AudioActor.cpp`.** Not SbK1BaseEffect.h, not SbK1BaseEffect.cpp, not RendererActor.cpp, not EdgeMixer.h, not ControlBus.h. Nothing.
2. **Do NOT change the chroma peak tracker coefficients.** The `*= 0.98f` decay and `+= distance * 0.5f` attack in the chromagram section were separately reviewed and are being retained for now. Only revert the 8 specific coefficients listed above.
3. **Do NOT refactor, rename, reformat, or "improve" anything.** This is a surgical coefficient revert — change the numbers, nothing else.
4. **Do NOT modify comments** unless a comment explicitly references the old value and the new value makes the comment wrong (e.g., if a comment says "// attack=0.6" and you're changing to 0.25, update that comment). Do not add new comments.
5. **Do NOT create new files, new functions, or new abstractions.**
6. **Do NOT run the EdgeMixer evaluation harness.** This revert is AudioActor-only.

## Verification

After making all 8 edits:

1. **Count verification:** Run `grep -n "delta \* 0.25f" src/audio/AudioActor.cpp | wc -l` — must return **2** (one per backend). Run `grep -n "delta \* 0.005f" src/audio/AudioActor.cpp | wc -l` — must return **2**. Run `grep -n "1.0f + (10.0f \* moodNorm)" src/audio/AudioActor.cpp | wc -l` — must return **2**. Run `grep -n "distance \* 0.75f" src/audio/AudioActor.cpp | wc -l` — must return **4** (attack + decay × 2 backends).

2. **No aggressive values remain:** Run `grep -n "delta \* 0.6f\|delta \* 0.03f\|25.0f + (5.0f\|distance \* 0.95f" src/audio/AudioActor.cpp` — must return **zero lines**. If any match, you missed an edit.

3. **Build both backends:**
   ```bash
   cd firmware-v3
   pio run -e esp32dev_audio_pipelinecore 2>&1 | tail -5
   pio run -e esp32dev_audio_esv11 2>&1 | tail -5
   ```
   Both must compile with zero errors. Warnings are acceptable if they pre-existed.

4. **Diff review:** Run `git diff src/audio/AudioActor.cpp` and verify:
   - Exactly 10 lines changed (8 coefficient lines + up to 2 comment updates if applicable)
   - No unrelated changes
   - Both backend copies show identical reverts

## Pass/Fail Criteria

- **PASS:** All 8 coefficients reverted to exact pre-`5c9f4f74` values. Both backends build clean. Grep verification returns expected counts. No other files modified.
- **FAIL:** Any coefficient wrong. Any extra file modified. Any missing backend copy. Any build failure. Any "improvement" beyond the 8 specified edits.

## Deliverables

1. The edited `src/audio/AudioActor.cpp` with exactly 8 coefficient changes
2. Build output confirming both backends compile
3. `git diff` output for review
