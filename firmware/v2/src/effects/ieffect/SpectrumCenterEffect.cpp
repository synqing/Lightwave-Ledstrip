/**
 * @file SpectrumCenterEffect.cpp
 * @brief CENTER ORIGIN spectrum analyzer using all 64 FFT bins
 *
 * Implementation notes:
 * - Uses CENTER ORIGIN pattern: bass at center (79/80), treble at edges
 * - Dual strip rendering with +90 hue offset for visual differentiation
 * - Sensory Bridge asymmetric smoothing (50ms attack, 300ms release)
 * - Peak hold indicators that briefly highlight maximum values
 * - Beat pulse overlay that flashes center on detected beats
 */

#include "SpectrumCenterEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

// ============================================================================
// Lifecycle Methods
// ============================================================================

bool SpectrumCenterEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Initialize bin followers with Sensory Bridge time constants
    for (uint8_t i = 0; i < NUM_BINS; i++) {
        m_binFollowers[i] = enhancement::AsymmetricFollower(0.0f, RISE_TAU, FALL_TAU);
        m_smoothedBins[i] = 0.0f;
        m_targetBins[i] = 0.0f;
    }

    // Initialize peak hold state
    for (uint8_t i = 0; i < 80; i++) {
        m_peakValues[i] = 0.0f;
        m_peakHoldTimers[i] = 0.0f;
    }

    // Reset beat pulse state
    m_beatPulseIntensity = 0.0f;
    m_lastBeatPhase = 0.0f;
    m_lastHopSeq = 0;

    return true;
}

void SpectrumCenterEffect::render(plugins::EffectContext& ctx) {
    float dt = ctx.getSafeDeltaSeconds();
    float moodNorm = ctx.getMoodNormalized();

    // Clear buffer
    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));

#if !FEATURE_AUDIO_SYNC
    (void)moodNorm;
    // No audio: display dim baseline
    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        uint8_t hue = binToHue(distanceToBin(dist), ctx.gHue);
        CRGB color = ctx.palette.getColor(hue, 15);  // Very dim
        SET_CENTER_PAIR(ctx, dist, color);
    }
    return;
#else
    // =========================================================================
    // Phase 1: Update bin targets from audio (hop-based)
    // =========================================================================

    bool newHop = false;
    if (ctx.audio.available) {
        newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
            for (uint8_t b = 0; b < NUM_BINS; b++) {
                m_targetBins[b] = ctx.audio.bin(b);
            }
        }
    } else {
        // Fallback: gentle decay when no audio
        for (uint8_t b = 0; b < NUM_BINS; b++) {
            m_targetBins[b] *= 0.95f;
        }
    }

    // =========================================================================
    // Phase 2: Smooth bins with MOOD-adjusted asymmetric followers
    // =========================================================================

    for (uint8_t b = 0; b < NUM_BINS; b++) {
        m_smoothedBins[b] = m_binFollowers[b].updateWithMood(m_targetBins[b], dt, moodNorm);
    }

    // =========================================================================
    // Phase 3: Update beat pulse overlay
    // =========================================================================

    updateBeatPulse(ctx, dt);

    // =========================================================================
    // Phase 4: Render spectrum from CENTER ORIGIN
    // =========================================================================

    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        // Map distance to bin index (bass at center, treble at edge)
        uint8_t binIndex = distanceToBin(dist);

        // Get averaged bin value for smoother display
        float binValue = getAveragedBinValue(binIndex);

        // Apply bass boost for visibility (bins 0-15)
        if (binIndex < 16) {
            binValue *= 1.5f;
        }

        // Clamp to valid range
        binValue = fminf(1.0f, binValue);

        // Update peak hold for this position
        updatePeakHold((uint8_t)dist, binValue, dt);

        // Calculate brightness from bin value
        uint8_t baseBrightness = (uint8_t)(binValue * 255.0f);

        // Get hue from bin index (warm at center, cool at edges)
        uint8_t hue = binToHue(binIndex, ctx.gHue);

        // Get color from palette
        CRGB color = ctx.palette.getColor(hue, baseBrightness);

        // Scale by master brightness
        color = color.nscale8(ctx.brightness);

        // =====================================================================
        // Render to Strip 1 (LEDs 0-159)
        // =====================================================================

        uint16_t leftIdx1 = CENTER_LEFT - dist;
        uint16_t rightIdx1 = CENTER_RIGHT + dist;

        if (leftIdx1 < STRIP_LENGTH && leftIdx1 < ctx.ledCount) {
            ctx.leds[leftIdx1] = color;
        }
        if (rightIdx1 < STRIP_LENGTH && rightIdx1 < ctx.ledCount) {
            ctx.leds[rightIdx1] = color;
        }

        // =====================================================================
        // Render to Strip 2 (LEDs 160-319) with +90 hue offset
        // =====================================================================

        uint8_t hue2 = hue + 90;  // +90 degrees for visual differentiation
        CRGB color2 = ctx.palette.getColor(hue2, baseBrightness);
        color2 = color2.nscale8(ctx.brightness);

        uint16_t leftIdx2 = STRIP_LENGTH + CENTER_LEFT - dist;
        uint16_t rightIdx2 = STRIP_LENGTH + CENTER_RIGHT + dist;

        if (leftIdx2 < ctx.ledCount) {
            ctx.leds[leftIdx2] = color2;
        }
        if (rightIdx2 < ctx.ledCount) {
            ctx.leds[rightIdx2] = color2;
        }

        // =====================================================================
        // Overlay peak hold indicators
        // =====================================================================

        if (m_peakValues[dist] > binValue + 0.05f) {
            // Peak is significantly above current value - show indicator
            uint8_t peakBright = (uint8_t)(m_peakValues[dist] * 255.0f);
            peakBright = scale8(peakBright, ctx.brightness);

            CRGB peakColor = ctx.palette.getColor(hue, peakBright);

            // Brighten the peak indicator slightly
            peakColor = peakColor.nscale8(200);  // Slightly dimmer than full
            peakColor.r = qadd8(peakColor.r, 40);
            peakColor.g = qadd8(peakColor.g, 40);
            peakColor.b = qadd8(peakColor.b, 40);

            // Apply to all 4 LEDs (symmetric on both strips)
            if (leftIdx1 < ctx.ledCount) {
                ctx.leds[leftIdx1] = peakColor;
            }
            if (rightIdx1 < ctx.ledCount) {
                ctx.leds[rightIdx1] = peakColor;
            }
            if (leftIdx2 < ctx.ledCount) {
                CRGB peakColor2 = ctx.palette.getColor(hue2, peakBright);
                peakColor2.r = qadd8(peakColor2.r, 40);
                peakColor2.g = qadd8(peakColor2.g, 40);
                peakColor2.b = qadd8(peakColor2.b, 40);
                ctx.leds[leftIdx2] = peakColor2;
            }
            if (rightIdx2 < ctx.ledCount) {
                CRGB peakColor2 = ctx.palette.getColor(hue2, peakBright);
                peakColor2.r = qadd8(peakColor2.r, 40);
                peakColor2.g = qadd8(peakColor2.g, 40);
                peakColor2.b = qadd8(peakColor2.b, 40);
                ctx.leds[rightIdx2] = peakColor2;
            }
        }
    }

    // =========================================================================
    // Phase 5: Beat pulse overlay at center
    // =========================================================================

    if (m_beatPulseIntensity > 0.01f) {
        // Calculate pulse brightness
        uint8_t pulseBright = (uint8_t)(m_beatPulseIntensity * 255.0f);
        pulseBright = scale8(pulseBright, ctx.brightness);

        // Create pulse color (white/palette mix for punch)
        CRGB pulseColor = ctx.palette.getColor(ctx.gHue, pulseBright);

        // Add white component for extra punch
        uint8_t whiteAdd = (uint8_t)(m_beatPulseIntensity * 100.0f);
        pulseColor.r = qadd8(pulseColor.r, whiteAdd);
        pulseColor.g = qadd8(pulseColor.g, whiteAdd);
        pulseColor.b = qadd8(pulseColor.b, whiteAdd);

        // Apply to center region with falloff
        for (uint16_t dist = 0; dist < BEAT_PULSE_RADIUS; dist++) {
            // Calculate falloff from center
            float falloff = 1.0f - ((float)dist / (float)BEAT_PULSE_RADIUS);
            falloff = falloff * falloff;  // Quadratic falloff for tighter pulse

            uint8_t localBright = (uint8_t)(falloff * (float)pulseBright);

            CRGB localPulse = pulseColor;
            localPulse = localPulse.nscale8(localBright);

            // Add to existing colors (don't overwrite)
            uint16_t leftIdx1 = CENTER_LEFT - dist;
            uint16_t rightIdx1 = CENTER_RIGHT + dist;
            uint16_t leftIdx2 = STRIP_LENGTH + CENTER_LEFT - dist;
            uint16_t rightIdx2 = STRIP_LENGTH + CENTER_RIGHT + dist;

            if (leftIdx1 < ctx.ledCount) {
                ctx.leds[leftIdx1].r = qadd8(ctx.leds[leftIdx1].r, localPulse.r);
                ctx.leds[leftIdx1].g = qadd8(ctx.leds[leftIdx1].g, localPulse.g);
                ctx.leds[leftIdx1].b = qadd8(ctx.leds[leftIdx1].b, localPulse.b);
            }
            if (rightIdx1 < ctx.ledCount) {
                ctx.leds[rightIdx1].r = qadd8(ctx.leds[rightIdx1].r, localPulse.r);
                ctx.leds[rightIdx1].g = qadd8(ctx.leds[rightIdx1].g, localPulse.g);
                ctx.leds[rightIdx1].b = qadd8(ctx.leds[rightIdx1].b, localPulse.b);
            }

            // Strip 2 with offset hue
            CRGB localPulse2 = ctx.palette.getColor(ctx.gHue + 90, localBright);
            localPulse2.r = qadd8(localPulse2.r, (uint8_t)(falloff * whiteAdd));
            localPulse2.g = qadd8(localPulse2.g, (uint8_t)(falloff * whiteAdd));
            localPulse2.b = qadd8(localPulse2.b, (uint8_t)(falloff * whiteAdd));

            if (leftIdx2 < ctx.ledCount) {
                ctx.leds[leftIdx2].r = qadd8(ctx.leds[leftIdx2].r, localPulse2.r);
                ctx.leds[leftIdx2].g = qadd8(ctx.leds[leftIdx2].g, localPulse2.g);
                ctx.leds[leftIdx2].b = qadd8(ctx.leds[leftIdx2].b, localPulse2.b);
            }
            if (rightIdx2 < ctx.ledCount) {
                ctx.leds[rightIdx2].r = qadd8(ctx.leds[rightIdx2].r, localPulse2.r);
                ctx.leds[rightIdx2].g = qadd8(ctx.leds[rightIdx2].g, localPulse2.g);
                ctx.leds[rightIdx2].b = qadd8(ctx.leds[rightIdx2].b, localPulse2.b);
            }
        }
    }
#endif  // FEATURE_AUDIO_SYNC
}

void SpectrumCenterEffect::cleanup() {
    // No dynamically allocated resources to free
}

const plugins::EffectMetadata& SpectrumCenterEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Spectrum Center",
        "64-bin spectrum analyzer: bass at center, treble at edges",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

// ============================================================================
// Helper Methods
// ============================================================================

uint8_t SpectrumCenterEffect::distanceToBin(uint16_t dist) const {
    // Perceptual (quasi-logarithmic) mapping from distance to bin
    // This concentrates more resolution in the bass/mid range where human
    // hearing is most sensitive, and compresses the treble range.
    //
    // Distance 0-10:  bins 0-7   (sub-bass, 55-82 Hz)
    // Distance 11-25: bins 8-23  (bass/low-mids, 87-185 Hz)
    // Distance 26-50: bins 24-47 (mids/presence, 196-622 Hz)
    // Distance 51-79: bins 48-63 (treble/air, 659-2093 Hz)

    if (dist <= 10) {
        // Sub-bass: 11 positions -> 8 bins
        return (uint8_t)((dist * 8) / 11);
    } else if (dist <= 25) {
        // Bass/low-mids: 15 positions -> 16 bins
        return (uint8_t)(8 + ((dist - 11) * 16) / 15);
    } else if (dist <= 50) {
        // Mids/presence: 25 positions -> 24 bins
        return (uint8_t)(24 + ((dist - 26) * 24) / 25);
    } else {
        // Treble/air: 29 positions -> 16 bins
        return (uint8_t)(48 + ((dist - 51) * 16) / 29);
    }
}

float SpectrumCenterEffect::getAveragedBinValue(uint8_t binIndex) const {
    // Average with adjacent bins for smoother display
    // This reduces single-bin noise that can cause flickering

    float sum = m_smoothedBins[binIndex];
    uint8_t count = 1;

    if (binIndex > 0) {
        sum += m_smoothedBins[binIndex - 1];
        count++;
    }
    if (binIndex < NUM_BINS - 1) {
        sum += m_smoothedBins[binIndex + 1];
        count++;
    }

    return sum / (float)count;
}

uint8_t SpectrumCenterEffect::binToHue(uint8_t binIndex, uint8_t baseHue) const {
    // Map bin index to hue with warm-to-cool gradient
    // Bin 0 (bass) = warm (near baseHue)
    // Bin 63 (treble) = cool (baseHue + spread)
    //
    // We spread across ~160 degrees (5/9 of color wheel) to avoid
    // creating a rainbow effect (explicit project constraint)

    uint8_t spread = (uint8_t)((binIndex * 160) / 63);
    return baseHue + spread;
}

void SpectrumCenterEffect::updatePeakHold(uint8_t posIndex, float newValue, float dt) {
    if (posIndex >= 80) return;

    if (newValue >= m_peakValues[posIndex]) {
        // New peak: update value and reset hold timer
        m_peakValues[posIndex] = newValue;
        m_peakHoldTimers[posIndex] = PEAK_HOLD_TIME;
    } else if (m_peakHoldTimers[posIndex] > 0) {
        // In hold phase: decrement timer
        m_peakHoldTimers[posIndex] -= dt;
    } else {
        // Decay phase: exponential decay
        float decayAlpha = 1.0f - expf(-PEAK_DECAY_RATE * dt);
        m_peakValues[posIndex] -= m_peakValues[posIndex] * decayAlpha;

        // Clamp to zero
        if (m_peakValues[posIndex] < 0.01f) {
            m_peakValues[posIndex] = 0.0f;
        }
    }
}

void SpectrumCenterEffect::updateBeatPulse(const plugins::EffectContext& ctx, float dt) {
#if FEATURE_AUDIO_SYNC
    if (!ctx.audio.available) {
        // Decay pulse when no audio
        float decayAlpha = 1.0f - expf(-dt / BEAT_PULSE_DECAY);
        m_beatPulseIntensity -= m_beatPulseIntensity * decayAlpha;
        return;
    }

    // Detect beat using beat tick (single-frame pulse from AudioNode)
    if (ctx.audio.isOnBeat()) {
        // Beat detected! Trigger pulse based on beat strength
        float targetIntensity = ctx.audio.beatStrength();

        // Boost with bass energy for visceral impact
        float bassBoost = ctx.audio.bass() * 0.3f;
        targetIntensity = fminf(1.0f, targetIntensity + bassBoost);

        // Fast rise to target
        float riseAlpha = 1.0f - expf(-dt / BEAT_PULSE_ATTACK);
        m_beatPulseIntensity += (targetIntensity - m_beatPulseIntensity) * riseAlpha;
    } else {
        // No beat: decay
        float decayAlpha = 1.0f - expf(-dt / BEAT_PULSE_DECAY);
        m_beatPulseIntensity -= m_beatPulseIntensity * decayAlpha;
    }

    // Clamp
    if (m_beatPulseIntensity < 0.01f) {
        m_beatPulseIntensity = 0.0f;
    }

    m_lastBeatPhase = ctx.audio.beatPhase();
#else
    (void)ctx;
    (void)dt;
    m_beatPulseIntensity = 0.0f;
#endif
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
