/**
 * @file LGPWaveCollisionEffect.cpp
 * @brief LGP Wave Collision effect implementation
 */

#include "LGPWaveCollisionEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
#include "../../validation/EffectValidationMacros.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

static float smoothValue(float current, float target, float rise, float fall) {
    float alpha = (target > current) ? rise : fall;
    return current + (target - current) * alpha;
}

bool LGPWaveCollisionEffect::init(plugins::EffectContext& ctx) {
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
    m_energyAvgSmooth = 0.0f;
    m_energyDeltaSmooth = 0.0f;
    m_dominantBinSmooth = 0.0f;
    m_collisionBoost = 0.0f;
    m_speedScaleSmooth = 1.0f;
    m_speedTarget = 1.0f;
    return true;
}

void LGPWaveCollisionEffect::render(plugins::EffectContext& ctx) {
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

            const float led_share = 255.0f / 12.0f;
            float chromaEnergy = 0.0f;
            float maxBinVal = 0.0f;
            uint8_t dominantBin = 0;
            for (uint8_t i = 0; i < 12; ++i) {
                float bin = ctx.audio.controlBus.chroma[i];
                float bright = bin * bin;
                bright *= 1.5f;
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
        }
    } else
#endif
    {
        m_energyAvg *= 0.98f;
        m_energyDelta = 0.0f;
    }

    float dt = ctx.deltaTimeMs * 0.001f;
    float riseAvg = dt / (0.20f + dt);
    float fallAvg = dt / (0.50f + dt);
    // JOG-DIAL FIX: Slower delta response prevents jitter-induced speed changes
    float riseDelta = dt / (0.25f + dt);   // 250ms rise (was 80ms)
    float fallDelta = dt / (0.40f + dt);   // 400ms fall (was 250ms)
    float alphaBin = dt / (0.25f + dt);

    m_energyAvgSmooth = smoothValue(m_energyAvgSmooth, m_energyAvg, riseAvg, fallAvg);
    m_energyDeltaSmooth = smoothValue(m_energyDeltaSmooth, m_energyDelta, riseDelta, fallDelta);
    m_dominantBinSmooth += (m_dominantBin - m_dominantBinSmooth) * alphaBin;
    if (m_dominantBinSmooth < 0.0f) m_dominantBinSmooth = 0.0f;
    if (m_dominantBinSmooth > 11.0f) m_dominantBinSmooth = 11.0f;

    // Percussion-driven collision boost (snare = collision event!)
#if FEATURE_AUDIO_SYNC
    if (hasAudio && ctx.audio.isSnareHit()) {
        m_collisionBoost = 1.0f;  // Force max brightness on snare hit
    } else {
        // Fallback: energy delta still contributes, but weaker
        m_collisionBoost += m_energyDeltaSmooth * 0.4f;
    }
    if (m_collisionBoost > 1.0f) m_collisionBoost = 1.0f;
    m_collisionBoost *= 0.88f;  // Slightly faster decay for snappier response

    // Hi-hat driven speed burst
    if (hasAudio && ctx.audio.isHihatHit()) {
        m_speedTarget = 1.6f;  // Temporary speed boost on hi-hat
    }
    // Decay speed target back to normal (exponential smoothing)
    m_speedTarget = m_speedTarget * 0.95f + 1.0f * 0.05f;

    // Use heavy_bands for smoother bass energy (replaces raw chroma energy)
    float bassEnergy = ctx.audio.heavyBass();  // Already averages heavy_bands[0] + [1]
#else
    m_collisionBoost += m_energyDeltaSmooth * 0.4f;
    if (m_collisionBoost > 1.0f) m_collisionBoost = 1.0f;
    m_collisionBoost *= 0.88f;
    m_speedTarget = m_speedTarget * 0.95f + 1.0f * 0.05f;
    float bassEnergy = m_energyAvgSmooth;
#endif

    // JOG-DIAL FIX: Constant base speed with gentle energy modulation (no energyDelta on speed)
    // Now modulated by bassEnergy (from heavy_bands) and speedTarget (from hi-hat)
    float rawSpeedScale = (0.7f + 0.6f * bassEnergy) * m_speedTarget;  // Capture raw speed for validation
    float speedTargetClamped = rawSpeedScale;
    if (speedTargetClamped > 1.6f) speedTargetClamped = 1.6f;  // Allow higher speed with hi-hat boost

    // Slew limiter: max 0.25 units/sec change rate
    const float maxSlewRate = 0.25f;
    float slewLimit = maxSlewRate * dt;
    float speedDelta = speedTargetClamped - m_speedScaleSmooth;
    if (speedDelta > slewLimit) speedDelta = slewLimit;
    if (speedDelta < -slewLimit) speedDelta = -slewLimit;
    m_speedScaleSmooth += speedDelta;

    // Capture phase before update for delta calculation
    float prevPhase = m_phase;
    // FIX: Use 240.0f multiplier like ChevronWaves (was 4.0f - 60x slower!)
    // Fast phase accumulation makes forward motion perceptually dominant
    m_phase += speedNorm * 240.0f * m_speedScaleSmooth * dt;
    if (m_phase > 628.3f) m_phase -= 628.3f;  // Wrap at 100*2π to prevent float overflow
    float phaseDelta = m_phase - prevPhase;

    // Validation instrumentation
    VALIDATION_INIT(17);  // Effect ID 17
    VALIDATION_PHASE(m_phase, phaseDelta);
    VALIDATION_SPEED(rawSpeedScale, m_speedScaleSmooth);
    VALIDATION_AUDIO(m_dominantBinSmooth, m_energyAvgSmooth, m_energyDeltaSmooth);
    VALIDATION_REVERSAL_CHECK(m_prevPhaseDelta, phaseDelta);
    VALIDATION_SUBMIT(::lightwaveos::validation::g_validationRing);
    m_prevPhaseDelta = phaseDelta;

    fadeToBlackBy(ctx.leds, ctx.ledCount, 30);

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
        float audioIntensity = 0.4f + 0.5f * m_energyAvgSmooth + 0.4f * m_energyDeltaSmooth;
        float interference = wave1 * audioIntensity + collisionFlash * 0.8f;  // Collision adds separate layer

        // CRITICAL: Use tanhf for uniform brightness (like ChevronWaves)
        interference = tanhf(interference * 2.0f) * 0.5f + 0.5f;

        uint8_t brightness = (uint8_t)(interference * 255.0f * intensityNorm);

        // CENTRE ORIGIN colour mapping
        uint8_t paletteIndex = (uint8_t)(distFromCenter * 2.0f + interference * 50.0f);
        uint8_t baseHue = (uint8_t)(ctx.gHue + (uint8_t)(m_dominantBinSmooth * (255.0f / 12.0f)));

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(baseHue + paletteIndex), brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            // FIX: Hue offset +90 matches ChevronWaves pattern (was +128)
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(baseHue + paletteIndex + 90), brightness);
        }
    }
}

void LGPWaveCollisionEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPWaveCollisionEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Wave Collision",
        "Colliding wave fronts creating standing nodes",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
