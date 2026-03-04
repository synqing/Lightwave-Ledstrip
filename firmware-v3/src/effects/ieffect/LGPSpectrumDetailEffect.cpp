/**
 * @file LGPSpectrumDetailEffect.cpp
 * @brief Detailed 64-bin FFT spectrum visualization with logarithmic mapping
 */

#include "LGPSpectrumDetailEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"

#ifdef FEATURE_EFFECT_VALIDATION
#include "../../validation/EffectValidationMacros.h"
#endif

#ifndef NATIVE_BUILD
#include <FastLED.h>
#include <esp_heap_caps.h>
#endif

#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool LGPSpectrumDetailEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<SpectrumDetailPsram*>(
            heap_caps_malloc(sizeof(SpectrumDetailPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) return false;
    }
    memset(m_ps, 0, sizeof(SpectrumDetailPsram));
    for (uint8_t i = 0; i < 64; i++) {
        m_ps->binFollowers[i].reset(0.0f);
    }
#endif
    m_lastHopSeq = 0;
    return true;
}

void LGPSpectrumDetailEffect::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;
    // #region agent log
    #ifndef NATIVE_BUILD
    static uint32_t lastRenderLogMs = 0;
    static uint32_t lastHopLogMs = 0;
    static uint32_t lastStatsLogMs = 0;
    uint32_t debugNowMs = millis();
    if (debugNowMs - lastRenderLogMs >= 10000) {  // Every 10 seconds
        lastRenderLogMs = debugNowMs;
        Serial.printf("{\"id\":\"spectrum_render_entry\",\"location\":\"LGPSpectrumDetailEffect.cpp:37\",\"message\":\"Render called\",\"data\":{\"audioAvailable\":%d,\"brightness\":%u,\"ledCount\":%u,\"centerPoint\":%u},\"timestamp\":%lu}\n",
            ctx.audio.available ? 1 : 0, ctx.brightness, ctx.ledCount, ctx.centerPoint, debugNowMs);
    }
    #endif
    // #endregion
    
    // Clear buffer
    fadeToBlackBy(ctx.leds, ctx.ledCount, 30);

#if !FEATURE_AUDIO_SYNC
    (void)ctx;
    return;
#else
    if (!ctx.audio.available) {
        // #region agent log
        #ifndef NATIVE_BUILD
        static uint32_t noAudioCount = 0;
        noAudioCount++;
        if (noAudioCount % 60 == 0) {
            Serial.printf("{\"id\":\"spectrum_no_audio\",\"location\":\"LGPSpectrumDetailEffect.cpp:45\",\"message\":\"Audio not available\",\"data\":{\"count\":%u},\"timestamp\":%lu}\n",
                noAudioCount, millis());
        }
        #endif
        // #endregion
        return;
    }

    // Primary source: full-resolution 256-bin FFT from PipelineCore.
    // Fallback source: 64-bin adaptive Goertzel spectrum.
    const float* bins64 = ctx.audio.bins64Adaptive();
    if (!bins64) bins64 = ctx.audio.bins64();
    const float* bins256 = nullptr;
    float binHz = 0.0f;
#if FEATURE_AUDIO_BACKEND_PIPELINECORE
    bins256 = ctx.audio.bins256();
    binHz = ctx.audio.binHz();
#endif
    const bool useBins256 = (bins256 != nullptr) && (binHz > 0.0f);
    constexpr uint8_t NUM_BINS = 64;
    
    float rawDt = ctx.getSafeRawDeltaSeconds();
    float dt = ctx.getSafeDeltaSeconds();
    float moodNorm = ctx.getMoodNormalized();
    
    // Hop-based updates: update targets only on new hops
    bool newHop = (ctx.audio.hopSequence() != m_lastHopSeq);
    if (newHop) {
        m_lastHopSeq = ctx.audio.hopSequence();
        float maxBin = 0.0f;
        if (useBins256) {
            constexpr float kMinHz = 62.5f;
            constexpr float kMaxHz = 12000.0f;
            const float logMin = log10f(kMinHz);
            const float logMax = log10f(kMaxHz);
            constexpr uint16_t kFftBins = 256;

            for (uint8_t bin = 0; bin < NUM_BINS; ++bin) {
                const float t0 = static_cast<float>(bin) / static_cast<float>(NUM_BINS);
                const float t1 = static_cast<float>(bin + 1) / static_cast<float>(NUM_BINS);

                const float hzLo = powf(10.0f, logMin + (logMax - logMin) * t0);
                const float hzHi = powf(10.0f, logMin + (logMax - logMin) * t1);

                uint16_t lo = static_cast<uint16_t>(hzLo / binHz);
                uint16_t hi = static_cast<uint16_t>(hzHi / binHz);
                if (lo >= kFftBins) lo = kFftBins - 1;
                if (hi <= lo) hi = static_cast<uint16_t>(lo + 1);
                if (hi > kFftBins) hi = kFftBins;

                float acc = 0.0f;
                uint16_t count = 0;
                for (uint16_t i = lo; i < hi; ++i) {
                    acc += bins256[i];
                    ++count;
                }
                const float v = (count > 0) ? (acc / static_cast<float>(count)) : 0.0f;
                m_ps->targetBins[bin] = v;
                if (v > maxBin) maxBin = v;
            }
        } else {
            for (uint8_t bin = 0; bin < NUM_BINS; ++bin) {
                const float v = bins64 ? bins64[bin] : 0.0f;
                m_ps->targetBins[bin] = v;
                if (v > maxBin) maxBin = v;
            }
        }
        // #region agent log
        #ifndef NATIVE_BUILD
        if (debugNowMs - lastHopLogMs >= 10000) {  // Every 10 seconds
            lastHopLogMs = debugNowMs;
            Serial.printf("{\"id\":\"spectrum_new_hop\",\"location\":\"LGPSpectrumDetailEffect.cpp\",\"message\":\"New hop detected\",\"data\":{\"hopSeq\":%u,\"maxBin\":%.4f,\"source\":\"%s\",\"binHz\":%.2f},\"timestamp\":%lu}\n",
                m_lastHopSeq, maxBin, useBins256 ? "bins256" : "bins64", binHz, debugNowMs);
        }
        #endif
        // #endregion
    }
    
    // Smooth toward targets every frame with MOOD-adjusted smoothing
    float maxSmooth = 0.0f;
    uint8_t binsAboveThreshold = 0;
    for (uint8_t bin = 0; bin < NUM_BINS; ++bin) {
        m_ps->binSmoothing[bin] = m_ps->binFollowers[bin].updateWithMood(m_ps->targetBins[bin], rawDt, moodNorm);
        if (m_ps->binSmoothing[bin] > maxSmooth) maxSmooth = m_ps->binSmoothing[bin];
        if (m_ps->binSmoothing[bin] >= 0.01f) binsAboveThreshold++;
    }
    
    // =========================================================================
    // Render: Logarithmic mapping of 64 visual bins to LED positions
    // Low frequencies (bins 0-15) near center, high frequencies (bins 48-63) at edges
    // =========================================================================
    
    // Clear buffer first (already done above)
    
    // Map each bin to LED positions using logarithmic spacing
    uint16_t ledsWritten = 0;
    uint8_t maxBright = 0;
    const bool pulseBehaviour = ctx.audio.shouldPulseOnBeat();
    const bool textureBehaviour = ctx.audio.shouldTextureFlow();
    const float styleConfidence = ctx.audio.styleConfidence();
    const float behaviourGain = pulseBehaviour ? (ctx.audio.isOnBeat() ? 1.35f : 0.95f)
                                               : (textureBehaviour ? 0.92f : 1.0f);
    for (uint8_t bin = 0; bin < NUM_BINS; ++bin) {
        float magnitude = m_ps->binSmoothing[bin] * behaviourGain;
        magnitude = fmaxf(0.0f, fminf(1.0f, magnitude));

        // Apply gentle bass assist to avoid low-bin under-representation.
        if (bin < 16) {
            magnitude = fminf(1.0f, magnitude * 1.6f);
        }

        if (magnitude < 0.01f) {
            continue;  // Skip bins with no energy
        }
        
        // Map bin to LED distance from center (logarithmic)
        uint16_t ledDist = binToLedDistance(bin);
        
        // Get color for this frequency band (using palette system)
        CRGB color = frequencyToColor(bin, ctx);
        
        // Scale brightness by magnitude and master brightness
        const float confidenceBoost = 0.85f + 0.15f * styleConfidence;
        uint8_t bright = (uint8_t)(magnitude * confidenceBoost * ctx.brightness);
        if (bright > maxBright) maxBright = bright;
        color = color.nscale8(bright);

        // Pre-scale band contribution so multiple bins hitting same LED stay in range (colour corruption fix)
        constexpr uint8_t SPECTRUM_PRE_SCALE = 85;  // ~3 overlapping bands sum to 255
        color = color.nscale8(SPECTRUM_PRE_SCALE);
        
        // Apply to center pair (symmetric)
        uint16_t centerLED = ctx.centerPoint;
        uint16_t leftIdx = centerLED - 1 - ledDist;
        uint16_t rightIdx = centerLED + ledDist;
        
        if (leftIdx < ctx.ledCount) {
            ctx.leds[leftIdx] += color;
            ledsWritten++;
        }
        if (rightIdx < ctx.ledCount) {
            ctx.leds[rightIdx] += color;
            ledsWritten++;
        }
        
        // Also apply to adjacent LEDs for smoother visualization (optional)
        // This creates a "bar" effect for each frequency band
        if (ledDist > 0) {
            uint16_t leftIdx2 = centerLED - 1 - (ledDist - 1);
            uint16_t rightIdx2 = centerLED + (ledDist - 1);

            CRGB color2 = color.nscale8(128);  // Dimmer for adjacent (already pre-scaled)

            if (leftIdx2 < ctx.ledCount) {
                ctx.leds[leftIdx2] += color2;
            }
            if (rightIdx2 < ctx.ledCount) {
                ctx.leds[rightIdx2] += color2;
            }
        }

        // Strip 2: Centre-origin at LED 240
        if (ctx.ledCount >= 320) {
            uint16_t strip2Center = 240;
            uint16_t leftIdx3 = strip2Center - 1 - ledDist;
            uint16_t rightIdx3 = strip2Center + ledDist;

            // Use complementary hue offset for strip 2 (same pre-scale as strip 1)
            CRGB color3 = frequencyToColor(bin, ctx);
            color3 = color3.nscale8(bright).nscale8(SPECTRUM_PRE_SCALE);

            if (leftIdx3 >= 160 && leftIdx3 < ctx.ledCount) {
                ctx.leds[leftIdx3] += color3;
            }
            if (rightIdx3 < ctx.ledCount) {
                ctx.leds[rightIdx3] += color3;
            }
        }
    }
    
    // #region agent log
    #ifndef NATIVE_BUILD
    if (debugNowMs - lastStatsLogMs >= 10000) {  // Every 10 seconds
        lastStatsLogMs = debugNowMs;
        Serial.printf("{\"id\":\"spectrum_render_stats\",\"location\":\"LGPSpectrumDetailEffect.cpp:162\",\"message\":\"Render stats\",\"data\":{\"maxSmooth\":%.4f,\"binsAboveThreshold\":%u,\"ledsWritten\":%u,\"maxBright\":%u,\"brightness\":%u,\"colorR\":%u,\"colorG\":%u,\"colorB\":%u},\"timestamp\":%lu}\n",
            maxSmooth, binsAboveThreshold, ledsWritten, maxBright, ctx.brightness, 
            ledsWritten > 0 ? ctx.leds[ctx.centerPoint].r : 0,
            ledsWritten > 0 ? ctx.leds[ctx.centerPoint].g : 0,
            ledsWritten > 0 ? ctx.leds[ctx.centerPoint].b : 0, debugNowMs);
    }
    #endif
    // #endregion
#endif  // FEATURE_AUDIO_SYNC
}

uint16_t LGPSpectrumDetailEffect::binToLedDistance(uint8_t bin) const {
    // Logarithmic mapping: lower bins closer to center
    // bin 0 (110 Hz) -> distance 0 (center)
    // bin 63 (4186 Hz) -> distance ~79 (edge)
    
    if (bin == 0) {
        return 0;
    }
    
    // Logarithmic scale: log10((bin + 1) / 64) * maxDistance
    // This compresses low frequencies near center, spreads high frequencies toward edges
    float logPos = log10f((float)(bin + 1) / 64.0f);
    
    // Map to LED distance (0 to HALF_LENGTH-1)
    // log10(1/64) ≈ -1.806, log10(64/64) = 0
    // Normalize: (logPos - log10(1/64)) / (0 - log10(1/64))
    float normalized = (logPos + 1.806f) / 1.806f;  // Maps -1.806..0 to 0..1
    normalized = fmaxf(0.0f, fminf(1.0f, normalized));  // Clamp
    
    uint16_t distance = (uint16_t)(normalized * (float)(HALF_LENGTH - 1));
    
    return distance;
}

CRGB LGPSpectrumDetailEffect::frequencyToColor(uint8_t bin, const plugins::EffectContext& ctx) const {
    // Constrained tonal mapping: keeps colour motion expressive without full hue-wheel sweeps.
    const float prog = static_cast<float>(bin) / 63.0f;

    float anchor = 20.0f;
    float window = 84.0f;
    if (ctx.audio.shouldPulseOnBeat()) {
        anchor = 8.0f;
        window = 64.0f;
    } else if (ctx.audio.shouldDriftWithHarmony()) {
        anchor = 32.0f;
        window = 72.0f;
    } else if (ctx.audio.shouldShimmerWithMelody()) {
        anchor = 78.0f;
        window = 56.0f;
    } else if (ctx.audio.shouldTextureFlow()) {
        anchor = 18.0f;
        window = 48.0f;
    }

    const float confidence = fmaxf(0.0f, fminf(1.0f, ctx.audio.styleConfidence()));
    window *= (0.65f + 0.35f * confidence);

    const float beatNudge = ctx.audio.isOnBeat() ? 7.0f : 0.0f;
    const float paletteFloat = anchor + prog * window + beatNudge;
    const uint8_t paletteIdx = static_cast<uint8_t>(static_cast<uint16_t>(paletteFloat) & 0xFFu);
    return ctx.palette.getColor(paletteIdx, 255);
}

void LGPSpectrumDetailEffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
}

const plugins::EffectMetadata& LGPSpectrumDetailEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Spectrum Detail",
        "Detailed 64-bin FFT spectrum visualization with logarithmic frequency mapping",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
