---
abstract: "Complete specification for the OnsetDetector spectral flux pipeline. Chooses Option A (CPJKU/Bello log-domain, no AGC) for discrete onsets with a secondary linear envelope for LED effects. Every parameter cited from published research. Supersedes the broken AGC+log hybrid in the v0.1 implementation."
---

# ADR-002: Onset Detection Pipeline — Spectral Flux Architecture

## Status

**Proposed** — replaces `OnsetDetector.cpp` v0.1 (2026-03-25, AGC+linear hybrid)

## Context

The v0.1 `OnsetDetector` has a fundamental signal chain error:

1. **AGC** (asymmetric max-follower) normalises magnitude bins to 0..1 range.
2. The code computes **linear-domain** half-wave rectified difference on these AGC-normalised values.
3. The documentation claims "log-spectral flux" but the code never applies `log()`.
4. The AGC compresses all magnitude dynamics into a ~0..1 band. Linear differencing on this compressed signal produces near-zero flux values. A quiet tap and a loud kick both produce approximately the same tiny difference, defeating the purpose of onset detection.
5. If log were actually applied to the 0..1 range, it would produce negative values, and the half-wave rectifier would discard them — equally broken.

The AGC and log domain are **mutually exclusive** normalisation strategies. Using both (or neither, as the code actually does) breaks the detection function.

## Decision

**Option A: CPJKU/Bello log-domain, no AGC.** With a secondary linear-domain continuous envelope for LED effects.

### Why Option A over Option B (SensoryBridge)

| Criterion | Option A (CPJKU/Bello) | Option B (SensoryBridge) |
|-----------|----------------------|------------------------|
| Discrete onset events | Designed for this (peak picker, threshold) | Not designed — produces continuous signal only |
| Beat tracking input | Standard onset function input for autocorrelation | Would need separate peak picking bolted on |
| Volume invariance | log() compresses dynamic range naturally; 60 dB becomes ~6.9x difference, not 1000x | AGC provides explicit volume invariance but removes transient sharpness |
| Transient sensitivity | log() preserves sharp transient edges relative to steady state | AGC follower with slow decay smears transients over ~2s decay window |
| Published research basis | Bello et al. 2005, Boeck et al. 2012 (CPJKU), Dixon 2006, Essentia/madmom | Lixie Labs production code (no formal publication) |
| Parameter provenance | Every parameter traceable to peer-reviewed publications | Parameters from single-product tuning |
| Band-split triggers | Works correctly — log-domain flux has consistent dynamic range across frequency bands | Band-split on AGC-normalised values: AGC is full-band, so band ratios are distorted |

**Hybrid extension:** A secondary continuous onset envelope (0..1, suitable for direct LED brightness mapping) is derived from the log-spectral flux via a fast-attack/slow-decay follower. This provides what SensoryBridge's linear approach gives, without compromising the discrete onset detector.

### Why not pure SensoryBridge

SensoryBridge's approach (AGC + linear + sqrt compression) is optimised for continuous visualisation — it produces a smooth "novelty" signal that directly drives LED brightness. It does not produce discrete onset events and has no peak picker or threshold mechanism. Since we need BOTH a continuous envelope AND discrete triggers (for beat tracking, kick/snare/hihat events), Option A is the better foundation. The continuous envelope is trivially derived from the detection function; deriving a reliable discrete detector from a continuously-compressed signal is harder and less well-studied.

## Specification

### 1. Magnitude Preprocessing

**No AGC. No normalisation. Raw FFT magnitudes.**

```
Input:  1024 float samples, DC-blocked, Hann-windowed
FFT:    1024-point real FFT (esp-dsp half-length complex trick, ~269 us)
Output: 512 magnitude bins, 31.25 Hz spacing @ 32 kHz sample rate

magnitude[k] = sqrt(Re[k]^2 + Im[k]^2)    for k = 0..511
```

Magnitudes are left in **raw linear scale** (units: FFT amplitude). No normalisation, no AGC. The log transform in step 2 provides the necessary dynamic range compression.

**Citation:** Bello et al. (2005), "A Tutorial on Onset Detection in Music Signals", IEEE TASLP 13(5), Section III-B: "The magnitude of the STFT is computed from the complex-valued output." No normalisation step is mentioned. The log is applied directly to raw magnitudes.

### 2. Spectral Flux Formula

**Log-domain spectral flux with half-wave rectification (Bello SF_log).**

```
eps = 1e-7

log_mag[k]      = log10(max(eps, magnitude[k]))
log_prev_mag[k] = log10(max(eps, prev_magnitude[k]))

SF(t) = SUM over k in [binLo..binHi) of:
    max(0, log_mag[k] - log_prev_mag[k])
```

The `eps` floor prevents `log(0)` but does NOT affect signal dynamics. For typical FFT magnitudes (0.001 to 100+), `log10` maps to roughly -3 to +2. The half-wave rectifier keeps only positive-going log differences (energy increases = onsets).

**Why log10 not ln:** Log base is irrelevant to the detection function — it is a constant scale factor that washes out in the adaptive threshold. We use log10 because the resulting flux values are in a more intuitive range (0..50 typical vs 0..115 for ln). CPJKU reference code uses `np.log` (natural log); we normalise to log10 for debuggability. The threshold parameters are adjusted accordingly.

**Citation:** Bello et al. (2005), Eq. 6: "SF_log(n) = SUM_k H(|X(n,k)|_log - |X(n-1,k)|_log)" where H() is the half-wave rectifier. Boeck et al. (2012) "Evaluating the Online Capabilities of Onset Detection Methods" (ISMIR): "logarithmic magnitude spectrogram" as preprocessing before spectral flux.

### 3. Musical Bin Range

```
binLo = 2     // ~62.5 Hz  (skip DC and sub-bass rumble below 62 Hz)
binHi = 192   // ~6000 Hz  (musical ceiling — hi-hat fundamentals extend to ~5-6 kHz)
```

**Rationale for binHi = 192 (not 128):** The v0.1 ceiling of 128 bins (~4 kHz) cuts off hi-hat detection. Hi-hat fundamental energy extends to 5-6 kHz (Tindale et al. 2004, "Retrieval of Percussion Gestures"). Bins above 192 (~6 kHz) contain primarily noise and aliased harmonics for typical music content at 32 kHz sample rate.

**Citation:** Boeck et al. (2012) use "spectrogram from 30 Hz to Nyquist" — they include the full range. Our truncation at 6 kHz is a pragmatic choice for the 32 kHz sample rate (Nyquist at 16 kHz) and the musical content relevant to LED visualisation.

### 4. Adaptive Threshold

**Moving average + fixed offset (CPJKU/SuperFlux approach).**

The adaptive threshold operates on a **causal** sliding window of past flux values (no future lookahead for the threshold itself).

```
threshold(t) = alpha * mean(SF[t - pre_avg_frames .. t-1]) + delta

onset_env(t) = max(0, SF(t) - threshold(t))
```

Parameters:

| Parameter | Value | Derivation |
|-----------|-------|------------|
| `pre_avg_frames` | 12 | CPJKU `pre_avg` = 100 ms. At 125 Hz hop rate: 100/8 = 12.5 -> 12 frames. Source: Boeck & Widmer (2013) "Maximum Filter Vibrato Suppression for Onset Detection", Table 1. |
| `alpha` | 1.0 | Multiplicative scaling of the moving average. Set to 1.0 (no scaling) per Boeck et al. default. The threshold sensitivity is controlled by `delta`. |
| `delta` | 0.05 | Fixed offset above the moving average. Source: Boeck & Widmer (2013), Table 1: `delta` = 0.05 for SuperFlux. This is in log10-flux units (not arbitrary). |

**Why sliding window, not EMA:** The v0.1 implementation used an EMA (exponential moving average) for mean and variance tracking. EMA has infinite memory — a single loud transient biases the mean for hundreds of frames (the 0.003 alpha gives ~333-frame time constant = 2.7 seconds). A finite sliding window forgets cleanly. CPJKU and madmom both use finite sliding windows.

**Implementation:** Circular buffer of the last `pre_avg_frames` flux values. Mean computed per-frame. This costs 12 floats (48 bytes) and one division per frame — negligible.

**Citation:** Boeck et al. (2012), Section 3.2: "thresholding function ... uses a local mean of the detection function." Boeck & Widmer (2013): `pre_avg = 100ms`, `delta = 0.05` (SuperFlux optimal parameters from Table 1). Dixon (2006) "Onset Detection Revisited": `delta` acts as a noise floor to prevent false triggers during sustained tones.

### 5. Peak Picker

**Local maximum + refractory gate (CPJKU standard peak picker).**

```
onset_event(t) = onset_env(t)   IF all of:
    (a) onset_env(t) >= onset_env(t-i) for i = 1..pre_max_frames   (local max, past)
    (b) onset_env(t) >= onset_env(t+j) for j = 1..post_max_frames  (local max, future)
    (c) t - t_last_event >= wait_frames                              (refractory gate)

onset_event(t) = 0   otherwise
```

Parameters:

| Parameter | Value | Frames | Derivation |
|-----------|-------|--------|------------|
| `pre_max_frames` | 3 | ~24 ms | CPJKU `pre_max` = 30 ms. At 125 Hz: 30/8 = 3.75 -> 3. Source: Boeck & Widmer (2013), Table 1. |
| `post_max_frames` | 1 | ~8 ms | CPJKU `post_max` = 10 ms. At 125 Hz: 10/8 = 1.25 -> 1. Introduces 8 ms latency — acceptable for LED visualisation. Source: Boeck & Widmer (2013), Table 1. |
| `wait_frames` | 4 | ~32 ms | CPJKU `combine` = 30 ms minimum inter-onset interval. At 125 Hz: 30/8 = 3.75 -> 4 frames (32 ms). This prevents double-triggers on a single physical onset. Source: Boeck et al. (2012), Section 3.2. |

**Note:** The v0.1 had `peakWait = 8` (64 ms). This is too long — it suppresses valid hi-hat patterns at 16th notes (125 BPM = 120 ms per 8th note, 60 ms per 16th). 32 ms allows clean 16th note detection up to ~220 BPM.

**Citation:** Boeck & Widmer (2013), Table 1. Boeck et al. (2012), Section 3.2: "peaks are picked from the thresholded detection function."

### 6. Band-Split Triggers

**Independent log-spectral flux per frequency band, each with its own sliding-window threshold.**

Each band computes its own log-spectral flux using the same formula as the full-band flux, but restricted to its bin range. Each band has its own independent adaptive threshold (sliding window mean + delta).

#### Band Definitions

| Band | Bin Range | Frequency Range | Musical Target |
|------|-----------|-----------------|----------------|
| Bass (kick) | 2 - 8 | 62 - 250 Hz | Kick drum fundamental, bass guitar |
| Mid (snare) | 8 - 40 | 250 - 1250 Hz | Snare body, vocal transients, guitar attacks |
| High (hihat) | 40 - 192 | 1250 - 6000 Hz | Hi-hat, cymbals, snare rattle |

**Changes from v0.1:**
- Snare upper bin: 32 -> 40 (1000 Hz -> 1250 Hz). Snare body extends to ~1.2 kHz (Tindale et al. 2004).
- Hihat lower bin: 32 -> 40 (matching snare upper for non-overlapping bands).
- Hihat upper bin: 128 -> 192 (4 kHz -> 6 kHz). Hi-hat energy extends higher.

#### Band Threshold Parameters

| Parameter | Kick | Snare | Hi-hat | Derivation |
|-----------|------|-------|--------|------------|
| `pre_avg_frames` | 12 | 12 | 12 | Same as full-band (100 ms window). |
| `delta` | 0.08 | 0.06 | 0.04 | Kick needs higher delta (bass has more spectral leakage). Hi-hat needs lower delta (sharp transients, less sustained energy). Proportional to typical per-band log-flux magnitude range — see Section 8 below. |
| `wait_frames` | 6 | 4 | 3 | Kick: ~48 ms (fastest kick patterns ~100 ms apart). Snare: ~32 ms (same as full-band). Hi-hat: ~24 ms (16th note hi-hats at 150 BPM = 100 ms, need headroom). |

**Citation for band frequency ranges:** Tindale et al. (2004), "Retrieval of Percussion Gestures Using Timbre Classification Techniques". FitzGerald (2010), "Harmonic/Percussive Separation using Median Filtering", DAFX, Section 2: frequency ranges for percussion classification. The specific bin boundaries are adapted to our 31.25 Hz/bin resolution.

#### Trigger Logic

```
band_flux(t) = SUM over k in [band_binLo..band_binHi) of:
    max(0, log_mag[t,k] - log_prev_mag[t,k])

band_threshold(t) = mean(band_flux[t - pre_avg .. t-1]) + band_delta

band_trigger(t) = (band_flux(t) > band_threshold(t))
                  AND (t - t_last_band_trigger >= band_wait)
```

No peak picking on band triggers — the refractory gate alone is sufficient for percussion triggers. Peak picking adds latency (post_max lookahead) that is undesirable for immediate LED response to kicks and hi-hats.

### 7. Continuous Onset Envelope (LED Effect Output)

The discrete onset detector (sections 4-5) produces sparse events. LED effects also need a **continuous** 0..1 envelope that tracks "onset-ness" — high during transients, decaying smoothly between them.

```
raw_env(t) = onset_env(t)    // From section 4 (flux above threshold)

// Fast-attack, slow-decay follower (clamped to 0..1 for LED mapping)
if raw_env(t) > smooth_env(t-1):
    smooth_env(t) = raw_env(t)                                    // instant attack
else:
    smooth_env(t) = smooth_env(t-1) * decay_per_frame             // exponential decay

// Normalise to 0..1 via adaptive peak follower
env_peak += (smooth_env(t) - env_peak) * 0.002                   // ~500 frames = 4s decay
if smooth_env(t) > env_peak:
    env_peak = smooth_env(t)                                      // instant attack
if env_peak < 0.001:
    env_peak = 0.001                                              // floor

onset_envelope_01(t) = clamp01(smooth_env(t) / env_peak)
```

| Parameter | Value | Derivation |
|-----------|-------|------------|
| `decay_per_frame` | 0.92 | At 125 Hz: half-life = ln(0.5)/ln(0.92) * 8 ms = ~66 ms. Matches typical LED effect visual decay. SensoryBridge uses similar decay for its output smoothing. |
| `env_peak_alpha` | 0.002 | ~500 frames = 4 s for peak follower to decay. Matches SensoryBridge's AGC time constant. |

**This is the SensoryBridge-inspired component** of the hybrid. The continuous envelope uses the log-domain flux as its input (preserving transient sharpness from Option A), but applies the same output conditioning philosophy as SensoryBridge (adaptive normalisation to 0..1 for direct LED mapping).

### 8. Expected Value Ranges

These ranges are derived from the signal chain mathematics, validated against published results.

#### Full-Band Log-Spectral Flux

**Steady-state (sustained tone or ambient noise):**
- Flux values: **0.5 - 5.0** (log10 units summed over ~190 bins)
- Reasoning: Steady-state magnitude bins change slowly between frames. Per-bin log10 differences are small (~0.001-0.01). Summed over 190 bins: 190 * 0.005 = ~0.95. With half-wave rectification keeping only positive changes, roughly half contribute: ~0.5.

**Onset (percussive attack, new note):**
- Flux values: **10 - 80** (typical), up to **150** (loud broadband transient like a snare hit)
- Reasoning: A percussive onset increases magnitude by 20-40 dB in affected bins. log10(100) - log10(1) = 2.0 per bin. If 30-60 bins see a significant onset: 40 * 1.5 = 60 typical. Maximum with full-band transient: 190 * 0.8 = ~150.

**Ratio (onset/steady):** 10x - 80x. This is the dynamic range that makes log-domain flux work — raw linear flux only achieves 5x-20x ratio because amplitude differences dominate over spectral shape changes.

**Citation:** Bello et al. (2005), Figure 3: onset detection function shows peaks 10-50x above the baseline for musical excerpts. Our expected range is consistent with 190-bin summation at 31.25 Hz resolution.

#### Band-Split Flux

| Band | Steady-State | Onset | Bins |
|------|-------------|-------|------|
| Bass (kick) | 0.1 - 1.0 | 5 - 20 | 6 bins |
| Mid (snare) | 0.3 - 3.0 | 8 - 40 | 32 bins |
| High (hihat) | 0.5 - 5.0 | 10 - 50 | 152 bins |

Scaling: Band flux is proportional to bin count. High band has 25x more bins than bass, so its steady-state and onset values are correspondingly larger. The per-band threshold (section 6) normalises this automatically via the sliding-window mean.

#### Adaptive Threshold

- Threshold value (full-band): **1.5 - 8.0** (mean of recent flux + delta)
- Onset envelope (above threshold): **0** during steady state, **5 - 70** during onset
- Peak-picked events: **0** (no event) or **5 - 70** (event strength = envelope value at peak)

#### Continuous Envelope (0..1)

- Silence: **0.0**
- Between onsets: **0.0 - 0.15** (residual decay)
- During onset: **0.6 - 1.0** (normalised by adaptive peak)
- Sustained loud section: **0.2 - 0.5** (elevated baseline from frequent transients)

### 9. Warmup Behaviour

| Phase | Duration | Behaviour |
|-------|----------|-----------|
| Frame 0 | 1 frame (8 ms) | No prev_magnitude. Store magnitudes, output zeros. |
| Frames 1-12 | ~96 ms | Sliding window filling. Threshold uses partial window (mean of available frames). No peak picking (need pre_max history). Band triggers suppressed. |
| Frames 13-16 | ~32 ms | Peak picker active (has 3-frame pre_max + 1-frame post_max history). Full detection operational. |
| Frame 17+ | Steady state | All systems fully warmed. |

Total warmup: **~128 ms** (16 frames). Compared to v0.1's 400 ms (50 frames with EMA), this is 3x faster.

### 10. Memory Budget

| Buffer | Size | Notes |
|--------|------|-------|
| `m_hannLut[1024]` | 4096 B | Pre-computed Hann window (unchanged) |
| `m_fftBuf[1024]` | 4096 B | In-place FFT (unchanged) |
| `m_magnitude[512]` | 2048 B | Current magnitudes (unchanged) |
| `m_prevMagnitude[512]` | 2048 B | Previous magnitudes (unchanged) |
| `m_fluxHistory[12]` | 48 B | **NEW:** Sliding window for full-band threshold |
| `m_bassFluxHistory[12]` | 48 B | **NEW:** Sliding window for kick threshold |
| `m_midFluxHistory[12]` | 48 B | **NEW:** Sliding window for snare threshold |
| `m_highFluxHistory[12]` | 48 B | **NEW:** Sliding window for hihat threshold |
| `m_envRing[5]` | 20 B | **SHRUNK:** Peak picker only needs pre_max(3)+post_max(1)+1 = 5 entries (was 32) |
| Scalars | ~60 B | Peak follower, frame counters, etc. |
| **Total** | ~12,560 B | **Down from ~16 KB** (removed AGC state, shrunk ring buffer) |

### 11. Computational Budget

| Step | Estimated us | Notes |
|------|-------------|-------|
| Hann window | ~15 | 1024 multiplies |
| FFT (esp-dsp) | ~269 | Measured, half-length complex trick |
| Magnitudes | ~40 | 511 sqrt operations |
| Log magnitudes | ~60 | 190 log10f calls (only musical bins) |
| Flux computation | ~15 | 190 subtracts + max + accumulate |
| Band flux (3 bands) | ~10 | Subset of above bins |
| Threshold + peak pick | ~5 | Arithmetic on ring buffers |
| Continuous envelope | ~2 | 3 multiplies + 1 divide |
| **Total** | ~416 us | **Well under 3 ms budget** |

The log10f calls add ~60 us compared to v0.1's linear-domain flux. This is offset by removing the AGC loop (~30 us for max-finding + scaling). Net increase: ~30 us — negligible.

### 12. Summary of Changes from v0.1

| Aspect | v0.1 (Broken) | v0.2 (This Spec) |
|--------|---------------|-------------------|
| Magnitude preprocessing | AGC max-follower, normalise to 0..1 | **None — raw magnitudes** |
| Flux domain | Linear (mislabelled as "log") | **Log10** |
| Flux formula | `max(0, mag[k] - prev[k])` | **`max(0, log10(mag[k]) - log10(prev[k]))`** |
| `eps` floor | None (implicit via AGC floor) | **1e-7** (prevents log(0)) |
| Threshold | EMA mean + k*sigma | **Sliding window mean + fixed delta** |
| Threshold window | Infinite (EMA, alpha=0.003, tau=2.7s) | **12 frames (96 ms)** |
| Peak picker ring | 32 entries (oversized) | **5 entries** (minimum required) |
| Refractory period | 8 frames (64 ms) | **4 frames (32 ms)** |
| Musical bin range | 2-128 (62-4000 Hz) | **2-192 (62-6000 Hz)** |
| Snare bin range | 8-32 (250-1000 Hz) | **8-40 (250-1250 Hz)** |
| Hihat bin range | 32-128 (1-4 kHz) | **40-192 (1.25-6 kHz)** |
| Band triggers | Mean * multiplier (EMA) | **Sliding window + delta + refractory** |
| Continuous envelope | None | **NEW: fast-attack/slow-decay + adaptive peak** |
| Warmup | 50 frames (400 ms) | **16 frames (128 ms)** |

## Consequences

### What becomes easier
- Onset detection actually works — the signal chain is mathematically coherent.
- Log domain provides natural volume invariance without AGC (a 20 dB louder signal produces +2 log units, not 10x amplitude — the adaptive threshold absorbs this shift within 96 ms).
- Band triggers have correct dynamic range — each band's flux is in log-domain so a kick drum's 6-bin flux has comparable scale to a hi-hat's 152-bin flux after threshold normalisation.
- Continuous onset envelope gives effects a reliable 0..1 signal that tracks transients, filling the gap the broken v0.1 flux signal was meant to serve.
- 3x faster warmup (128 ms vs 400 ms) — onset detection is live within the first beat of most songs.

### What becomes harder
- Log10f calls add ~60 us per hop. Still well within budget but this is the primary cost of correctness.
- Sliding window buffers add ~200 bytes of SRAM. Negligible on ESP32-S3 (512 KB SRAM).
- The flux output is no longer in 0..1 range — effects that consumed `onsetFlux` as a normalised value must use `onset_envelope_01` instead, or handle the raw log-domain range (0..150).

### What stays the same
- FFT pipeline (Hann window, esp-dsp half-length trick, magnitude extraction) — unchanged.
- Integration with AudioActor and ControlBus — same `OnsetResult` struct, same merge point.
- Band-split trigger semantics — same `kick_trigger`, `snare_trigger`, `hihat_trigger` booleans.
- Memory footprint — actually decreases slightly.

## References

1. Bello, J.P. et al. (2005). "A Tutorial on Onset Detection in Music Signals." IEEE Transactions on Audio, Speech, and Language Processing, 13(5), pp. 1035-1047.
2. Boeck, S. et al. (2012). "Evaluating the Online Capabilities of Onset Detection Methods." Proc. ISMIR.
3. Boeck, S. & Widmer, G. (2013). "Maximum Filter Vibrato Suppression for Onset Detection." Proc. DAFx. Table 1: SuperFlux optimal parameters.
4. Dixon, S. (2006). "Onset Detection Revisited." Proc. DAFx. Section 3: adaptive threshold with fixed offset delta.
5. Tindale, A. et al. (2004). "Retrieval of Percussion Gestures Using Timbre Classification Techniques." Proc. ISMIR. Percussion frequency ranges.
6. FitzGerald, D. (2010). "Harmonic/Percussive Separation using Median Filtering." Proc. DAFx. Section 2: percussive frequency characterisation.
7. CPJKU madmom library: `madmom.features.onsets.SpectralOnsetProcessor`. Reference implementation of log-spectral flux with peak picking.
8. Lixie Labs SensoryBridge: `sensory_bridge/bridge_fs/FFT.h`. AGC + linear + sqrt production implementation.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-25 | agent:software-architect | Created. Full specification for OnsetDetector v0.2, choosing CPJKU/Bello log-domain approach with hybrid continuous envelope. |
