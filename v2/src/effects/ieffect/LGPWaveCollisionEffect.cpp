/**
 * @file LGPWaveCollisionEffect.cpp
 * @brief LGP Wave Collision effect implementation
 */

#include "LGPWaveCollisionEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
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
    float riseDelta = dt / (0.08f + dt);
    float fallDelta = dt / (0.25f + dt);
    float alphaBin = dt / (0.25f + dt);

    m_energyAvgSmooth = smoothValue(m_energyAvgSmooth, m_energyAvg, riseAvg, fallAvg);
    m_energyDeltaSmooth = smoothValue(m_energyDeltaSmooth, m_energyDelta, riseDelta, fallDelta);
    m_dominantBinSmooth += (m_dominantBin - m_dominantBinSmooth) * alphaBin;
    if (m_dominantBinSmooth < 0.0f) m_dominantBinSmooth = 0.0f;
    if (m_dominantBinSmooth > 11.0f) m_dominantBinSmooth = 11.0f;

    m_collisionBoost += m_energyDeltaSmooth * 0.6f;
    if (m_collisionBoost > 1.0f) m_collisionBoost = 1.0f;
    m_collisionBoost *= 0.90f;

    // CRITICAL FIX: Single phase accumulator for traveling waves
    float speedScale = 0.4f + 1.6f * m_energyAvgSmooth + 2.0f * m_energyDeltaSmooth;
    m_phase += speedNorm * 4.0f * speedScale * dt;  // dt-corrected: moderate speed

    fadeToBlackBy(ctx.leds, ctx.ledCount, 30);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        // CENTRE ORIGIN: Calculate distance from centre pair
        float distFromCenter = (float)centerPairDistance((uint16_t)i);

        // DEFINITIVE FIX: NO decay envelope - match ChevronWaves pattern
        // sin(k*dist - phase) produces OUTWARD motion when phase increases
        const float freqBase = 0.2f;

        // Two traveling waves at slightly different frequencies for interference
        float wave1 = sinf(distFromCenter * freqBase - m_phase);
        float wave2 = sinf(distFromCenter * (freqBase * 0.7f) - m_phase + PI);

        // Collision interference pattern with audio modulation
        float interference = (wave1 + wave2) * (0.3f + 0.7f * m_energyAvgSmooth + 0.5f * m_collisionBoost);

        // CRITICAL: Use tanhf for uniform brightness (like ChevronWaves)
        interference = tanhf(interference * 2.0f) * 0.5f + 0.5f;

        uint8_t brightness = (uint8_t)(interference * 255.0f * intensityNorm);

        // CENTRE ORIGIN colour mapping
        uint8_t paletteIndex = (uint8_t)(distFromCenter * 2.0f + interference * 50.0f);
        uint8_t baseHue = (uint8_t)(ctx.gHue + (uint8_t)(m_dominantBinSmooth * (255.0f / 12.0f)));

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(baseHue + paletteIndex), brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(baseHue + paletteIndex + 128), brightness);
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
