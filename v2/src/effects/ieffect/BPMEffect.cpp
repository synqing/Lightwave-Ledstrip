/**
 * @file BPMEffect.cpp
 * @brief BPM effect implementation - phase-locked to detected tempo
 *
 * Uses K1 beat PHASE (smooth 0.0-1.0) instead of raw BPM value.
 * This eliminates jitter from BPM detection uncertainty (±5-10 BPM).
 * Pattern: Beat-Sync (phase-locked motion, audio→color, energy→brightness)
 *
 * Audio Integration (Sensory Bridge pattern):
 * - Chromagram → Color: Musical pitch content drives visual color
 * - RMS Energy → Brightness: Overall audio energy modulates brightness
 * - Beat Phase → Motion: Smooth phase-locked pulsing (no jitter)
 * - BPM-Adaptive Decay: Beat pulse decay matches musical tempo
 */

#include "BPMEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <math.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
    /**
     * @brief Compute chromatic color from 12-bin chromagram (Sensory Bridge pattern)
     * 
     * Converts musical pitch content into visual color by summing contributions
     * from all 12 pitch classes (C, C#, D, ..., B). Each pitch class contributes
     * its energy as a hue-saturated color, creating a musically-responsive palette.
     * 
     * @param chroma 12-bin chromagram array [0.0-1.0] for each pitch class
     * @return Combined RGB color representing musical content
     */
    static CRGB computeChromaticColor(const float chroma[12]) {
        CRGB sum = CRGB::Black;
        float share = 1.0f / 6.0f;  // Divide brightness among notes (Sensory Bridge uses 1/6.0)
        
        for (int i = 0; i < 12; i++) {
            float hue = i / 12.0f;  // 0.0 to 0.917 (0° to 330°)
            float brightness = chroma[i] * chroma[i] * share;  // Quadratic contrast (like Sensory Bridge)
            
            // Clamp brightness to valid range
            if (brightness > 1.0f) brightness = 1.0f;
            
            CRGB noteColor;
            hsv2rgb_spectrum(CHSV((uint8_t)(hue * 255), 255, (uint8_t)(brightness * 255)), noteColor);

            // PRE-SCALE: Prevent white accumulation from 12-bin chromagram sum
            // With 12 bins at full intensity, worst case is 12 * 255 = 3060
            // Scale by ~70% (180/255) to leave headroom for accumulation
            constexpr uint8_t PRE_SCALE = 180;
            noteColor.r = scale8(noteColor.r, PRE_SCALE);
            noteColor.g = scale8(noteColor.g, PRE_SCALE);
            noteColor.b = scale8(noteColor.b, PRE_SCALE);

            // Accumulate color contributions (now with reduced intensity)
            sum.r = qadd8(sum.r, noteColor.r);
            sum.g = qadd8(sum.g, noteColor.g);
            sum.b = qadd8(sum.b, noteColor.b);
        }
        
        // Clamp to valid range (qadd8 already handles overflow, but be safe)
        if (sum.r > 255) sum.r = 255;
        if (sum.g > 255) sum.g = 255;
        if (sum.b > 255) sum.b = 255;
        
        return sum;
    }
}

BPMEffect::BPMEffect()
    : m_beatPulse(0.0f)
{
}

bool BPMEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_beatPulse = 0.0f;
    m_tempoLocked = false;
    return true;
}

void BPMEffect::render(plugins::EffectContext& ctx) {
    // Motion uses K1 beat PHASE (smooth), not raw BPM (jittery)
    uint8_t beat = 128;  // Default mid-value
    float energyEnvelope = 1.0f;  // Default: full brightness
    CRGB chromaticColor = CRGB(128, 128, 128);  // Default: neutral grey
    float decayRate = 0.85f;  // Default fallback decay

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        // =====================================================================
        // ENHANCEMENT 1: BPM-Adaptive Decay with Tempo Lock Hysteresis
        // =====================================================================
        // Hysteresis prevents jerky mode transitions at threshold boundary
        float tempoConf = ctx.audio.tempoConfidence();
        if (tempoConf > 0.6f) m_tempoLocked = true;       // Higher to lock
        else if (tempoConf < 0.4f) m_tempoLocked = false; // Lower to unlock

        if (m_tempoLocked) {
            // RELIABLE BEAT TRACKING: BPM-adaptive decay
            // Clamp to 60-180 BPM to prevent wild decay rates from jitter
            float bpm = ctx.audio.bpm();
            if (bpm < 60.0f) bpm = 60.0f;   // Floor: prevents very slow decay
            if (bpm > 180.0f) bpm = 180.0f; // Cap: prevents very fast decay

            // Decay should reach ~20% by next beat
            float beatPeriodFrames = (60.0f / bpm) * 120.0f;  // Frames per beat at 120 FPS
            decayRate = powf(0.2f, 1.0f / beatPeriodFrames);

            // Trigger pulse on beat, weighted by strength AND confidence
            if (ctx.audio.isOnBeat()) {
                float strength = ctx.audio.beatStrength();
                // Scale by confidence for gentler response on uncertain beats
                float weightedStrength = strength * (0.5f + 0.5f * tempoConf);
                m_beatPulse = fmaxf(m_beatPulse, weightedStrength);
            }
        } else {
            // UNRELIABLE BEAT: Use fixed decay fallback
            decayRate = 0.85f;
            if (ctx.audio.isOnBeat()) {
                m_beatPulse = 1.0f;
            }
        }

        // Apply decay
        m_beatPulse *= decayRate;
        if (m_beatPulse < 0.01f) m_beatPulse = 0.0f;

        // =====================================================================
        // ENHANCEMENT 2: Energy Envelope Modulation (Sensory Bridge pattern)
        // =====================================================================
        // Quadratic RMS for contrast (quiet passages dim, loud passages brighten)
        float rms = ctx.audio.rms();
        energyEnvelope = rms * rms;
        // Add minimum floor to prevent complete blackout
        if (energyEnvelope < 0.1f) energyEnvelope = 0.1f;

        // =====================================================================
        // ENHANCEMENT 3: Chromatic Color Mapping (Sensory Bridge pattern)
        // =====================================================================
        // Use heavy_chroma for stability (less flicker than raw chroma)
        chromaticColor = computeChromaticColor(ctx.audio.controlBus.heavy_chroma);

        // =====================================================================
        // Beat Phase → Sine Wave Intensity (EXISTING - KEEP)
        // =====================================================================
        // Use K1 beat PHASE for smooth motion (not raw BPM)
        // beatPhase() returns 0.0-1.0 smoothly, wraps at each beat
        // This is phase-locked to tempo but doesn't jitter with BPM uncertainty
        float beatPhase = ctx.audio.beatPhase();
        uint8_t phaseAngle = (uint8_t)(beatPhase * 255.0f);
        uint8_t rawBeat = sin8(phaseAngle);  // 0-255 sine
        beat = map8(rawBeat, 64, 255);  // Scale to 64-255 range
    } else
#endif
    {
        // Fallback: time-based sine at ~60 BPM when no audio
        beat = beatsin8(60, 64, 255);
        // Fallback: use palette color when no audio
        chromaticColor = ctx.palette.getColor(ctx.gHue, 128);
    }

    // =========================================================================
    // ENHANCEMENT 4: Silence Gating (Sensory Bridge pattern)
    // =========================================================================
    // Apply silentScale to fade to black during prolonged silence
    float silenceMultiplier = 1.0f;
#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        silenceMultiplier = ctx.audio.controlBus.silentScale;
    }
#endif

    // Beat pulse boost (scaled by energy envelope for cohesion)
    uint8_t pulseBoost = (uint8_t)(m_beatPulse * 60.0f * energyEnvelope);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);

        // Intensity decreases with distance from center
        uint8_t intensity = beat - (uint8_t)(distFromCenter * 3);
        intensity = max(intensity, (uint8_t)32);

        // Apply energy envelope modulation (Sensory Bridge pattern)
        intensity = (uint8_t)(intensity * energyEnvelope);
        intensity = max(intensity, (uint8_t)32);  // Maintain minimum brightness

        // Apply silence gating (fades to black during prolonged silence)
        intensity = (uint8_t)(intensity * silenceMultiplier);

        // Add beat pulse (stronger at center)
        float centerFade = 1.0f - (distFromCenter / HALF_LENGTH);
        intensity = qadd8(intensity, (uint8_t)(pulseBoost * centerFade));

        // Apply chromatic color with intensity scaling
        CRGB color = CRGB(
            scale8(chromaticColor.r, intensity),
            scale8(chromaticColor.g, intensity),
            scale8(chromaticColor.b, intensity)
        );

        ctx.leds[i] = color;
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = color;
        }
    }
}

void BPMEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& BPMEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "BPM",
        "Phase-locked to tempo with musical color and energy-responsive brightness",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
