// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPWaveCollisionEffect.cpp
 * @brief LGP Wave Collision effect implementation
 */

#include "LGPWaveCollisionEffectEnhanced.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
#include "../../validation/EffectValidationMacros.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool LGPWaveCollisionEnhancedEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // CRITICAL FIX: Single phase for traveling waves
    m_phase = 0.0f;
    m_lastHopSeq = 0;
    m_chromaEnergySum = 0.0f;
    m_chromaHistIdx = 0;
    for (uint8_t i = 0; i < CHROMA_HISTORY; ++i) {
        m_chromaEnergyHist[i] = 0.0f;
    }
    m_energyAvg = 0.0f;
    m_energyDelta = 0.0f;
    m_dominantBin = 0;
    m_dominantBinSmooth = 0.0f;
    m_collisionBoost = 0.0f;
    m_speedTarget = 1.0f;

    // Initialize chromagram smoothing
    for (uint8_t i = 0; i < 12; i++) {
        m_chromaFollowers[i].reset(0.0f);
        m_chromaSmoothed[i] = 0.0f;
        m_chromaTargets[i] = 0.0f;
    }
    
    // Initialize enhancement utilities
    m_speedSpring.init(50.0f, 1.0f);  // stiffness=50, mass=1 (critically damped)
    m_speedSpring.reset(1.0f);         // Start at base speed
    m_energyAvgFollower.reset(0.0f);
    m_energyDeltaFollower.reset(0.0f);
    m_tempoLocked = false;
    return true;
}

void LGPWaveCollisionEnhancedEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN WAVE COLLISION - Wave packets expand outward from centre and collide
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;
    const bool hasAudio = ctx.audio.available;
    bool newHop = false;

#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
            
            // Update chromagram targets
            for (uint8_t i = 0; i < 12; i++) {
                m_chromaTargets[i] = ctx.audio.controlBus.heavy_chroma[i];
            }

            const float led_share = 255.0f / 12.0f;
            float chromaEnergy = 0.0f;
            float maxBinVal = 0.0f;
            uint8_t dominantBin = 0;
            for (uint8_t i = 0; i < 12; ++i) {
                // Use smoothed chromagram for energy calculation
                float bin = m_chromaSmoothed[i];
                // FIX: Use sqrt scaling instead of squaring to preserve low-level signals
                float bright = sqrtf(bin) * 1.5f;
                if (bright > 1.0f) bright = 1.0f;
                if (bright > maxBinVal) {
                    maxBinVal = bright;
                    dominantBin = i;
                }
                chromaEnergy += bright * led_share;
            }
            float energyNorm = chromaEnergy / 255.0f;
            if (energyNorm < 0.0f) energyNorm = 0.0f;
            if (energyNorm > 1.0f) energyNorm = 1.0f;

            m_chromaEnergySum -= m_chromaEnergyHist[m_chromaHistIdx];
            m_chromaEnergyHist[m_chromaHistIdx] = energyNorm;
            m_chromaEnergySum += energyNorm;
            m_chromaHistIdx = (m_chromaHistIdx + 1) % CHROMA_HISTORY;

            m_energyAvg = m_chromaEnergySum / CHROMA_HISTORY;
            m_energyDelta = energyNorm - m_energyAvg;
            if (m_energyDelta < 0.0f) m_energyDelta = 0.0f;
            m_dominantBin = dominantBin;
            
            // Enhanced: 64-bin Sub-Bass Detection (bins 0-5 = ~110-155 Hz)
            float subBassSum = 0.0f;
            for (uint8_t i = 0; i < 6; ++i) {
                subBassSum += ctx.audio.bin(i);
            }
            m_targetSubBass = subBassSum / 6.0f;
        }
    } else
#endif
    {
        m_energyAvg *= 0.98f;
        m_energyDelta = 0.0f;
    }

    float dt = enhancement::getSafeDeltaSeconds(ctx.deltaTimeSeconds);
    float moodNorm = ctx.getMoodNormalized();

    // Smooth chromagram with AsymmetricFollower (every frame)
    if (hasAudio) {
        for (uint8_t i = 0; i < 12; i++) {
            m_chromaSmoothed[i] = m_chromaFollowers[i].updateWithMood(
                m_chromaTargets[i], dt, moodNorm);
        }
        // Enhanced: Smooth sub-bass energy
        m_subBassEnergy = m_subBassFollower.updateWithMood(m_targetSubBass, dt, moodNorm);
    }

    // True exponential smoothing with AsymmetricFollower (frame-rate independent)
    float energyAvgSmooth = m_energyAvgFollower.updateWithMood(m_energyAvg, dt, moodNorm);
    float energyDeltaSmooth = m_energyDeltaFollower.updateWithMood(m_energyDelta, dt, moodNorm);

    // Dominant bin smoothing
    float alphaBin = 1.0f - expf(-dt / 0.25f);  // True exponential, 250ms time constant
    m_dominantBinSmooth += (m_dominantBin - m_dominantBinSmooth) * alphaBin;
    if (m_dominantBinSmooth < 0.0f) m_dominantBinSmooth = 0.0f;
    if (m_dominantBinSmooth > 11.0f) m_dominantBinSmooth = 11.0f;

    // Enhanced: Percussion-driven collision boost (snare = collision event!)
#if FEATURE_AUDIO_SYNC
    if (hasAudio && ctx.audio.isSnareHit()) {
        // Enhanced: Boost intensity with sub-bass energy
        m_collisionBoost = 1.0f + m_subBassEnergy * 0.3f;  // Up to 1.3x on snare with bass
    } else {
        // Fallback: energy delta still contributes, but weaker
        m_collisionBoost += energyDeltaSmooth * 0.4f;
    }
    if (m_collisionBoost > 1.3f) m_collisionBoost = 1.3f;  // Enhanced: Allow higher boost
    m_collisionBoost *= 0.88f;  // Slightly faster decay for snappier response

    // Hi-hat driven speed burst
    if (hasAudio && ctx.audio.isHihatHit()) {
        m_speedTarget = 1.6f;  // Temporary speed boost on hi-hat
    }
    // Decay speed target back to normal (exponential smoothing)
    m_speedTarget = m_speedTarget * 0.95f + 1.0f * 0.05f;

    // Enhanced: Use 64-bin sub-bass for wave intensity (more responsive than heavy_bands)
    float bassEnergy = m_subBassEnergy * 0.7f + ctx.audio.heavyBass() * 0.3f;  // Blend sub-bass with heavy_bands
#else
    m_collisionBoost += energyDeltaSmooth * 0.4f;
    if (m_collisionBoost > 1.0f) m_collisionBoost = 1.0f;
    m_collisionBoost *= 0.88f;
    m_speedTarget = m_speedTarget * 0.95f + 1.0f * 0.05f;
    float bassEnergy = energyAvgSmooth;
#endif

    // Speed modulation with Spring physics (natural momentum, no jitter)
    // Now modulated by bassEnergy (from heavy_bands) and speedTarget (from hi-hat)
    float rawSpeedScale = (0.7f + 0.6f * bassEnergy) * m_speedTarget;  // Capture raw speed for validation
    float speedTargetClamped = rawSpeedScale;
    if (speedTargetClamped > 1.6f) speedTargetClamped = 1.6f;  // Allow higher speed with hi-hat boost

    // Spring physics for speed modulation (replaces linear slew limiting)
    float smoothedSpeed = m_speedSpring.update(speedTargetClamped, dt);
    if (smoothedSpeed > 1.6f) smoothedSpeed = 1.6f;  // Hard clamp
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

    float prevPhase = m_phase;
    
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
    
    float phaseDelta = m_phase - prevPhase;

    // Validation instrumentation
    VALIDATION_INIT(17);  // Effect ID 17
    VALIDATION_PHASE(m_phase, phaseDelta);
    VALIDATION_SPEED(rawSpeedScale, smoothedSpeed);
    VALIDATION_AUDIO(m_dominantBinSmooth, energyAvgSmooth, energyDeltaSmooth);
    VALIDATION_REVERSAL_CHECK(m_prevPhaseDelta, phaseDelta);
    VALIDATION_SUBMIT(::lightwaveos::validation::g_validationRing);
    m_prevPhaseDelta = phaseDelta;

    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // Anti-aliased collision core at true center (79.5) using SubpixelRenderer
    if (m_collisionBoost > 0.05f) {
        float intensityNorm = ctx.brightness / 255.0f;
        uint8_t baseHue = (uint8_t)(ctx.gHue + m_dominantBinSmooth * (255.0f / 12.0f));
        CRGB collisionColor = ctx.palette.getColor(baseHue, 255);
        uint8_t collisionBright = (uint8_t)(m_collisionBoost * 200.0f * intensityNorm);

        // Render bright core at fractional center (between LED 79 and 80)
        enhancement::SubpixelRenderer::renderPoint(
            ctx.leds, STRIP_LENGTH, 79.5f, collisionColor, collisionBright
        );

        // Also render on strip 2
        if (STRIP_LENGTH * 2 <= ctx.ledCount) {
            enhancement::SubpixelRenderer::renderPoint(
                ctx.leds + STRIP_LENGTH, STRIP_LENGTH, 79.5f,
                ctx.palette.getColor(baseHue + 90, 255), collisionBright
            );
        }
    }

    for (int i = 0; i < STRIP_LENGTH; i++) {
        // CENTRE ORIGIN: Calculate distance from centre pair
        float distFromCenter = (float)centerPairDistance((uint16_t)i);

        // DIFFERENTIATED WAVE COLLISION: Longer wavelength than Interference Scanner
        // sin(k*dist - phase) produces OUTWARD motion when phase increases
        const float freqBase = 0.15f;  // ~42 LED wavelength (vs Scanner's ~25-31)
        float wave1 = sinf(distFromCenter * freqBase - m_phase);

        // COLLISION FLASH: Center-focused explosion on snare hits
        // collisionBoost decays from 1.0 (snare hit) with spatial falloff from center
        float collisionFlash = m_collisionBoost * expf(-distFromCenter * 0.12f);  // Bright at center, fades out

        // Base audio intensity (without uniform collision boost - moved to spatial flash)
        float audioIntensity = 0.4f + 0.5f * energyAvgSmooth + 0.4f * energyDeltaSmooth;
        // FIX: Add minimum amplitude floor for wave visibility at low bass
        audioIntensity = fmaxf(0.2f, audioIntensity);
        float interference = wave1 * audioIntensity + collisionFlash * 0.8f;  // Collision adds separate layer

        // CRITICAL: Use tanhf for uniform brightness (like ChevronWaves)
        interference = tanhf(interference * 2.0f) * 0.5f + 0.5f;

        uint8_t brightness = (uint8_t)(interference * 255.0f * intensityNorm);

        // CENTRE ORIGIN colour mapping
        uint8_t paletteIndex = (uint8_t)(distFromCenter * 2.0f + interference * 50.0f);
        uint8_t baseHue = (uint8_t)(ctx.gHue + (uint8_t)(m_dominantBinSmooth * (255.0f / 12.0f)));

        // Blend with existing pixel (preserves trails from fadeToBlackBy)
        // nblend uses 8-bit amount: 0=keep existing, 255=full replace
        CRGB newColor = ctx.palette.getColor((uint8_t)(baseHue + paletteIndex), brightness);
        nblend(ctx.leds[i], newColor, 180);  // ~70% new, ~30% existing
        
        if (i + STRIP_LENGTH < ctx.ledCount) {
            // FIX: Hue offset +90 matches ChevronWaves pattern (was +128)
            CRGB newColor2 = ctx.palette.getColor((uint8_t)(baseHue + paletteIndex + 90), brightness);
            nblend(ctx.leds[i + STRIP_LENGTH], newColor2, 180);  // Apply same blend to BOTH strips
        }
    }
}

void LGPWaveCollisionEnhancedEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPWaveCollisionEnhancedEffect::getMetadata() const {
    static plugins::EffectMetadata meta(
        "LGP Wave Collision Enhanced",
        "Enhanced: 64-bin sub-bass, enhanced snare/hi-hat triggers, beatPhase sync",
        plugins::EffectCategory::QUANTUM,
        1
    );
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
