/**
 * @file BPMEnhancedEffect.cpp
 * @brief BPM Enhanced effect - Enhanced version with 64-bin spectrum, heavy_chroma, beatPhase sync
 */

#include "BPMEnhancedEffect.h"
#include "ChromaUtils.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

BPMEnhancedEffect::BPMEnhancedEffect()
    : m_phase(0.0f)
    , m_nextRing(0)
    , m_tempoLocked(false)
    , m_fallbackPhase(0.0f)
{
}

bool BPMEnhancedEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Initialize phase
    m_phase = 0.0f;
    m_fallbackPhase = 0.0f;

    // Initialize Spring with stiffness=50, critically damped
    m_speedSpring.init(50.0f, 1.0f);
    m_speedSpring.reset(1.0f);
    
    // Initialize smoothing followers
    m_heavyEnergyFollower.reset(0.0f);
    m_beatStrengthFollower.reset(0.0f);
    m_tempoConfFollower.reset(0.0f);
    m_subBassFollower.reset(0.0f);
    m_lastHopSeq = 0;
    m_targetHeavyEnergy = 0.0f;
    m_targetBeatStrength = 0.0f;
    m_targetTempoConf = 0.0f;
    m_targetSubBass = 0.0f;

    // Initialize chromagram smoothing
    for (uint8_t i = 0; i < 12; i++) {
        m_chromaFollowers[i].reset(0.0f);
        m_chromaSmoothed[i] = 0.0f;
        m_chromaTargets[i] = 0.0f;
    }
    m_chromaAngle = 0.0f;

    // Clear ring buffer
    for (int r = 0; r < MAX_RINGS; r++) {
        m_ringRadius[r] = 0.0f;
        m_ringIntensity[r] = 0.0f;
    }
    m_nextRing = 0;

    m_tempoLocked = false;

    return true;
}

void BPMEnhancedEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // SAFE DELTA TIME (clamped for physics stability)
    // =========================================================================
    float rawDt = ctx.getSafeRawDeltaSeconds();
    float dt = ctx.getSafeDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;
    float moodNorm = ctx.getMoodNormalized();

    // Default values for no-audio mode
    float speedMult = 1.0f;
    float expansionRate = 80.0f;
    float subBassEnergy = 0.0f;
    uint8_t chromaHueOffset = 0;

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        // =====================================================================
        // Hop-based updates: update targets only on new hops
        // =====================================================================
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
            m_targetHeavyEnergy = (ctx.audio.controlBus.heavy_bands[1] +
                                   ctx.audio.controlBus.heavy_bands[2]) / 2.0f;
            m_targetBeatStrength = ctx.audio.beatStrength();
            m_targetTempoConf = ctx.audio.tempoConfidence();
            
            // =================================================================
            // 64-bin Sub-Bass Detection (bins 0-5 = ~110-155 Hz)
            // =================================================================
            float subBassSum = 0.0f;
            for (uint8_t i = 0; i < 6; ++i) {
                subBassSum += ctx.audio.bin(i);
            }
            m_targetSubBass = subBassSum / 6.0f;
            
            // Update chromagram targets (use heavy_chroma for stability)
            for (uint8_t i = 0; i < 12; i++) {
                m_chromaTargets[i] = ctx.audio.controlBus.heavy_chroma[i];
            }
        }
        
        // Smooth toward targets every frame with MOOD-adjusted smoothing
        float heavyEnergy = m_heavyEnergyFollower.updateWithMood(m_targetHeavyEnergy, rawDt, moodNorm);
        float beatStrength = m_beatStrengthFollower.updateWithMood(m_targetBeatStrength, rawDt, moodNorm);
        float tempoConf = m_tempoConfFollower.updateWithMood(m_targetTempoConf, rawDt, moodNorm);
        subBassEnergy = m_subBassFollower.updateWithMood(m_targetSubBass, rawDt, moodNorm);
        
        // Smooth chromagram with AsymmetricFollower
        for (uint8_t i = 0; i < 12; i++) {
            m_chromaSmoothed[i] = m_chromaFollowers[i].updateWithMood(
                m_chromaTargets[i], rawDt, moodNorm);
        }
        
        // Circular weighted mean + circular EMA for smooth, continuous hue.
        // Eliminates argmax discontinuities AND temporal chroma shifts.
        chromaHueOffset = effects::chroma::circularChromaHueSmoothed(
            m_chromaSmoothed, m_chromaAngle, rawDt, 0.20f);
        
        // =====================================================================
        // v8 SPEED MODULATION: heavy_bands → Spring (NO stacked smoothing!)
        // =====================================================================
        float targetSpeed = 0.6f + 0.8f * heavyEnergy;  // 0.6-1.4x range
        speedMult = m_speedSpring.update(targetSpeed, dt);
        if (speedMult > 1.6f) speedMult = 1.6f;
        if (speedMult < 0.3f) speedMult = 0.3f;

        // =====================================================================
        // TEMPO LOCK HYSTERESIS
        // =====================================================================
        if (tempoConf > 0.6f) m_tempoLocked = true;
        else if (tempoConf < 0.4f) m_tempoLocked = false;

        // Expansion rate scales with tempo confidence
        // Use sqrt for gentler curve, add minimum floor for visibility
        float confFactor = sqrtf(tempoConf) * 1.5f;
        confFactor = fmaxf(0.3f, confFactor);  // Minimum 0.3 floor when confidence low
        expansionRate = 80.0f * (0.5f + confFactor);

        // =====================================================================
        // BEAT RING SPAWNING (enhanced with beatPhase sync and snare triggers)
        // =====================================================================
        bool shouldSpawnRing = false;
        float ringIntensity = 0.0f;

        // Primary: Beat detection
        if (ctx.audio.isOnBeat()) {
            // Scale by confidence - use sqrt for gentler curve at low confidence
            float confWeight = sqrtf(tempoConf) * 1.5f;
            confWeight = fmaxf(0.3f, confWeight);  // Minimum floor for visibility
            float weightedStrength = beatStrength * (0.5f + 0.5f * confWeight);
            // Boost intensity with sub-bass energy
            weightedStrength = fmaxf(weightedStrength, subBassEnergy * 0.5f);
            ringIntensity = weightedStrength;
            shouldSpawnRing = true;
        }
        
        // Secondary: Snare hit trigger (additional ring spawns)
        if (ctx.audio.isSnareHit()) {
            float snareIntensity = 0.7f + subBassEnergy * 0.3f;
            if (snareIntensity > ringIntensity) {
                ringIntensity = snareIntensity;
                shouldSpawnRing = true;
            }
        }
        
        if (shouldSpawnRing && ringIntensity > 0.1f) {
            // Spawn new ring
            m_ringRadius[m_nextRing] = 0.0f;
            m_ringIntensity[m_nextRing] = ringIntensity;
            m_nextRing = (m_nextRing + 1) % MAX_RINGS;
        }
    } else {
        // NO AUDIO: Fallback slow time-based animation
        m_fallbackPhase += speedNorm * 0.5f * dt;  // Slow animation
        if (m_fallbackPhase > 2.0f * 3.14159f * 10.0f) {
            m_fallbackPhase -= 2.0f * 3.14159f * 10.0f;
        }
    }
#else
    // NO AUDIO FEATURE: Fallback slow time-based animation
    m_fallbackPhase += speedNorm * 0.5f * dt;
    if (m_fallbackPhase > 2.0f * 3.14159f * 10.0f) {
        m_fallbackPhase -= 2.0f * 3.14159f * 10.0f;
    }
#endif

    // =========================================================================
    // PHASE ACCUMULATION (PLL-style correction for smooth lock)
    // =========================================================================
#if FEATURE_AUDIO_SYNC
    // Domain constants (compute once, use consistently)
    const float PHASE_DOMAIN = 628.3f;      // 100 * 2 * PI
    const float HALF_DOMAIN = 314.15f;      // PHASE_DOMAIN / 2

    // Always advance phase (free-run oscillator)
    m_phase += speedNorm * 240.0f * speedMult * dt;

    // Apply phase correction when tempo-locked (PLL-style P-only correction)
    if (ctx.audio.available && m_tempoLocked) {
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
#else
    // Fallback: use fallback phase
    m_phase = m_fallbackPhase;
#endif

    // =========================================================================
    // UPDATE RING RADII (expand outward, fade as they go)
    // =========================================================================
    for (int r = 0; r < MAX_RINGS; r++) {
        if (m_ringIntensity[r] > 0.01f) {
            m_ringRadius[r] += expansionRate * dt;
            m_ringIntensity[r] *= powf(0.97f, dt * 60.0f);  // Gradual fade (dt-corrected)

            // Kill ring when it reaches edge
            if (m_ringRadius[r] > HALF_LENGTH) {
                m_ringIntensity[r] = 0.0f;
            }
        }
    }

    // =========================================================================
    // Fade for background wave trails
    // =========================================================================
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // =========================================================================
    // DUAL-LAYER RENDER LOOP
    // =========================================================================
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float dist = (float)centerPairDistance(i);

        // =====================================================================
        // LAYER 1: Background traveling sine wave from center
        // sin(dist * freq - phase) → OUTWARD motion when phase increases
        // =====================================================================
        float waveFreq = 0.12f;  // ~52 LED wavelength
        float wave = sinf(dist * waveFreq - m_phase);
        // Map [-1,1] to [0.2, 0.8] for subtle background
        // Add minimum brightness floor (0.3) when tempo confidence low
        float baseBrightness = (wave * 0.3f + 0.5f);
        baseBrightness = fmaxf(0.3f, baseBrightness);  // Floor for visibility
        uint8_t baseIntensity = (uint8_t)(baseBrightness * ctx.brightness);

        // =====================================================================
        // LAYER 2: Beat rings overlay
        // Each ring is a Gaussian-ish bump that expands from center
        // =====================================================================
        uint8_t ringBoost = 0;
        for (int r = 0; r < MAX_RINGS; r++) {
            if (m_ringIntensity[r] > 0.01f) {
                float delta = fabsf(dist - m_ringRadius[r]);
                if (delta < 6.0f) {
                    // Soft falloff within 6 LEDs of ring center
                    float ringBright = (1.0f - delta / 6.0f) * m_ringIntensity[r];
                    ringBoost = qadd8(ringBoost, (uint8_t)(ringBright * 180.0f));
                }
            }
        }

        // Combine layers
        uint8_t intensity = qadd8(baseIntensity, ringBoost);

        // =====================================================================
        // COLOR: Use heavy_chroma for stable color (enhanced)
        // =====================================================================
#if FEATURE_AUDIO_SYNC
        if (ctx.audio.available) {
            // Use dominant chroma bin for hue offset
            uint8_t hue = ctx.gHue + chromaHueOffset + (uint8_t)(dist / 3);
            CRGB color = ctx.palette.getColor(hue, intensity);
            ctx.leds[i] = color;
            
            // Strip 2: Complementary color (+128 hue offset)
            if (i + STRIP_LENGTH < ctx.ledCount) {
                ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(
                    (uint8_t)(hue + 128), intensity);
            }
        } else {
            // Fallback: simple palette color
            uint8_t hue = ctx.gHue + (uint8_t)(dist / 3);
            CRGB color = ctx.palette.getColor(hue, intensity);
            ctx.leds[i] = color;
            if (i + STRIP_LENGTH < ctx.ledCount) {
                ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(
                    (uint8_t)(hue + 128), intensity);
            }
        }
#else
        // Fallback: simple palette color
        uint8_t hue = ctx.gHue + (uint8_t)(dist / 3);
        CRGB color = ctx.palette.getColor(hue, intensity);
        ctx.leds[i] = color;
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(
                (uint8_t)(hue + 128), intensity);
        }
#endif
    }
}

void BPMEnhancedEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& BPMEnhancedEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "BPM Enhanced",
        "Enhanced: 64-bin sub-bass, heavy_chroma, beatPhase sync, snare triggers",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

