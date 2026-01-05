/**
 * @file SaliencyAwareEffect.cpp
 * @brief Saliency-aware effect implementation
 *
 * Adapts visual behavior based on dominant saliency type from Musical Intelligence System.
 */

#include "SaliencyAwareEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool SaliencyAwareEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase1 = 0.0f;
    m_phase2 = 0.0f;
    m_phase3 = 0.0f;
    m_rhythmicPulse = 0.0f;
    m_timbralTexture = 0.0f;
    m_dynamicEnergy = 0.0f;
    m_lastHopSeq = 0;
    m_targetRms = 0.0f;
    m_smoothRms = 0.0f;
    m_lastTimbralSaliency = 0.0f;
    m_targetHarmonic = 0.0f;
    m_targetRhythmic = 0.0f;
    m_targetTimbral = 0.0f;
    m_targetDynamic = 0.0f;
    m_targetOverall = 0.0f;
    m_smoothHarmonic = 0.0f;
    m_smoothRhythmic = 0.0f;
    m_smoothTimbral = 0.0f;
    m_smoothDynamic = 0.0f;
    m_smoothOverall = 0.0f;
    m_currentMode = 0.0f;
    m_modeTransition = 0.0f;
    m_currentModeStrength = 0.0f;
    m_smoothFreq1 = 0.16f;
    m_smoothFreq2 = 0.28f;
    m_smoothFreq3 = 0.12f;
    m_saliencyBoostSmooth = 0.0f;
    m_boostHistorySum = 0.0f;
    m_boostHistIdx = 0;
    for (uint8_t i = 0; i < BOOST_HISTORY_SIZE; ++i) {
        m_boostHistory[i] = 0.0f;
    }
    // Initialize history buffer
    for (uint8_t i = 0; i < SALIENCY_HISTORY_SIZE; ++i) {
        for (uint8_t j = 0; j < 6; ++j) {
            m_saliencyHistory[i][j] = 0.0f;
        }
    }
    m_saliencyHistIdx = 0;
    
    // Reset AsymmetricFollower instances for mood integration
    m_harmonicFollower.reset(0.0f);
    m_rhythmicFollower.reset(0.0f);
    m_timbralFollower.reset(0.0f);
    m_dynamicFollower.reset(0.0f);
    m_overallFollower.reset(0.0f);
    m_rmsFollower.reset(0.0f);
    
    return true;
}

void SaliencyAwareEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Multi-layer interference pattern
    // Visual Foundation: Interference moiré patterns (like LGPInterferenceScanner)
    // Audio Enhancement: Saliency metrics modulate brightness/color and switch modes
    
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

#if !FEATURE_AUDIO_SYNC
    // Fallback: simple interference pattern
    float speedNorm = ctx.speed / 50.0f;
    float dt = ctx.getSafeDeltaSeconds();
    m_phase1 += speedNorm * 0.02f * dt;
    m_phase2 += speedNorm * 0.03f * dt;
    m_phase3 += speedNorm * 0.05f * dt;
    
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        float distNorm = (float)dist / (float)HALF_LENGTH;
        float wave1 = sinf(distNorm * 0.16f * 2.0f * 3.14159f + m_phase1);
        float wave2 = sinf(distNorm * 0.28f * 2.0f * 3.14159f + m_phase2);
        float wave3 = sinf(distNorm * 0.12f * 2.0f * 3.14159f + m_phase3);
        float interference = (wave1 + wave2 * 0.6f + wave3 * 0.4f) / 2.0f;
        uint8_t bright = (uint8_t)(128 + 127 * interference);
        CRGB color = ctx.palette.getColor(ctx.gHue + (uint8_t)(distNorm * 50.0f), bright);
        SET_CENTER_PAIR(ctx, dist, color);
    }
    return;
#else
    if (!ctx.audio.available) {
        // Fallback when audio unavailable
        float speedNorm = ctx.speed / 50.0f;
        float dt = ctx.getSafeDeltaSeconds();
        m_phase1 += speedNorm * 0.02f * dt;
        m_phase2 += speedNorm * 0.03f * dt;
        m_phase3 += speedNorm * 0.05f * dt;
        
        for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
            float distNorm = (float)dist / (float)HALF_LENGTH;
            float wave1 = sinf(distNorm * 0.16f * 2.0f * 3.14159f + m_phase1);
            float wave2 = sinf(distNorm * 0.28f * 2.0f * 3.14159f + m_phase2);
            float wave3 = sinf(distNorm * 0.12f * 2.0f * 3.14159f + m_phase3);
            float interference = (wave1 + wave2 * 0.6f + wave3 * 0.4f) / 2.0f;
            uint8_t bright = (uint8_t)(128 + 127 * interference);
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
    
    // Different speed rates per mode (all time-based)
    float speed1 = speedNorm * 0.02f;  // Slow for harmonic
    float speed2 = speedNorm * 0.03f;  // Medium
    float speed3 = speedNorm * 0.05f;  // Fast for rhythmic
    
    m_phase1 += speed1 * dt;  // Time-based only
    m_phase2 += speed2 * dt;
    m_phase3 += speed3 * dt;
    
    // Proper modulo wrapping to prevent discontinuities
    const float twoPi = 2.0f * 3.14159f;
    m_phase1 = fmodf(m_phase1, twoPi);
    if (m_phase1 < 0.0f) m_phase1 += twoPi;
    m_phase2 = fmodf(m_phase2, twoPi);
    if (m_phase2 < 0.0f) m_phase2 += twoPi;
    m_phase3 = fmodf(m_phase3, twoPi);
    if (m_phase3 < 0.0f) m_phase3 += twoPi;

    // =========================================================================
    // Audio Enhancement: Saliency metrics with Sensory Bridge smoothing pattern
    // =========================================================================
    // Check for new audio hop (prevents per-frame noise)
    bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
    if (newHop) {
        m_lastHopSeq = ctx.audio.controlBus.hop_seq;
        
        // Update targets only on new hops (fresh audio data)
        m_targetHarmonic = ctx.audio.harmonicSaliency();
        m_targetRhythmic = ctx.audio.rhythmicSaliency();
        m_targetTimbral = ctx.audio.timbralSaliency();
        m_targetDynamic = ctx.audio.dynamicSaliency();
        m_targetOverall = ctx.audio.overallSaliency();
        m_targetRms = ctx.audio.rms();
        
        // =========================================================================
        // Sensory Bridge Pattern: History Buffer BEFORE Smoothing (filters spikes)
        // =========================================================================
        // Update history buffer ONLY on new hops (when data actually changes)
        // This filters single-frame audio spikes before smoothing
        m_saliencyHistory[m_saliencyHistIdx][0] = m_targetHarmonic;
        m_saliencyHistory[m_saliencyHistIdx][1] = m_targetRhythmic;
        m_saliencyHistory[m_saliencyHistIdx][2] = m_targetTimbral;
        m_saliencyHistory[m_saliencyHistIdx][3] = m_targetDynamic;
        m_saliencyHistory[m_saliencyHistIdx][4] = m_targetOverall;
        m_saliencyHistory[m_saliencyHistIdx][5] = m_targetRms;
        m_saliencyHistIdx = (m_saliencyHistIdx + 1) % SALIENCY_HISTORY_SIZE;
    }
    
    // Average history (4-frame rolling average - Sensory Bridge pattern)
    float avgHarmonic = (m_saliencyHistory[0][0] + m_saliencyHistory[1][0] + 
                         m_saliencyHistory[2][0] + m_saliencyHistory[3][0]) / 4.0f;
    float avgRhythmic = (m_saliencyHistory[0][1] + m_saliencyHistory[1][1] + 
                         m_saliencyHistory[2][1] + m_saliencyHistory[3][1]) / 4.0f;
    float avgTimbral = (m_saliencyHistory[0][2] + m_saliencyHistory[1][2] + 
                        m_saliencyHistory[2][2] + m_saliencyHistory[3][2]) / 4.0f;
    float avgDynamic = (m_saliencyHistory[0][3] + m_saliencyHistory[1][3] + 
                        m_saliencyHistory[2][3] + m_saliencyHistory[3][3]) / 4.0f;
    float avgOverall = (m_saliencyHistory[0][4] + m_saliencyHistory[1][4] + 
                        m_saliencyHistory[2][4] + m_saliencyHistory[3][4]) / 4.0f;
    float avgRms = (m_saliencyHistory[0][5] + m_saliencyHistory[1][5] + 
                    m_saliencyHistory[2][5] + m_saliencyHistory[3][5]) / 4.0f;
    
    // =========================================================================
    // Sensory Bridge Pattern: Simple Symmetric 0.75 Linear Interpolation
    // =========================================================================
    // Smooth toward averaged targets using simple 0.75 coefficient (like get_smooth_spectrogram)
    const float smoothingCoeff = 0.75f;  // Sensory Bridge spectrogram smoothing coefficient
    
    float distance = avgHarmonic - m_smoothHarmonic;
    m_smoothHarmonic += distance * smoothingCoeff;
    
    distance = avgRhythmic - m_smoothRhythmic;
    m_smoothRhythmic += distance * smoothingCoeff;
    
    distance = avgTimbral - m_smoothTimbral;
    m_smoothTimbral += distance * smoothingCoeff;
    
    distance = avgDynamic - m_smoothDynamic;
    m_smoothDynamic += distance * smoothingCoeff;
    
    distance = avgOverall - m_smoothOverall;
    m_smoothOverall += distance * smoothingCoeff;
    
    distance = avgRms - m_smoothRms;
    m_smoothRms += distance * smoothingCoeff;
    
    // Clamp smoothed values
    m_smoothHarmonic = fmaxf(0.0f, fminf(1.0f, m_smoothHarmonic));
    m_smoothRhythmic = fmaxf(0.0f, fminf(1.0f, m_smoothRhythmic));
    m_smoothTimbral = fmaxf(0.0f, fminf(1.0f, m_smoothTimbral));
    m_smoothDynamic = fmaxf(0.0f, fminf(1.0f, m_smoothDynamic));
    m_smoothOverall = fmaxf(0.0f, fminf(1.0f, m_smoothOverall));
    m_smoothRms = fmaxf(0.0f, m_smoothRms);
    
    // Determine target mode with hysteresis (prevents rapid flipping)
    float targetMode = m_currentMode;
    float targetModeStrength = 0.0f;
    
    // Calculate strength of each mode
    float harmonicStrength = m_smoothHarmonic;
    float rhythmicStrength = m_smoothRhythmic;
    float timbralStrength = m_smoothTimbral;
    float dynamicStrength = m_smoothDynamic;
    
    // Find strongest mode
    float maxStrength = fmaxf(fmaxf(harmonicStrength, rhythmicStrength), 
                               fmaxf(timbralStrength, dynamicStrength));
    
    // Only switch if new mode is significantly stronger (30% hysteresis threshold)
    if (harmonicStrength == maxStrength && harmonicStrength > m_currentModeStrength * 1.3f) {
        targetMode = 0.0f;
        targetModeStrength = harmonicStrength;
    } else if (rhythmicStrength == maxStrength && rhythmicStrength > m_currentModeStrength * 1.3f) {
        targetMode = 1.0f;
        targetModeStrength = rhythmicStrength;
    } else if (timbralStrength == maxStrength && timbralStrength > m_currentModeStrength * 1.3f) {
        targetMode = 2.0f;
        targetModeStrength = timbralStrength;
    } else if (dynamicStrength == maxStrength && dynamicStrength > m_currentModeStrength * 1.3f) {
        targetMode = 3.0f;
        targetModeStrength = dynamicStrength;
    } else {
        // Keep current mode, update strength
        if (m_currentMode < 0.5f) {
            targetModeStrength = harmonicStrength;
        } else if (m_currentMode < 1.5f) {
            targetModeStrength = rhythmicStrength;
        } else if (m_currentMode < 2.5f) {
            targetModeStrength = timbralStrength;
        } else {
            targetModeStrength = dynamicStrength;
        }
    }
    
    // Update current mode and strength
    m_currentMode = targetMode;
    // Smooth mode strength with mood-adjusted smoothing
    float modeStrengthAlpha = dt / (0.2f + dt);  // ~200ms time constant
    m_currentModeStrength += (targetModeStrength - m_currentModeStrength) * modeStrengthAlpha;
    
    // Smooth mode transitions (~500ms time constant for slow, smooth transitions)
    float modeAlpha = dt / (0.5f + dt);
    m_modeTransition += (targetMode - m_modeTransition) * modeAlpha;
    
    // Rhythmic: Pulse on beat with smoothed saliency intensity
    if (ctx.audio.isOnBeat()) {
        m_rhythmicPulse = 1.0f;
    } else {
        m_rhythmicPulse *= (1.0f - dt * 5.0f);  // ~200ms decay
    }
    m_rhythmicPulse = fmaxf(m_rhythmicPulse, m_smoothRhythmic * 0.5f);
    
    // Timbral: Texture intensity based on smoothed spectral changes
    // Use smoothed timbral saliency change (not comparing to target which is only updated on hops)
    float timbralChange = fabsf(m_smoothTimbral - m_lastTimbralSaliency);
    m_timbralTexture += (timbralChange - m_timbralTexture) * 0.3f;
    m_lastTimbralSaliency = m_smoothTimbral;
    
    // Dynamic: Energy level from smoothed RMS with smoothed saliency scaling
    m_dynamicEnergy += (m_smoothRms * (1.0f + m_smoothDynamic) - m_dynamicEnergy) * 0.2f;
    
    // =========================================================================
    // Frequency Smoothing: Calculate target frequencies and smooth ONCE per frame
    // =========================================================================
    // Base frequencies
    const float baseFreq1 = 0.16f;  // Low frequency (harmonic)
    const float baseFreq2 = 0.28f;  // Medium frequency (rhythmic)
    const float baseFreq3 = 0.12f;  // Very low frequency (timbral)
    
    // Calculate target frequencies based on mode (with interpolation)
    float targetFreq1 = baseFreq1;
    float targetFreq2 = baseFreq2;
    float targetFreq3 = baseFreq3;
    
    if (m_modeTransition >= 1.0f && m_modeTransition < 2.0f) {
        // Rhythmic mode: faster frequencies (interpolate)
        float blend = m_modeTransition - 1.0f;
        targetFreq1 = baseFreq1 * (1.0f + blend * 0.5f);  // 1.0x to 1.5x
        targetFreq2 = baseFreq2 * (1.0f + blend * 0.3f);  // 1.0x to 1.3x
    } else if (m_modeTransition >= 2.0f && m_modeTransition < 3.0f) {
        // Timbral mode: higher frequencies (interpolate)
        float blend = m_modeTransition - 2.0f;
        targetFreq1 = baseFreq1 * (1.5f + blend * 0.5f);  // 1.5x to 2.0x
        targetFreq2 = baseFreq2 * (1.3f + blend * 0.5f);  // 1.3x to 1.8x
        targetFreq3 = baseFreq3 * (1.0f + blend * 0.5f);  // 1.0x to 1.5x
    } else if (m_modeTransition >= 3.0f) {
        // Dynamic mode: back to base frequencies
        float blend = m_modeTransition - 3.0f;
        if (blend < 1.0f) {
            // Interpolate back from timbral
            targetFreq1 = baseFreq1 * (2.0f - blend * 0.5f);
            targetFreq2 = baseFreq2 * (1.8f - blend * 0.5f);
            targetFreq3 = baseFreq3 * (1.5f - blend * 0.5f);
        } else {
            targetFreq1 = baseFreq1;
            targetFreq2 = baseFreq2;
            targetFreq3 = baseFreq3;
        }
    }
    
    // Smooth frequency changes ONCE per frame (~500ms time constant to prevent visual jumps)
    float freqAlpha = dt / (0.5f + dt);
    m_smoothFreq1 += (targetFreq1 - m_smoothFreq1) * freqAlpha;
    m_smoothFreq2 += (targetFreq2 - m_smoothFreq2) * freqAlpha;
    m_smoothFreq3 += (targetFreq3 - m_smoothFreq3) * freqAlpha;
    
    // Pre-calculate mode transition weights (used inside loop)
    float harmonicWeight = 0.0f;
    float rhythmicWeight = 0.0f;
    float timbralWeight = 0.0f;
    float dynamicWeight = 0.0f;
    
    if (m_modeTransition < 0.5f) {
        harmonicWeight = 1.0f - m_modeTransition * 2.0f;
    }
    if (m_modeTransition >= 0.5f && m_modeTransition < 1.5f) {
        rhythmicWeight = 1.0f - fabsf(m_modeTransition - 1.0f) * 2.0f;
    }
    if (m_modeTransition >= 1.5f && m_modeTransition < 2.5f) {
        timbralWeight = 1.0f - fabsf(m_modeTransition - 2.0f) * 2.0f;
    }
    if (m_modeTransition >= 2.5f) {
        dynamicWeight = 1.0f - (m_modeTransition - 3.0f) * 2.0f;
        if (dynamicWeight < 0.0f) dynamicWeight = 0.0f;
    }
    
    // =========================================================================
    // Render: Multi-layer interference pattern with saliency modulation
    // =========================================================================
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        float distNorm = (float)dist / (float)HALF_LENGTH;
        float distFromCenter = (float)dist;
        
        // Use pre-calculated smoothed frequencies
        float freq1 = m_smoothFreq1;
        float freq2 = m_smoothFreq2;
        float freq3 = m_smoothFreq3;
        
        // Standing wave equations: sin(k*dist - phase) produces OUTWARD motion
        float wave1 = sinf(distFromCenter * freq1 - m_phase1);
        float wave2 = sinf(distFromCenter * freq2 - m_phase2);
        float wave3 = sinf(distFromCenter * freq3 - m_phase3);
        
        // Combine waves with weights (creates interference moiré)
        float interference = wave1 + wave2 * 0.6f + wave3 * 0.4f;
        interference /= 2.0f;  // Normalize
        
        // Audio Enhancement: Modulate brightness based on saliency
        // Use pre-calculated mode weights
        float audioGain = 0.4f;
        
        // Harmonic: Slow color drift, chord-responsive (use smoothed values)
        if (harmonicWeight > 0.0f) {
            audioGain += m_smoothHarmonic * 0.4f * harmonicWeight;
        }
        
        // Rhythmic: Beat-synced pulse (use smoothed values)
        if (rhythmicWeight > 0.0f) {
            float pulseBoost = m_rhythmicPulse * (1.0f - distNorm * 0.7f);
            audioGain += pulseBoost * 0.5f * rhythmicWeight;
        }
        
        // Timbral: High-frequency shimmer (use smoothed values)
        if (timbralWeight > 0.0f) {
            audioGain += m_timbralTexture * 0.4f * timbralWeight;
        }
        
        // Dynamic: Energy-based brightness (use smoothed values)
        if (dynamicWeight > 0.0f) {
            audioGain += m_dynamicEnergy * 0.5f * dynamicWeight;
        }
        
        // Apply audio gain to interference pattern
        float brightness = interference * audioGain;
        
        // Scale by smoothed overall saliency
        brightness *= (0.5f + m_smoothOverall * 0.5f);
        
        // Normalize brightness
        brightness = tanhf(brightness * 2.0f) * 0.5f + 0.5f;
        brightness = fminf(1.0f, brightness);
        
        // Color: Hue shifts based on mode and saliency
        uint8_t hue = ctx.gHue;
        if (ctx.audio.isHarmonicDominant() && ctx.audio.hasChord()) {
            // Harmonic mode: use chord root note
            hue += ctx.audio.rootNote() * 21;  // Chromatic circle
        } else {
            // Other modes: hue shifts with distance
            hue += (uint8_t)(distNorm * 50.0f);
        }
        
        // Get color from palette
        uint8_t bright = (uint8_t)(brightness * ctx.brightness);
        CRGB color = ctx.palette.getColor(hue, bright);
        
        // Set center pair
        SET_CENTER_PAIR(ctx, dist, color);
    }
    
    // Smooth center saliency boost (prevents white flash from sudden spikes)
    float targetBoost = 0.0f;
    if (m_smoothOverall > 0.5f) {
        targetBoost = m_smoothOverall * 0.3f;  // Reduced max boost
    }
    
    // Rolling average to filter single-frame spikes (like BreathingEffect)
    m_boostHistorySum -= m_boostHistory[m_boostHistIdx];
    m_boostHistory[m_boostHistIdx] = targetBoost;
    m_boostHistorySum += targetBoost;
    float avgTargetBoost = m_boostHistorySum / (float)BOOST_HISTORY_SIZE;
    m_boostHistIdx = (m_boostHistIdx + 1) % BOOST_HISTORY_SIZE;
    
    // Smooth boost with ~300ms time constant (prevents sudden flashes)
    float boostAlpha = dt / (0.3f + dt);
    m_saliencyBoostSmooth += (avgTargetBoost - m_saliencyBoostSmooth) * boostAlpha;
    
    // Apply smoothed boost at center (with fade) using alpha blending
    if (m_saliencyBoostSmooth > 0.01f) {
        uint8_t saliencyBoost = (uint8_t)(m_saliencyBoostSmooth * 255.0f);
        for (uint16_t dist = 0; dist < 3; ++dist) {
            float fade = 1.0f - ((float)dist / 3.0f);
            uint8_t fadedBoost = (uint8_t)(saliencyBoost * fade);
            
            // Calculate indices with bounds checking (prevent underflow)
            int16_t leftIdx = (int16_t)ctx.centerPoint - 1 - (int16_t)dist;
            uint16_t rightIdx = ctx.centerPoint + dist;
            
            // Alpha blending: blend boost into existing color (smoother than qadd8)
            float blendAlpha = (float)fadedBoost / 255.0f;
            
            if (leftIdx >= 0 && leftIdx < (int16_t)ctx.ledCount) {
                // Blend: new = old + (boost - old) * alpha
                ctx.leds[leftIdx].r = (uint8_t)(ctx.leds[leftIdx].r + (fadedBoost - ctx.leds[leftIdx].r) * blendAlpha);
                ctx.leds[leftIdx].g = (uint8_t)(ctx.leds[leftIdx].g + (fadedBoost - ctx.leds[leftIdx].g) * blendAlpha);
                ctx.leds[leftIdx].b = (uint8_t)(ctx.leds[leftIdx].b + (fadedBoost - ctx.leds[leftIdx].b) * blendAlpha);
            }
            if (rightIdx < ctx.ledCount) {
                ctx.leds[rightIdx].r = (uint8_t)(ctx.leds[rightIdx].r + (fadedBoost - ctx.leds[rightIdx].r) * blendAlpha);
                ctx.leds[rightIdx].g = (uint8_t)(ctx.leds[rightIdx].g + (fadedBoost - ctx.leds[rightIdx].g) * blendAlpha);
                ctx.leds[rightIdx].b = (uint8_t)(ctx.leds[rightIdx].b + (fadedBoost - ctx.leds[rightIdx].b) * blendAlpha);
            }
        }
    }
#endif  // FEATURE_AUDIO_SYNC
}

void SaliencyAwareEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& SaliencyAwareEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Saliency Aware",
        "Adapts visual behavior based on musical saliency (harmonic, rhythmic, timbral, dynamic)",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

