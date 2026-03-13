/**
 * @file SbK1BaseEffect.cpp
 * @brief Shared base class implementation for K1.Lightwave effect ports
 *
 * All smoothing uses rate-independent exponential moving averages:
 *   alpha = 1 - exp(-dt / tau)
 * Tau constants were derived from original frame-coupled alphas at 120 FPS.
 */

#include "SbK1BaseEffect.h"

#include "../../../config/features.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#include <esp_heap_caps.h>
#endif

#include <cmath>
#include <cstring>
#include <cstdlib>  // rand

namespace lightwaveos::effects::ieffect::sensorybridge_reference {

// =========================================================================
// Utility
// =========================================================================

namespace {

static inline float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

} // namespace

// =========================================================================
// IEffect lifecycle
// =========================================================================

bool SbK1BaseEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    return baseInit();
}

void SbK1BaseEffect::render(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps) return;
#endif
#if !FEATURE_AUDIO_SYNC
    (void)ctx;
    return;
#else
    if (!ctx.audio.available) {
        fadeToBlackBy(ctx.leds, ctx.ledCount, 32);
        return;
    }
    baseProcessAudio(ctx);
    renderEffect(ctx);
#endif
}

void SbK1BaseEffect::cleanup() {
    baseCleanup();
}

// =========================================================================
// baseInit / baseCleanup
// =========================================================================

bool SbK1BaseEffect::baseInit() {
#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<SbK1BasePsram*>(
            heap_caps_malloc(sizeof(SbK1BasePsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) return false;
    }
    std::memset(m_ps, 0, sizeof(SbK1BasePsram));
#endif

    // Zero DRAM state
    std::memset(m_chromaSmooth, 0, sizeof(m_chromaSmooth));
    std::memset(m_noveltyCurve, 0, sizeof(m_noveltyCurve));
    m_histIdx           = 0;
    m_chromaPeakTracker = 0.01f;

    m_huePosition       = 0.0f;
    m_hueShiftSpeed     = 0.0f;
    m_huePushDir        = -1.0f;
    m_hueDest           = 0.0f;
    m_hueShiftMix       = -0.35f;
    m_hueShiftMixTarget = 1.0f;

    m_vuLast            = 0.0f;
    m_vuAvg             = 0.0f;
    m_wfPeakScaled      = 0.0f;
    m_wfPeakLast        = 0.0f;
    m_wfFollower        = 750.0f;

    m_lastHopSeq        = 0;

    m_chromaColor       = CRGB_F();
    m_chromaTotalMag    = 0.0f;

    return true;
}

void SbK1BaseEffect::baseCleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
}

// =========================================================================
// baseProcessAudio — full per-frame audio pipeline
// =========================================================================

void SbK1BaseEffect::baseProcessAudio(plugins::EffectContext& ctx) {
#if !FEATURE_AUDIO_SYNC
    (void)ctx;
    return;
#else
    const auto& cb = ctx.audio.controlBus;

    // Compute rate-independent dt from render context
    m_dt = ctx.getSafeDeltaSeconds();

    // -----------------------------------------------------------------
    // SB parity shortcut: if the pipeline already provides K1-style data,
    // use it directly instead of reconstructing from bins256.
    // -----------------------------------------------------------------
    bool hasSbParity = ctx.audio.hasSbWaveform() && (cb.sb_hue_position > 0.0001f);

    if (hasSbParity) {
        // Use pre-computed SB chromagram
        for (int i = 0; i < kChromaBins; ++i) {
            m_chromaSmooth[i] = cb.sb_chromagram_smooth[i];
        }
        m_huePosition = cb.sb_hue_position;
        m_wfPeakScaled = cb.sb_waveform_peak_scaled;
        // Must also compute smoothed peak (K1's waveform_peak_scaled_last)
        // tau=0.023s (was alpha=0.3/0.7 at 120 FPS)
        float aWfPeak = 1.0f - expf(-m_dt / kTauWfPeakLast);
        m_wfPeakLast += (m_wfPeakScaled - m_wfPeakLast) * aWfPeak;
    } else {
#if FEATURE_AUDIO_BACKEND_PIPELINECORE
        // Reconstruct from bins256 → full pipeline
        reconstructSpectrum96(cb.bins256, cb.binHz);
        smoothSpectrogram();
        buildChromagram();
        computeNovelty();

        // Run color shift with latest novelty value
        float noveltyNow = m_noveltyCurve[0];
        processColorShift(noveltyNow);
#else
        // Without bins256, use ControlBus chroma directly.
        // Do NOT call smoothSpectrogram()/buildChromagram() — they operate
        // on specSmooth[]/magAvg[] which are never populated on this path
        // and would overwrite m_chromaSmooth with zeros.
        for (int i = 0; i < kChromaBins; ++i) {
            m_chromaSmooth[i] = cb.chroma[i];
        }

        // Derive novelty from flux (closest ControlBus equivalent)
        float noveltyNow = ctx.audio.flux();
        for (int i = kHistoryDepth - 1; i > 0; --i) {
            m_noveltyCurve[i] = m_noveltyCurve[i - 1];
        }
        m_noveltyCurve[0] = noveltyNow;
        processColorShift(noveltyNow);
#endif
        // Update waveform peak
        updateWaveformPeak(ctx);
    }

    // Update VU level (always, regardless of path)
    float rmsNow = ctx.audio.rms();
    // tau=0.037s (was alpha=0.2/0.8 at 120 FPS)
    float aVu = 1.0f - expf(-m_dt / kTauVuAvg);
    m_vuAvg += (rmsNow - m_vuAvg) * aVu;
    m_vuLast = rmsNow;

#endif
}

// =========================================================================
// getVu — floor-subtracted VU
// =========================================================================

float SbK1BaseEffect::getVu(plugins::EffectContext& ctx) const {
    float vu = ctx.audio.rms() - m_vuAvg;
    if (vu < 0.0f) vu = 0.0f;
    return vu;
}

// =========================================================================
// applyContrast — K1 contrast curve
// =========================================================================

float SbK1BaseEffect::applyContrast(float bin, float squareIter) {
    // K1 parity: integer iterations of bin *= bin, then fractional blend
    // For SQUARE_ITER=1: one full squaring → bin²
    // For SQUARE_ITER=1.5: bin² then blend halfway to bin⁴
    uint8_t intIter = (uint8_t)squareIter;
    float fractIter = squareIter - floorf(squareIter);

    // Integer iterations
    for (uint8_t i = 0; i < intIter; ++i) {
        bin *= bin;
    }

    // Fractional iteration: lerp between current and one more squaring
    if (fractIter > 0.01f) {
        float squared = bin * bin;
        bin = bin * (1.0f - fractIter) + squared * fractIter;
    }

    return bin;
}

// =========================================================================
// hsvToRgbF — K1-style HSV->RGB (float domain)
// =========================================================================

CRGB_F SbK1BaseEffect::hsvToRgbF(float h, float s, float v) {
#ifndef NATIVE_BUILD
    // K1 parity: generate color at V=255 (full bright), then scale by v separately.
    // K1's hsv() passes V=255 to CHSV, converts to CRGB16, then multiplies by v.
    // This avoids FastLED's non-linear V dimming at low values.
    uint8_t hue8 = (uint8_t)(h * 255.0f);
    uint8_t sat8 = (uint8_t)(s * 255.0f);
    CRGB out;
    hsv2rgb_rainbow(CHSV(hue8, sat8, 255), out);  // V=255 always (rainbow = K1 parity)
    CRGB_F result = CRGB_F::fromCRGB(out);
    result.r *= v;  // Apply value as linear multiply
    result.g *= v;
    result.b *= v;
    return result;
#else
    // Simple HSV->RGB for native builds (testing)
    float hh = fmodf(h, 1.0f);
    if (hh < 0.0f) hh += 1.0f;
    float hh6 = hh * 6.0f;
    int sector = (int)hh6;
    float frac = hh6 - sector;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * frac);
    float t = v * (1.0f - s * (1.0f - frac));
    switch (sector % 6) {
        case 0: return {v, t, p};
        case 1: return {q, v, p};
        case 2: return {p, v, t};
        case 3: return {p, q, v};
        case 4: return {t, p, v};
        case 5: return {v, p, q};
    }
    return {v, v, v};
#endif
}

// =========================================================================
// paletteColorF — palette-based colour lookup (no HSV round-trip)
// =========================================================================

CRGB_F SbK1BaseEffect::paletteColorF(const plugins::PaletteRef& palette,
                                      float position, float brightness) {
    // Wrap position to [0,1)
    position -= floorf(position);
    uint8_t idx = (uint8_t)(position * 255.0f);
    // Fetch from user-selected palette at full brightness, then scale linearly.
    // This matches K1's pattern: generate colour at V=255, then multiply by v.
    CRGB color = palette.getColor(idx, 255);
    CRGB_F result = CRGB_F::fromCRGB(color);
    result.r *= brightness;
    result.g *= brightness;
    result.b *= brightness;
    return result;
}

// =========================================================================
// reconstructSpectrum96 — map 256 linear bins to 96 semitone bins
// =========================================================================

void SbK1BaseEffect::reconstructSpectrum96(const float* bins256, float binHz) {
#ifndef NATIVE_BUILD
    if (!m_ps || !bins256 || binHz <= 0.0f) return;

    for (int note = 0; note < kSpecBins; ++note) {
        float freq = kK1Notes[note];

        // Find the fractional bin index for this frequency
        float binIndex = freq / binHz;

        // Interpolate between adjacent FFT bins
        int binLow = (int)binIndex;
        float frac = binIndex - binLow;

        float mag = 0.0f;
        if (binLow >= 0 && binLow < 255) {
            mag = bins256[binLow] * (1.0f - frac) + bins256[binLow + 1] * frac;
        } else if (binLow >= 0 && binLow < 256) {
            mag = bins256[binLow];
        }

        // Magnitude averaging: near-passthrough for sharp response
        // tau=0.007s (was alpha=0.7 new/0.3 old at 120 FPS)
        float aMag = 1.0f - expf(-m_dt / kTauMagAvg);
        m_ps->magAvg[note] += (mag - m_ps->magAvg[note]) * aMag;
    }
#else
    (void)bins256;
    (void)binHz;
#endif
}

// =========================================================================
// smoothSpectrogram — attack 0.5, decay 0.25 (frame-coupled)
// =========================================================================

void SbK1BaseEffect::smoothSpectrogram() {
#ifndef NATIVE_BUILD
    if (!m_ps) return;

    for (int i = 0; i < kSpecBins; ++i) {
        float current = m_ps->magAvg[i];
        float smooth = m_ps->specSmooth[i];

        if (current > smooth) {
            // Attack: near-instant
            // tau=0.004s (was alpha=0.85 at 120 FPS)
            smooth += (current - smooth) * (1.0f - expf(-m_dt / kTauSpecAttack));
        } else {
            // tau=0.009s (was alpha=0.6 at 120 FPS)
            smooth += (current - smooth) * (1.0f - expf(-m_dt / kTauSpecDecay));
        }
        m_ps->specSmooth[i] = smooth;
    }
#endif
}

// =========================================================================
// buildChromagram — fold 96 -> 12, peak-track, normalise
// =========================================================================

void SbK1BaseEffect::buildChromagram() {
#ifndef NATIVE_BUILD
    if (!m_ps) return;

    float chroma[kChromaBins] = {};

    // Fold 96 semitone bins into 12 chroma bins (8 octaves)
    for (int i = 0; i < kSpecBins; ++i) {
        int chromaBin = i % kChromaBins;
        chroma[chromaBin] += m_ps->specSmooth[i];
    }

    // Peak tracker: fast decay, near-instant attack
    float maxChroma = 0.0f;
    for (int i = 0; i < kChromaBins; ++i) {
        if (chroma[i] > maxChroma) maxChroma = chroma[i];
    }

    // tau=0.413s (was *=0.98 at 120 FPS)
    m_chromaPeakTracker *= expf(-m_dt / kTauChromaPeakDecay);
    if (maxChroma > m_chromaPeakTracker) {
        // tau=0.016s (was alpha=0.4 at 120 FPS)
        m_chromaPeakTracker += (maxChroma - m_chromaPeakTracker) * (1.0f - expf(-m_dt / kTauChromaPeakAttack));
    }

    // Normalise and store
    float invPeak = (m_chromaPeakTracker > 0.0001f) ? (1.0f / m_chromaPeakTracker) : 1.0f;
    for (int i = 0; i < kChromaBins; ++i) {
        m_chromaSmooth[i] = clamp01(chroma[i] * invPeak);
    }
#endif
}

// =========================================================================
// computeNovelty — positive spectral diffs, sqrt, history buffer
// =========================================================================

void SbK1BaseEffect::computeNovelty() {
#ifndef NATIVE_BUILD
    if (!m_ps) return;

    // Compute novelty: sum of positive spectral differences from previous frame
    float novelty = 0.0f;
    uint8_t prevIdx = (m_histIdx == 0) ? (kHistoryDepth - 1) : (m_histIdx - 1);

    for (int i = 0; i < kSpecBins; ++i) {
        float diff = m_ps->specSmooth[i] - m_ps->spectralHistory[prevIdx][i];
        if (diff > 0.0f) {
            novelty += diff;
        }
    }

    // Apply sqrt for perceptual scaling
    novelty = sqrtf(novelty);

    // Store current spectrum in history ring buffer
    std::memcpy(m_ps->spectralHistory[m_histIdx], m_ps->specSmooth, sizeof(float) * kSpecBins);
    m_histIdx = (m_histIdx + 1) % kHistoryDepth;

    // Shift novelty curve and insert new value at [0]
    for (int i = kHistoryDepth - 1; i > 0; --i) {
        m_noveltyCurve[i] = m_noveltyCurve[i - 1];
    }
    m_noveltyCurve[0] = novelty;
#endif
}

// =========================================================================
// processColorShift — EXACT K1 6-step algorithm
// =========================================================================

void SbK1BaseEffect::processColorShift(float noveltyNow) {
    // Step 1: Subtract threshold
    noveltyNow -= 0.10f;
    if (noveltyNow < 0.0f) noveltyNow = 0.0f;

    // Step 2: Scale up (inverse of 0.9 = 1.111111)
    noveltyNow *= 1.111111f;

    // Step 3: Square
    noveltyNow = noveltyNow * noveltyNow;

    // Step 4: Clamp to 0.02 maximum
    if (noveltyNow > 0.02f) noveltyNow = 0.02f;

    // Step 5: Speed control
    // If novelty > speed*0.5: speed = novelty*0.75
    // Else: speed *= 0.99, min 0.0001
    if (noveltyNow > m_hueShiftSpeed * 0.5f) {
        m_hueShiftSpeed = noveltyNow * 0.75f;
    } else {
        // tau=0.829s (was *=0.99 at 120 FPS)
        m_hueShiftSpeed *= expf(-m_dt / kTauHueSpeedDecay);
        if (m_hueShiftSpeed < 0.0001f) m_hueShiftSpeed = 0.0001f;
    }

    // Step 6: Position update — walk hue position
    m_huePosition += m_hueShiftSpeed * m_huePushDir;
    // Wrap to [0, 1]
    if (m_huePosition > 1.0f) m_huePosition -= 1.0f;
    if (m_huePosition < 0.0f) m_huePosition += 1.0f;

    // Destination check: if close enough, flip direction and pick new target
    float distToDest = fabsf(m_huePosition - m_hueDest);
    // Handle wrap-around distance
    if (distToDest > 0.5f) distToDest = 1.0f - distToDest;

    if (distToDest <= 0.01f) {
        // Flip direction
        m_huePushDir = -m_huePushDir;
        // Flip mix target
        m_hueShiftMixTarget = -m_hueShiftMixTarget;
        // New random destination [0, 1)
        m_hueDest = (float)(rand() % 10000) / 10000.0f;
    }

    // Mix: approach target at distance*0.01 rate
    float mixDist = m_hueShiftMixTarget - m_hueShiftMix;
    // tau=0.829s (was alpha=0.01 at 120 FPS)
    m_hueShiftMix += mixDist * (1.0f - expf(-m_dt / kTauHueMix));
}

// =========================================================================
// updateWaveformPeak — envelope follower (two-stage + smoothing)
// =========================================================================

void SbK1BaseEffect::updateWaveformPeak(plugins::EffectContext& ctx) {
#if FEATURE_AUDIO_SYNC
    // Extract peak amplitude from waveform (128 samples)
    const int16_t* wf = ctx.audio.waveform();
    if (!wf) return;

    float peak = 0.0f;
    for (int i = 0; i < audio::CONTROLBUS_WAVEFORM_N; ++i) {
        float sample = fabsf((float)wf[i]);
        if (sample > peak) peak = sample;
    }

    // K1 parity: subtract SWEET_SPOT_MIN_LEVEL noise floor before envelope
    peak -= 750.0f;
    if (peak < 0.0f) peak = 0.0f;

    // Stage 1: Envelope follower — aggressive attack/decay for sharp tracking
    if (peak > m_wfFollower) {
        // tau=0.007s (was alpha=0.7 at 120 FPS)
        m_wfFollower += (peak - m_wfFollower) * (1.0f - expf(-m_dt / kTauWfFollowerAttack));
    } else {
        // tau=0.100s (was alpha=0.08 at 120 FPS)
        m_wfFollower += (peak - m_wfFollower) * (1.0f - expf(-m_dt / kTauWfFollowerDecay));
    }
    if (m_wfFollower < 750.0f) m_wfFollower = 750.0f;

    // Normalise peak by follower → this is K1's waveform_peak_scaled (raw)
    float peakNorm = peak / m_wfFollower;
    if (peakNorm > 1.0f) peakNorm = 1.0f;

    // K1 parity: waveform_peak_scaled = raw normalized peak (used for trail fade)
    // No second-stage follower — K1 doesn't have one.
    m_wfPeakScaled = peakNorm;

    // tau=0.023s (was alpha=0.3/0.7 at 120 FPS)
    float aWfPk = 1.0f - expf(-m_dt / kTauWfPeakLast);
    m_wfPeakLast += (m_wfPeakScaled - m_wfPeakLast) * aWfPk;
#else
    (void)ctx;
#endif
}

// =========================================================================
// synthesizeChromaColor — 12-bin chromagram -> weighted color sum
// =========================================================================

void SbK1BaseEffect::synthesizeChromaColor(float squareIter, float saturation,
                                            float chroma, bool chromaticMode) {
    CRGB_F color;
    float totalMag = 0.0f;

    for (int c = 0; c < kChromaBins; ++c) {
        float bin = m_chromaSmooth[c];

        // Apply contrast curve
        bin = applyContrast(bin, squareIter);

        // Threshold: skip bins below 0.05
        if (bin < 0.05f) continue;

        totalMag += bin;

        float hue;
        if (chromaticMode) {
            // Bloom-style: note hue + 0.5 offset (complementary), plus hue position shift
            hue = kNoteHues[c] + 0.5f + chroma;
        } else {
            // Waveform-style: direct progression from chroma position
            float prog = (float)c / (float)kChromaBins;
            hue = prog + chroma;
        }

        // Wrap hue to [0, 1]
        hue = fmodf(hue, 1.0f);
        if (hue < 0.0f) hue += 1.0f;

        // Convert to RGB and accumulate weighted by bin magnitude
        CRGB_F noteColor = hsvToRgbF(hue, saturation, bin);
        color += noteColor;
    }

    m_chromaColor = color;
    m_chromaTotalMag = totalMag;
}

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
