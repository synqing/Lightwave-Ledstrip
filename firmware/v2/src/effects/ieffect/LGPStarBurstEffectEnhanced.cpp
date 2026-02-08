/**
 * @file LGPStarBurstEffect.cpp
 * @brief LGP Star Burst effect - explosive radial lines from center
 *
 * REPAIRED: Simplified to match Wave Collision's proven pattern.
 * Previous version was over-engineered with 9+ audio inputs causing chaos.
 *
 * Pattern: CENTER_ORIGIN radial waves with snare-driven bursts
 *
 * Audio Integration (Wave Collision pattern):
 * - Heavy Bass → Speed modulation (slew-limited)
 * - Snare Hit → Burst flash (center-focused)
 * - Chroma → Color (dominant bin for hue offset)
 */

#include "LGPStarBurstEffectEnhanced.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPStarBurstEnhancedEffect::LGPStarBurstEnhancedEffect()
    : m_phase(0.0f)
{
}

bool LGPStarBurstEnhancedEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    m_burst = 0.0f;
    m_lastHopSeq = 0;
    m_dominantBin = 0;
    m_dominantBinSmooth = 0.0f;
    m_subBassEnergy = 0.0f;
    m_targetSubBass = 0.0f;
    m_hihatSparkle = 0.0f;

    // Initialize chromagram smoothing
    for (uint8_t i = 0; i < 12; i++) {
        m_chromaFollowers[i].reset(0.0f);
        m_chromaSmoothed[i] = 0.0f;
        m_chromaTargets[i] = 0.0f;
    }
    
    // Initialize spring physics for natural speed momentum
    m_phaseSpeedSpring.init(50.0f, 1.0f);  // stiffness=50, mass=1 (critically damped)
    m_phaseSpeedSpring.reset(1.0f);        // Start at base speed
    m_subBassFollower.reset(0.0f);
    m_tempoLocked = false;

    return true;
}

void LGPStarBurstEnhancedEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN - Star-like patterns radiating from centre
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;
    const bool hasAudio = ctx.audio.available;

    // =========================================================================
    // Audio Analysis (per-hop, like Wave Collision)
    // =========================================================================
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
            
            // Update chromagram targets
            for (uint8_t i = 0; i < 12; i++) {
                m_chromaTargets[i] = ctx.audio.controlBus.heavy_chroma[i];
            }

            // Simple chroma analysis for color (dominant bin only) - use smoothed values
            float maxBinVal = 0.0f;
            for (uint8_t i = 0; i < 12; ++i) {
                float bin = m_chromaSmoothed[i];
                if (bin > maxBinVal) {
                    maxBinVal = bin;
                    m_dominantBin = i;
                }
            }

            // Enhanced: Snare = burst with sub-bass boost
            if (ctx.audio.isSnareHit()) {
                m_burst = 1.0f;
            }
            
            // Enhanced: 64-bin Sub-Bass Detection (bins 0-5 = ~110-155 Hz)
            float subBassSum = 0.0f;
            for (uint8_t i = 0; i < 6; ++i) {
                subBassSum += ctx.audio.bin(i);
            }
            m_targetSubBass = subBassSum / 6.0f;
        }
    }
#endif

    // =========================================================================
    // Per-frame Updates (smooth animation)
    // =========================================================================
    float dt = ctx.getSafeDeltaSeconds();
    float moodNorm = ctx.getMoodNormalized();
    
    // Smooth chromagram with AsymmetricFollower (every frame)
    if (hasAudio) {
        for (uint8_t i = 0; i < 12; i++) {
            m_chromaSmoothed[i] = m_chromaFollowers[i].updateWithMood(
                m_chromaTargets[i], dt, moodNorm);
        }
        // Enhanced: Smooth sub-bass energy
        m_subBassEnergy = m_subBassFollower.updateWithMood(m_targetSubBass, dt, moodNorm);
        
        // Enhanced: Hi-hat hit triggers sparkle burst
        if (ctx.audio.isHihatHit()) {
            m_hihatSparkle = 1.0f;
        }
        m_hihatSparkle *= 0.85f;  // Faster decay for sparkle
        if (m_hihatSparkle < 0.01f) m_hihatSparkle = 0.0f;
    }

    // Smooth dominant bin (for color stability) - true exponential, tau=250ms
    float alphaBin = 1.0f - expf(-dt / 0.25f);
    m_dominantBinSmooth += (m_dominantBin - m_dominantBinSmooth) * alphaBin;
    if (m_dominantBinSmooth < 0.0f) m_dominantBinSmooth = 0.0f;
    if (m_dominantBinSmooth > 11.0f) m_dominantBinSmooth = 11.0f;

    // Enhanced: Use 64-bin sub-bass for speed (more responsive)
    float heavyEnergy = 0.0f;
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        // Blend sub-bass with heavy_bands - use sqrt for gentler curve
        float subBass = sqrtf(m_subBassEnergy) * 1.5f;
        float heavyBass = sqrtf(ctx.audio.heavyBass()) * 1.5f;
        heavyEnergy = subBass * 0.7f + heavyBass * 0.3f;
        heavyEnergy = fmaxf(0.2f, heavyEnergy);  // Minimum floor for visibility
    }
#endif

    // Spring physics for speed modulation (natural momentum, no jitter)
    float targetSpeed = 0.7f + 0.6f * heavyEnergy;
    float smoothedSpeed = m_phaseSpeedSpring.update(targetSpeed, dt);
    if (smoothedSpeed > 2.0f) smoothedSpeed = 2.0f;
    if (smoothedSpeed < 0.3f) smoothedSpeed = 0.3f;  // Prevent stalling

    // Enhanced: Use beatPhase for synchronization when tempo confidence high (PLL-style correction)
    // Domain constants (compute once, use consistently)
    const float PHASE_DOMAIN = 628.3f;      // 100 * 2 * PI
    const float HALF_DOMAIN = 314.15f;     // PHASE_DOMAIN / 2

    // Tempo lock hysteresis (Schmitt trigger: prevents chatter near threshold)
    if (!hasAudio) {
        m_tempoLocked = false;  // Clear lock when audio drops (prevents ghost lock)
    } else {
        float tempoConf = ctx.audio.tempoConfidence();
        
        // Update lock state with hysteresis (0.6 lock / 0.4 unlock)
        if (tempoConf > 0.6f) m_tempoLocked = true;
        else if (tempoConf < 0.4f) m_tempoLocked = false;
    }
    
    // Always advance phase (free-run oscillator)
    m_phase += speedNorm * 240.0f * smoothedSpeed * dt;
    
    // Apply phase correction when tempo-locked (PLL-style P-only correction)
    if (hasAudio && m_tempoLocked) {
        float beatPhase = ctx.audio.beatPhase();
        float targetPhase = beatPhase * PHASE_DOMAIN;
        
        // Compute wrapped error (shortest path to target)
        float phaseError = targetPhase - m_phase;
        if (phaseError > HALF_DOMAIN) phaseError -= PHASE_DOMAIN;
        if (phaseError < -HALF_DOMAIN) phaseError += PHASE_DOMAIN;
        
        // Proportional correction (tau ~100ms gives smooth lock)
        // Compute ONCE per frame, not per pixel
        const float tau = 0.1f;
        const float correctionAlpha = 1.0f - expf(-dt / tau);
        m_phase += phaseError * correctionAlpha;
    }
    
    // CRITICAL: Wrap phase AFTER correction (handles negative and overflow)
    while (m_phase >= PHASE_DOMAIN) m_phase -= PHASE_DOMAIN;
    while (m_phase < 0.0f) m_phase += PHASE_DOMAIN;

    // Enhanced: Burst decay with sub-bass boost
    m_burst *= 0.88f;
    if (hasAudio && m_subBassEnergy > 0.3f) {
        m_burst = fmaxf(m_burst, m_subBassEnergy * 0.5f);  // Boost burst with sub-bass
    }

    // =========================================================================
    // Rendering
    // =========================================================================
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // Anti-aliased burst core at true center (79.5) using SubpixelRenderer
    // Lower threshold from 0.05 to 0.02 for more visibility
    if (m_burst > 0.02f) {
        uint8_t baseHue = (uint8_t)(ctx.gHue + m_dominantBinSmooth * (255.0f / 12.0f));
        CRGB burstColor = ctx.palette.getColor(baseHue, 255);
        // Enhanced: Boost brightness with sub-bass and hi-hat sparkle
        // Use sqrt for gentler curve on burst intensity
        float burstIntensity = sqrtf(m_burst) * 1.5f + m_hihatSparkle * 0.4f;
        burstIntensity = fmaxf(0.2f, burstIntensity);  // Minimum floor for visibility
        if (burstIntensity > 1.0f) burstIntensity = 1.0f;
        uint8_t burstBright = (uint8_t)(burstIntensity * 200.0f * intensityNorm);

        // Render bright core at fractional center (between LED 79 and 80)
        enhancement::SubpixelRenderer::renderPoint(
            ctx.leds, STRIP_LENGTH, 79.5f, burstColor, burstBright
        );

        // Also render on strip 2
        if (STRIP_LENGTH * 2 <= ctx.ledCount) {
            enhancement::SubpixelRenderer::renderPoint(
                ctx.leds + STRIP_LENGTH, STRIP_LENGTH, 79.5f,
                ctx.palette.getColor(baseHue + 90, 255), burstBright
            );
        }
    }

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);

        // FIXED frequency - no kick modulation (like Wave Collision)
        const float freqBase = 0.25f;
        float star = sinf(distFromCenter * freqBase - m_phase);

        // Center-focused burst flash (like Wave Collision's collision flash)
        float burstFlash = m_burst * expf(-distFromCenter * 0.12f);

        // Simple audio gain (like Wave Collision) - use sqrt for gentler curve
        float audioGain = 0.5f + 0.5f * sqrtf(heavyEnergy) * 1.5f;
        float pattern = star * audioGain + burstFlash * 0.8f;

        // tanhf for uniform brightness (PROVEN PATTERN)
        pattern = tanhf(pattern * 2.0f) * 0.5f + 0.5f;

        // Add base brightness floor (0.2) so effect is visible without harmonic content
        pattern = fmaxf(0.2f, pattern);
        uint8_t brightness = (uint8_t)(pattern * 255.0f * intensityNorm);
        uint8_t paletteIndex = (uint8_t)(distFromCenter * 2.0f + pattern * 50.0f);
        uint8_t baseHue = (uint8_t)(ctx.gHue + m_dominantBinSmooth * (255.0f / 12.0f));

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(baseHue + paletteIndex), brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(baseHue + paletteIndex + 90), brightness);
        }
    }
}

void LGPStarBurstEnhancedEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPStarBurstEnhancedEffect::getMetadata() const {
    static plugins::EffectMetadata meta(
        "LGP Star Burst Enhanced",
        "Enhanced: 64-bin sub-bass, enhanced snare/hi-hat triggers, beatPhase sync",
        plugins::EffectCategory::GEOMETRIC,
        1
    );
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
