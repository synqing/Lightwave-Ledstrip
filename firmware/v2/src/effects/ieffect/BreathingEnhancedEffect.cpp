/**
 * @file BreathingEnhancedEffect.cpp
 * @brief Breathing Enhanced - Enhanced version with 64-bin sub-bass, beatPhase sync, snare triggers
 */

#include "BreathingEnhancedEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

// Exponential foreshortening exponent (Bloom's secret)
static constexpr float FORESHORTEN_EXP = 0.7f;

// Helper: Compute Chromatic Color from 12-bin Chromagram
static CRGB computeChromaticColor(const float chroma[12], const plugins::EffectContext& ctx) {
    CRGB sum = CRGB::Black;
    float share = 1.0f / 6.0f;
    
    for (int i = 0; i < 12; i++) {
        float prog = i / 12.0f;
        float brightness = sqrtf(chroma[i]) * share * 2.0f;  // Sqrt boost for visibility
        if (brightness > 1.0f) brightness = 1.0f;
        
        uint8_t paletteIdx = (uint8_t)(prog * 255.0f + ctx.gHue);
        uint8_t brightU8 = (uint8_t)(brightness * 255.0f);
        brightU8 = (uint8_t)((brightU8 * ctx.brightness) / 255);
        CRGB noteColor = ctx.palette.getColor(paletteIdx, brightU8);

        constexpr uint8_t PRE_SCALE = 180;
        noteColor.r = scale8(noteColor.r, PRE_SCALE);
        noteColor.g = scale8(noteColor.g, PRE_SCALE);
        noteColor.b = scale8(noteColor.b, PRE_SCALE);

        sum.r = qadd8(sum.r, noteColor.r);
        sum.g = qadd8(sum.g, noteColor.g);
        sum.b = qadd8(sum.b, noteColor.b);
    }
    
    if (sum.r > 255) sum.r = 255;
    if (sum.g > 255) sum.g = 255;
    if (sum.b > 255) sum.b = 255;
    
    return sum;
}

BreathingEnhancedEffect::BreathingEnhancedEffect()
    : m_currentRadius(0.0f)
    , m_prevRadius(0.0f)
    , m_pulseIntensity(0.0f)
    , m_phase(0.0f)
    , m_fallbackPhase(0.0f)
    , m_energySmoothed(0.0f)
    , m_radiusTargetSum(0.0f)
    , m_histIdx(0)
{
    for (uint8_t i = 0; i < HISTORY_SIZE; ++i) {
        m_radiusTargetHist[i] = 0.0f;
    }
    for (uint8_t i = 0; i < 12; ++i) {
        m_chromaSmoothed[i] = 0.0f;
    }
}

bool BreathingEnhancedEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    m_currentRadius = 0.0f;
    m_prevRadius = 0.0f;
    m_pulseIntensity = 0.0f;
    m_phase = 0.0f;
    m_fallbackPhase = 0.0f;
    m_energySmoothed = 0.0f;
    
    m_rmsFollower.reset(0.0f);
    m_subBassFollower.reset(0.0f);
    for (int i = 0; i < 12; i++) {
        m_chromaFollowers[i].reset(0.0f);
        m_chromaTargets[i] = 0.0f;
    }
    m_lastHopSeq = 0;
    m_targetRms = 0.0f;
    m_targetSubBass = 0.0f;

    for (uint8_t i = 0; i < HISTORY_SIZE; ++i) {
        m_radiusTargetHist[i] = 0.0f;
    }
    m_radiusTargetSum = 0.0f;
    m_histIdx = 0;

    for (uint8_t i = 0; i < 12; ++i) {
        m_chromaSmoothed[i] = 0.0f;
    }
    
    m_tempoLocked = false;

    return true;
}

void BreathingEnhancedEffect::render(plugins::EffectContext& ctx) {
    // Enhanced Breathing: Audio → Color/Brightness, Time → Motion Speed

    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // ========================================================================
    // PHASE 1: TIME-BASED MOTION (User-controlled speed, NOT audio-reactive)
    // ========================================================================
    float dt = ctx.deltaTimeMs * 0.001f;
    float baseSpeed = ctx.speed / 200.0f;
    
    // Enhanced: Use beatPhase for sync when tempo confidence high (PLL-style correction)
    float tempoConf = 0.0f;
    float beatPhase = 0.0f;
#if FEATURE_AUDIO_SYNC
    // Domain constants (2*PI domain for BreathingEnhanced)
    const float PHASE_DOMAIN = 2.0f * 3.14159f;  // 2*PI
    const float HALF_DOMAIN = 3.14159f;          // PI

    // Tempo lock hysteresis (Schmitt trigger: prevents chatter near threshold)
    if (!ctx.audio.available) {
        m_tempoLocked = false;  // Clear lock when audio drops (prevents ghost lock)
    } else {
        tempoConf = ctx.audio.tempoConfidence();
        beatPhase = ctx.audio.beatPhase();
        
        // Update lock state with hysteresis (0.6 lock / 0.4 unlock)
        if (tempoConf > 0.6f) m_tempoLocked = true;
        else if (tempoConf < 0.4f) m_tempoLocked = false;
    }

    if (ctx.audio.available) {
        // Always advance phase (free-run oscillator)
        m_phase += baseSpeed * dt;
        
        // Apply phase correction when tempo-locked (PLL-style P-only correction)
        if (m_tempoLocked) {
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
    } else {
        // No audio: fallback slow animation
        m_fallbackPhase += baseSpeed * 0.3f * dt;
        m_phase = m_fallbackPhase;
    }
#else
    m_fallbackPhase += baseSpeed * 0.3f * dt;
    m_phase = m_fallbackPhase;
#endif
    
    // Generate breathing cycle from phase (sine wave)
    float timeBasedRadius = (sinf(m_phase) * 0.5f + 0.5f) * (float)HALF_LENGTH * 0.6f;

    // ========================================================================
    // PHASE 2: AUDIO-REACTIVE COLOR & BRIGHTNESS (Enhanced)
    // ========================================================================
    CRGB chromaticColor = CRGB::Black;
    float energyEnvelope = 0.0f;
    float brightness = 0.0f;
    float subBassEnergy = 0.0f;

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        float dt = ctx.getSafeDeltaSeconds();
        float moodNorm = ctx.getMoodNormalized();
        
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
            
            // Update chromagram targets (use heavy_chroma for stability)
            for (int i = 0; i < 12; i++) {
                m_chromaTargets[i] = ctx.audio.controlBus.heavy_chroma[i];
            }
            m_targetRms = ctx.audio.rms();
            
            // =================================================================
            // 64-bin Sub-Bass Detection (bins 0-5 = ~110-155 Hz)
            // =================================================================
            float subBassSum = 0.0f;
            for (uint8_t i = 0; i < 6; ++i) {
                subBassSum += ctx.audio.bin(i);
            }
            m_targetSubBass = subBassSum / 6.0f;
        }
        
        // Smooth chromagram with AsymmetricFollower
        for (int i = 0; i < 12; i++) {
            m_chromaSmoothed[i] = m_chromaFollowers[i].updateWithMood(
                m_chromaTargets[i], dt, moodNorm);
        }
        
        // Smooth energy envelope
        m_energySmoothed = m_rmsFollower.updateWithMood(m_targetRms, dt, moodNorm);
        subBassEnergy = m_subBassFollower.updateWithMood(m_targetSubBass, dt, moodNorm);
        
        // Compute Chromatic Color
        chromaticColor = computeChromaticColor(m_chromaSmoothed, ctx);
        
        // Compute Energy Envelope for Brightness Modulation
        energyEnvelope = m_energySmoothed;
        brightness = sqrtf(energyEnvelope) * 1.5f;  // Sqrt boost for visibility
        
        // Enhanced: Snare hit triggers sharp pulse
        if (ctx.audio.isSnareHit()) {
            m_pulseIntensity = 1.0f;
        }
        
        // Enhanced: Sub-bass boosts pulse intensity
        if (subBassEnergy > 0.15f) {  // Lower threshold for sensitivity
            m_pulseIntensity = fmaxf(m_pulseIntensity, subBassEnergy * 0.8f);
        }
        
        // Decay pulse intensity
        m_pulseIntensity *= 0.92f;
        if (m_pulseIntensity < 0.01f) m_pulseIntensity = 0.0f;
        
        // Boost brightness with pulse
        brightness = fminf(1.0f, brightness + m_pulseIntensity * 0.3f);
    } else
#endif
    {
        // NO AUDIO: Use default color and minimal brightness
        chromaticColor = ctx.palette.getColor(ctx.gHue, 128);
        brightness = 0.3f;
        m_pulseIntensity = 0.0f;
    }

    // ========================================================================
    // PHASE 3: COMBINE MOTION + AUDIO (Audio modulates size, not speed)
    // ========================================================================
    float targetRadius = timeBasedRadius * (0.4f + 0.6f * brightness);
    targetRadius = fmaxf(0.0f, fminf(targetRadius, (float)HALF_LENGTH));

    // ========================================================================
    // PHASE 4: ROLLING AVERAGE - Filter single-frame spikes
    // ========================================================================
    m_radiusTargetSum -= m_radiusTargetHist[m_histIdx];
    m_radiusTargetHist[m_histIdx] = targetRadius;
    m_radiusTargetSum += targetRadius;
    float avgTargetRadius = m_radiusTargetSum / (float)HISTORY_SIZE;
    m_histIdx = (m_histIdx + 1) % HISTORY_SIZE;

    // ========================================================================
    // PHASE 5: FRAME PERSISTENCE (Sensory Bridge Bloom Pattern)
    // ========================================================================
    float alpha = 0.99f;
    m_currentRadius = m_prevRadius * alpha + avgTargetRadius * (1.0f - alpha);
    m_prevRadius = m_currentRadius;

    // Wrap phase to prevent overflow
    if (m_phase > 2.0f * 3.14159f * 10.0f) {
        m_phase -= 2.0f * 3.14159f * 10.0f;
    }
    if (m_fallbackPhase > 2.0f * 3.14159f * 10.0f) {
        m_fallbackPhase -= 2.0f * 3.14159f * 10.0f;
    }

    // ========================================================================
    // PHASE 6: RENDERING with Chromatic Color & Energy-Modulated Brightness
    // ========================================================================
    if (m_currentRadius > 0.0001f) {  // Lower threshold for visibility
        for (int i = 0; i < STRIP_LENGTH; i++) {
            float dist = (float)centerPairDistance((uint16_t)i);

            if (dist <= m_currentRadius) {
                float intensity = 1.0f - (dist / m_currentRadius) * 0.5f;
                intensity = fmaxf(0.0f, intensity);
                
                float normalizedDist = dist / (float)HALF_LENGTH;
                float foreshortened = powf(normalizedDist, FORESHORTEN_EXP);
                float expMod = expf(-foreshortened * 1.5f);
                intensity *= (0.7f + 0.3f * expMod);

                float finalBrightness = intensity * brightness;
                uint8_t ledBrightness = (uint8_t)(255.0f * finalBrightness);

                CRGB color = chromaticColor;
                color.r = scale8(color.r, ledBrightness);
                color.g = scale8(color.g, ledBrightness);
                color.b = scale8(color.b, ledBrightness);

                ctx.leds[i] = color;
                if (i + STRIP_LENGTH < ctx.ledCount) {
                    ctx.leds[i + STRIP_LENGTH] = color;
                }
            }
        }
    }
    
    // ========================================================================
    // PHASE 7: SPATIAL FALLOFF (Quadratic Edge Fade)
    // ========================================================================
    for (uint8_t i = 0; i < 32; i++) {
        float prog = i / 31.0f;
        float falloff = prog * 0.7f;  // Linear fade (was quadratic)
        
        uint16_t edgeIdx = STRIP_LENGTH - 1 - i;
        if (edgeIdx < ctx.ledCount) {
            ctx.leds[edgeIdx].r = scale8(ctx.leds[edgeIdx].r, (uint8_t)(255.0f * (1.0f - falloff)));
            ctx.leds[edgeIdx].g = scale8(ctx.leds[edgeIdx].g, (uint8_t)(255.0f * (1.0f - falloff)));
            ctx.leds[edgeIdx].b = scale8(ctx.leds[edgeIdx].b, (uint8_t)(255.0f * (1.0f - falloff)));
        }
    }
}

void BreathingEnhancedEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& BreathingEnhancedEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Breathing Enhanced",
        "Enhanced: 64-bin sub-bass, beatPhase sync, snare triggers",
        plugins::EffectCategory::AMBIENT,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

