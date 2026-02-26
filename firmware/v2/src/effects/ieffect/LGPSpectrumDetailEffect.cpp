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


// AUTO_TUNABLES_BULK_BEGIN:LGPSpectrumDetailEffect
namespace {
constexpr float kLGPSpectrumDetailEffectSpeedScale = 1.0f;
constexpr float kLGPSpectrumDetailEffectOutputGain = 1.0f;
constexpr float kLGPSpectrumDetailEffectCentreBias = 1.0f;

float gLGPSpectrumDetailEffectSpeedScale = kLGPSpectrumDetailEffectSpeedScale;
float gLGPSpectrumDetailEffectOutputGain = kLGPSpectrumDetailEffectOutputGain;
float gLGPSpectrumDetailEffectCentreBias = kLGPSpectrumDetailEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPSpectrumDetailEffectParameters[] = {
    {"lgpspectrum_detail_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPSpectrumDetailEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpspectrum_detail_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPSpectrumDetailEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpspectrum_detail_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPSpectrumDetailEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPSpectrumDetailEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {
bool  LGPSpectrumDetailEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPSpectrumDetailEffect
    gLGPSpectrumDetailEffectSpeedScale = kLGPSpectrumDetailEffectSpeedScale;
    gLGPSpectrumDetailEffectOutputGain = kLGPSpectrumDetailEffectOutputGain;
    gLGPSpectrumDetailEffectCentreBias = kLGPSpectrumDetailEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPSpectrumDetailEffect

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
    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));

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

    // Prefer adaptive normalisation (Sensory Bridge max follower); fall back to raw bins if unavailable.
    const float* bins64 = ctx.audio.bins64Adaptive();
    if (!bins64) {
        bins64 = ctx.audio.bins64();
    }
    constexpr uint8_t NUM_BINS = 64;
    
    float rawDt = ctx.getSafeRawDeltaSeconds();
    float dt = ctx.getSafeDeltaSeconds();
    float moodNorm = ctx.getMoodNormalized();
    
    // Hop-based updates: update targets only on new hops
    bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
    if (newHop) {
        m_lastHopSeq = ctx.audio.controlBus.hop_seq;
        float maxBin = 0.0f;
        for (uint8_t bin = 0; bin < NUM_BINS; ++bin) {
            m_ps->targetBins[bin] = bins64[bin];
            if (bins64[bin] > maxBin) maxBin = bins64[bin];
        }
        // #region agent log
        #ifndef NATIVE_BUILD
        if (debugNowMs - lastHopLogMs >= 10000) {  // Every 10 seconds
            lastHopLogMs = debugNowMs;
            Serial.printf("{\"id\":\"spectrum_new_hop\",\"location\":\"LGPSpectrumDetailEffect.cpp:58\",\"message\":\"New hop detected\",\"data\":{\"hopSeq\":%u,\"maxBin\":%.4f,\"bins64[0]\":%.4f,\"bins64[31]\":%.4f,\"bins64[63]\":%.4f},\"timestamp\":%lu}\n",
                m_lastHopSeq, maxBin, bins64[0], bins64[31], bins64[63], debugNowMs);
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
    // Render: Logarithmic mapping of 64 bins to LED positions
    // Low frequencies (bins 0-15) near center, high frequencies (bins 48-63) at edges
    // =========================================================================
    
    // Clear buffer first (already done above)
    
    // Map each bin to LED positions using logarithmic spacing
    uint16_t ledsWritten = 0;
    uint8_t maxBright = 0;
    for (uint8_t bin = 0; bin < NUM_BINS; ++bin) {
        float magnitude = m_ps->binSmoothing[bin];

        // VISIBILITY FIX: Apply 2x gain boost to bass bins (0-15) for better visibility
        if (bin < 16) {
            magnitude *= 2.0f;
        }

        if (magnitude < 0.01f) {
            continue;  // Skip bins with no energy
        }
        
        // Map bin to LED distance from center (logarithmic)
        uint16_t ledDist = binToLedDistance(bin);
        
        // Get color for this frequency band (using palette system)
        CRGB color = frequencyToColor(bin, ctx);
        
        // Scale brightness by magnitude and master brightness
        uint8_t bright = (uint8_t)(magnitude * ctx.brightness);
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
    // Use palette system instead of hardcoded HSV rainbow
    // Matches SensoryBridge pattern: chromatic_mode determines color source
    const bool chromaticMode = (ctx.saturation >= 128);
    
    if (chromaticMode) {
        // Chromatic mode: Map bin to palette position (rainbow-like cycling)
        // SensoryBridge uses 21.333 * i, we'll use bin/64 * 255 for palette index
        float prog = (float)bin / 64.0f;
        uint8_t paletteIdx = (uint8_t)(prog * 255.0f + ctx.gHue);
        return ctx.palette.getColor(paletteIdx, 255);
    } else {
        // Non-chromatic mode: Single hue from palette
        return ctx.palette.getColor(ctx.gHue, 255);
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPSpectrumDetailEffect
uint8_t LGPSpectrumDetailEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPSpectrumDetailEffectParameters) / sizeof(kLGPSpectrumDetailEffectParameters[0]));
}

const plugins::EffectParameter* LGPSpectrumDetailEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPSpectrumDetailEffectParameters[index];
}

bool LGPSpectrumDetailEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpspectrum_detail_effect_speed_scale") == 0) {
        gLGPSpectrumDetailEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpspectrum_detail_effect_output_gain") == 0) {
        gLGPSpectrumDetailEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpspectrum_detail_effect_centre_bias") == 0) {
        gLGPSpectrumDetailEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPSpectrumDetailEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpspectrum_detail_effect_speed_scale") == 0) return gLGPSpectrumDetailEffectSpeedScale;
    if (strcmp(name, "lgpspectrum_detail_effect_output_gain") == 0) return gLGPSpectrumDetailEffectOutputGain;
    if (strcmp(name, "lgpspectrum_detail_effect_centre_bias") == 0) return gLGPSpectrumDetailEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPSpectrumDetailEffect

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
