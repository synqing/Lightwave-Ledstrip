/**
 * @file ChevronWavesEffect.cpp
 * @brief LGP Chevron Waves implementation
 */

#include "ChevronWavesEffect.h"
#include "../CoreEffects.h"
#include "../utils/FastLEDOptim.h"
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

static float smoothValue(float current, float target, float rise, float fall) {
    float alpha = (target > current) ? rise : fall;
    return current + (target - current) * alpha;
}

ChevronWavesEffect::ChevronWavesEffect()
    : m_chevronPos(0.0f)
{
}

bool ChevronWavesEffect::init(plugins::EffectContext& ctx) {
    m_chevronPos = 0.0f;
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
    return true;
}

void ChevronWavesEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN - V-shaped patterns from centre

    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;
    const bool hasAudio = ctx.audio.available;
    bool newHop = false;

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
    } else {
        m_energyAvg *= 0.98f;
        m_energyDelta = 0.0f;
    }

    float dt = ctx.deltaTimeMs * 0.001f;
    float riseAvg = dt / (0.20f + dt);
    float fallAvg = dt / (0.50f + dt);
    float riseDelta = dt / (0.18f + dt);   // 180ms rise (was 80ms)
    float fallDelta = dt / (0.25f + dt);
    float alphaBin = dt / (0.25f + dt);

    m_energyAvgSmooth = smoothValue(m_energyAvgSmooth, m_energyAvg, riseAvg, fallAvg);
    m_energyDeltaSmooth = smoothValue(m_energyDeltaSmooth, m_energyDelta, riseDelta, fallDelta);
    m_dominantBinSmooth += (m_dominantBin - m_dominantBinSmooth) * alphaBin;
    if (m_dominantBinSmooth < 0.0f) m_dominantBinSmooth = 0.0f;
    if (m_dominantBinSmooth > 11.0f) m_dominantBinSmooth = 11.0f;

    float speedScale = 0.5f + 0.8f * m_energyAvgSmooth + 1.2f * m_energyDeltaSmooth;
    if (speedScale > 2.0f) speedScale = 2.0f;  // Hard clamp
    m_chevronPos += speedNorm * 240.0f * speedScale * dt;  // dt-corrected: 240/sec at speedNorm=1

    fadeToBlackBy(ctx.leds, ctx.ledCount, FADE_AMOUNT);

    for (uint16_t i = 0; i < ctx.ledCount && i < STRIP_LENGTH; i++) {
        // CRITICAL FIX: Use centerPairDistance() like working effects
        float distFromCenter = (float)centerPairDistance(i);

        // CRITICAL FIX: Match reference pattern with moderate spatial frequency
        // sin(k*dist - phase) produces OUTWARD motion when phase increases
        const float freqBase = 0.25f;  // Wavelength ~25 LEDs (was ~7 with CHEVRON_COUNT)
        float chevron = sinf(distFromCenter * freqBase - m_chevronPos);

        // Sharp edges
        chevron = tanhf(chevron * (2.0f + 4.0f * m_energyAvgSmooth)) * 0.5f + 0.5f;

        float audioGain = 0.2f + 0.8f * m_energyAvgSmooth;
        uint8_t brightness = (uint8_t)(chevron * 255.0f * intensityNorm * audioGain);
        uint8_t baseHue = (uint8_t)(ctx.gHue + (uint8_t)(m_dominantBinSmooth * (255.0f / 12.0f)));
        uint8_t hue = baseHue + (uint8_t)(distFromCenter * 2.0f) + (uint8_t)(m_chevronPos * 0.5f);

        CRGB color = ctx.palette.getColor(hue, brightness);
        ctx.leds[i] += color;
        
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] += ctx.palette.getColor(hue + 90, brightness);
        }
    }
}

void ChevronWavesEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& ChevronWavesEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Chevron Waves",
        "V-shaped wave propagation from centre",
        plugins::EffectCategory::GEOMETRIC,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
