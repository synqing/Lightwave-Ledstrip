/**
 * @file SpectrumAnalyzerEffect.cpp
 * @brief Classic frequency spectrum analyzer implementation
 *
 * Uses 64-bin Goertzel spectrum for full musical range visualization.
 * Center-origin: bass at center (LEDs 79/80), treble at edges.
 */

#include "SpectrumAnalyzerEffect.h"
#include "../CoreEffects.h"
#include "../enhancement/SmoothingEngine.h"
#include "../../config/features.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>
#include <cstring>
#include <algorithm>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool SpectrumAnalyzerEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // Initialize smoothing followers
    for (uint8_t i = 0; i < 64; i++) {
        m_binFollowers[i].reset(0.0f);
        m_targetBins[i] = 0.0f;
        m_binSmoothing[i] = 0.0f;
    }
    m_lastHopSeq = 0;
    memset(m_peakHold, 0, sizeof(m_peakHold));
    memset(m_peakHoldTime, 0, sizeof(m_peakHoldTime));
    m_beatSyncMode = false;
    m_beatSyncPhase = 0.0f;
    m_phase = 0.0f;
    return true;
}

void SpectrumAnalyzerEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Standing wave pattern with frequency-dependent spatial frequency
    // Visual Foundation: Standing waves where spatial frequency is modulated by audio bins
    // Bass frequencies (low bins) create low spatial frequency patterns at center
    // Treble frequencies (high bins) create high spatial frequency patterns at edges
    
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

#if !FEATURE_AUDIO_SYNC
    // Fallback: simple standing wave pattern
    float speedNorm = ctx.speed / 50.0f;
    float dt = ctx.getSafeDeltaSeconds();
    m_phase += speedNorm * 2.0f * dt;
    
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        float distNorm = (float)dist / (float)HALF_LENGTH;
        // Low spatial frequency at center, high at edges
        float spatialFreq = 0.5f + distNorm * 3.0f;
        float wave = sinf(distNorm * spatialFreq * 2.0f * 3.14159f - phase);
        uint8_t bright = (uint8_t)(128 + 127 * wave);
        CRGB color = ctx.palette.getColor(ctx.gHue + (uint8_t)(distNorm * 50.0f), bright);
        SET_CENTER_PAIR(ctx, dist, color);
    }
    return;
#else
    if (!ctx.audio.available) {
        // Fallback when audio unavailable
        float speedNorm = ctx.speed / 50.0f;
        float dt = ctx.getSafeDeltaSeconds();
        m_phase += speedNorm * 2.0f * dt;
        
        for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
            float distNorm = (float)dist / (float)HALF_LENGTH;
            float spatialFreq = 0.5f + distNorm * 3.0f;
            float wave = sinf(distNorm * spatialFreq * 2.0f * 3.14159f - m_phase);
            uint8_t bright = (uint8_t)(128 + 127 * wave);
            CRGB color = ctx.palette.getColor(ctx.gHue + (uint8_t)(distNorm * 50.0f), bright);
            SET_CENTER_PAIR(ctx, dist, color);
        }
        return;
    }

    // =========================================================================
    // Visual Foundation: TIME-BASED phase (prevents jitter)
    // =========================================================================
    float speedNorm = ctx.speed / 50.0f;
    float dt = ctx.getSafeDeltaSeconds();
    m_phase += speedNorm * 2.0f * dt;  // Time-based only
    if (m_phase > 2.0f * 3.14159f) m_phase -= 2.0f * 3.14159f;

    // Beat-sync mode: use beat phase to time-stretch display
    m_beatSyncMode = (ctx.speed > 75);
    if (m_beatSyncMode) {
        m_beatSyncPhase = ctx.audio.beatPhase();
    } else {
        m_beatSyncPhase = 0.0f;
    }

    // =========================================================================
    // Audio Enhancement: Get 64-bin spectrum and smooth
    // =========================================================================
    const float* bins64 = ctx.audio.bins64();
    constexpr uint8_t NUM_BINS = 64;
    // Reuse dt from above (line 89)
    float moodNorm = ctx.getMoodNormalized();
    
    // Hop-based updates: update targets only on new hops
    bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
    if (newHop) {
        m_lastHopSeq = ctx.audio.controlBus.hop_seq;
        for (uint8_t bin = 0; bin < NUM_BINS; ++bin) {
            m_targetBins[bin] = bins64[bin];
        }
    }
    
    // Smooth toward targets every frame with MOOD-adjusted smoothing
    uint32_t now = ctx.totalTimeMs;
    for (uint8_t bin = 0; bin < NUM_BINS; ++bin) {
        m_binSmoothing[bin] = m_binFollowers[bin].updateWithMood(m_targetBins[bin], dt, moodNorm);
        
        // Update peak hold
        if (m_binSmoothing[bin] > m_peakHold[bin]) {
            m_peakHold[bin] = m_binSmoothing[bin];
            m_peakHoldTime[bin] = now;
        } else if (now - m_peakHoldTime[bin] > PEAK_HOLD_DURATION_MS) {
            // Decay peak hold after duration
            m_peakHold[bin] *= 0.95f;
            if (m_peakHold[bin] < m_binSmoothing[bin]) {
                m_peakHold[bin] = m_binSmoothing[bin];
            }
        }
    }
    
    // =========================================================================
    // Render: Standing wave pattern with audio-modulated spatial frequency
    // =========================================================================
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        float distNorm = (float)dist / (float)HALF_LENGTH;
        
        // Map distance to bin index (bass at center, treble at edges)
        float binF = distNorm * (float)(NUM_BINS - 1);
        uint8_t binIdx = (uint8_t)binF;
        if (binIdx >= NUM_BINS) binIdx = NUM_BINS - 1;
        
        // Reverse: bin 0 (bass) at center, bin 63 (treble) at edge
        uint8_t reversedBin = NUM_BINS - 1 - binIdx;
        
        // Get audio magnitude for this frequency region
        float magnitude = m_binSmoothing[reversedBin];
        float peak = m_peakHold[reversedBin];
        
        // Base spatial frequency: low at center (bass), high at edges (treble)
        // This creates the visual foundation: standing wave pattern
        float baseSpatialFreq = 0.3f + distNorm * 4.0f;  // 0.3 to 4.3 cycles across strip
        
        // Audio modulates spatial frequency: stronger frequencies create more visible patterns
        // Magnitude scales the spatial frequency visibility (not the frequency itself)
        float audioModulatedFreq = baseSpatialFreq * (0.5f + magnitude * 0.5f);
        
        // Standing wave equation: sin(k*dist - phase) produces OUTWARD motion
        float wavePhase = m_phase;
        if (m_beatSyncMode) {
            // Beat-sync: time-stretch using beat phase
            wavePhase = m_beatSyncPhase * 2.0f * 3.14159f;
        }
        
        float wave = sinf(distNorm * audioModulatedFreq * 2.0f * 3.14159f - wavePhase);
        
        // Audio modulates brightness (not speed!)
        float audioGain = 0.4f + magnitude * 0.6f;
        float brightness = wave * audioGain;
        
        // Add peak hold indicator
        if (peak > magnitude * 1.1f) {
            float peakIndicator = (peak - magnitude) * 0.3f;
            brightness += peakIndicator;
        }
        
        // Normalize brightness
        brightness = tanhf(brightness * 2.0f) * 0.5f + 0.5f;
        brightness = fminf(1.0f, brightness);
        
        // Color: hue shifts with frequency (bin index)
        uint8_t hue = ctx.gHue + (uint8_t)(reversedBin * 4);  // 4 hue units per bin
        
        // Brightness scaled by master brightness
        uint8_t bright = (uint8_t)(brightness * ctx.brightness);
        
        // Get color from palette
        CRGB color = ctx.palette.getColor(hue, bright);
        
        // Set center pair (symmetric)
        SET_CENTER_PAIR(ctx, dist, color);
    }
    
    // Beat pulse overlay at center
    if (ctx.audio.isOnBeat()) {
        float beatStrength = ctx.audio.beatStrength();
        if (beatStrength > 0.3f) {
            uint8_t boost = (uint8_t)(beatStrength * 40.0f);
            for (uint16_t dist = 0; dist < 5; ++dist) {
                float fade = 1.0f - ((float)dist / 5.0f);
                uint8_t fadedBoost = (uint8_t)(boost * fade);
                
                uint16_t leftIdx = ctx.centerPoint - 1 - dist;
                uint16_t rightIdx = ctx.centerPoint + dist;
                
                if (leftIdx < ctx.ledCount) {
                    ctx.leds[leftIdx].r = qadd8(ctx.leds[leftIdx].r, fadedBoost);
                    ctx.leds[leftIdx].g = qadd8(ctx.leds[leftIdx].g, fadedBoost);
                    ctx.leds[leftIdx].b = qadd8(ctx.leds[leftIdx].b, fadedBoost);
                }
                if (rightIdx < ctx.ledCount) {
                    ctx.leds[rightIdx].r = qadd8(ctx.leds[rightIdx].r, fadedBoost);
                    ctx.leds[rightIdx].g = qadd8(ctx.leds[rightIdx].g, fadedBoost);
                    ctx.leds[rightIdx].b = qadd8(ctx.leds[rightIdx].b, fadedBoost);
                }
            }
        }
    }
#endif  // FEATURE_AUDIO_SYNC
}

void SpectrumAnalyzerEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& SpectrumAnalyzerEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Spectrum Analyzer",
        "64-bin frequency spectrum visualization, bass at center, treble at edges",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

