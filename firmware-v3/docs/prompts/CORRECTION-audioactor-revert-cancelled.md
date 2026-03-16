# CORRECTION: AudioActor Coefficient Revert — CANCELLED

**Status: DO NOT EXECUTE**
**Applies to:** `audioactor-coefficient-revert-prompt.md` in this directory
**Date:** 2026-03-14

## What Happened

The Cowork agent (Opus) analysed the K1 Waveform (0x1302) colour corruption and identified two suspected causes:

1. **EdgeMixer bugs** — saturation destruction, trail fragmentation, RMS_GATE hard kill
2. **AudioActor smoothing coefficients** — commit `5c9f4f74` changed 8 coefficients that feed K1-family effects via ControlBus

The Opus agent wrote a detailed revert prompt (`audioactor-coefficient-revert-prompt.md`) to restore the pre-`5c9f4f74` coefficients. **This revert prompt is WRONG and must not be executed.**

## Why the Revert Is Wrong

K1 Waveform was ported from Sensory Bridge, which has a fundamentally different audio pipeline. The pre-`5c9f4f74` coefficients were **uncalibrated Sensory Bridge defaults** — they had not been adapted for firmware-v3's ESV11 pipeline architecture.

Commit `5c9f4f74` ("perf(audio): aggressive smoothing reduction for sharp visual response") was **deliberate calibration** to make the SB-ported effects respond correctly on the v3 pipeline. The coefficients look "aggressive" compared to the SB originals because the v3 pipeline has different hop sizes, FFT resolution, and smoothing stages upstream.

**Reverting these coefficients would:**
- Undo calibration work that was intentionally done for the v3 pipeline
- Break audio response for ALL SB-ported effects (K1 Waveform, K1 Bloom, and any future SB ports)
- Reintroduce the original uncalibrated-for-v3 behaviour

## What Was Actually Wrong

The colour corruption was caused entirely by **EdgeMixer bugs**, not AudioActor coefficients. Your original diagnosis from the first investigation was correct:

1. **Saturation destruction** — `scale8(hsv.sat, 217)` in COMPLEMENTARY mode applying 15% sat reduction to every pixel via lossy RGB→HSV→RGB round-trip. K1 Waveform builds colours via additive chromagram accumulation — the round-trip destroys the blend.

2. **Trail fragmentation** — `maxC < 6` near-black skip threshold was too high. K1 Waveform's sub-pixel rendering produces fractional blend values (e.g., 30% brightness on the dim half of a dot) that fall below 6. EdgeMixer skipped them, causing one half of each sub-pixel pair to get colour-shifted while the other didn't. **Fixed:** threshold lowered to `maxC < 2`.

3. **RMS_GATE hard kill** — `effectiveGlobal == 0` caused early return, leaving strip 2 completely unprocessed during silence. Waveform trails on strip 2 couldn't fade naturally — they just froze while strip 1 faded. **Fixed:** temporal floor of 15 added so RMS_GATE attenuates instead of hard-killing.

## What You Should Do

1. **Do NOT execute `audioactor-coefficient-revert-prompt.md`.** The AudioActor coefficients are correct as-is — they are v3 pipeline calibration, not bugs.

2. **The EdgeMixer fixes you already implemented are correct.** The 900-frame evaluation across both devices confirmed: zero near-black leaks, consistent cross-device behaviour, MIRROR baseline at expected divergence.

3. **Add a `# DEPRECATED — DO NOT USE` header to `audioactor-coefficient-revert-prompt.md`** so future agents don't pick it up. The file should not be deleted (user preference) but must be clearly marked as invalid.

4. **The SbK1BaseEffect.h tau constants (lines 166–176) are correct.** They were derived from the original K1 frame-coupled alphas for the `!hasSbParity` fallback path. They do not need changing regardless of AudioActor coefficient state.

## Coefficient Reference (for the record)

These are the CURRENT and CORRECT values in `src/audio/AudioActor.cpp`:

| Parameter | Current Value (CORRECT) | Pre-5c9f4f74 Value (WRONG for v3) |
|-----------|------------------------|-----------------------------------|
| Waveform follower attack | `delta * 0.6f` | `delta * 0.25f` |
| Waveform follower decay | `delta * 0.03f` | `delta * 0.005f` |
| Mood EMA base | `25.0f + (5.0f * moodNorm)` | `1.0f + (10.0f * moodNorm)` |
| Spectrogram follower (attack+decay) | `distance * 0.95f` | `distance * 0.75f` |
| Chroma peak decay | `*= 0.98f` | `*= 0.999f` |
| Chroma peak attack | `+= distance * 0.5f` | `+= distance * 0.05f` |

All values appear in the ESV11 backend section of AudioActor.cpp (`#if FEATURE_AUDIO_BACKEND_ESVADC11`). These are the only builds used: `esp32dev_audio_esv11_32khz` (V1) and `esp32dev_audio_esv11_k1v2_32khz` (K1v2). PipelineCore is NOT used in this project.
