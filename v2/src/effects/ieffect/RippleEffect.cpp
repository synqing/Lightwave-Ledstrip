/**
 * @file RippleEffect.cpp
 * @brief Ripple effect implementation
 */

#include "RippleEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

RippleEffect::RippleEffect()
{
    // Initialize all ripples as inactive
    for (uint8_t i = 0; i < MAX_RIPPLES; i++) {
        m_ripples[i].active = false;
        m_ripples[i].radius = 0;
        m_ripples[i].speed = 0;
        m_ripples[i].hue = 0;
        m_ripples[i].intensity = 255;
    }
    memset(m_radial, 0, sizeof(m_radial));
    memset(m_radialAux, 0, sizeof(m_radialAux));
}

bool RippleEffect::init(plugins::EffectContext& ctx) {
    // Reset all ripples
    for (uint8_t i = 0; i < MAX_RIPPLES; i++) {
        m_ripples[i].active = false;
        m_ripples[i].radius = 0;
        m_ripples[i].intensity = 255;
    }
    m_lastHopSeq = 0;
    m_spawnCooldown = 0;
    m_lastChromaEnergy = 0.0f;
    m_chromaEnergySum = 0.0f;
    m_chromaHistIdx = 0;
    for (uint8_t i = 0; i < CHROMA_HISTORY; ++i) {
        m_chromaEnergyHist[i] = 0.0f;
    }
    memset(m_radial, 0, sizeof(m_radial));
    memset(m_radialAux, 0, sizeof(m_radialAux));
    return true;
}

void RippleEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN RIPPLE - Ripples expand from center
    //
    // STATEFUL EFFECT: This effect maintains ripple state (position, speed, hue) across frames
    // in the m_ripples[] array. Ripples spawn at center and expand outward. Identified
    // in PatternRegistry::isStatefulEffect().
    
    fadeToBlackBy(m_radial, HALF_LENGTH, 20);

    const bool hasAudio = ctx.audio.available;
    bool newHop = false;
    if (hasAudio) {
        newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
        }
    }

    uint8_t spawnChance = 0;
    float energyNorm = 0.0f;
    float energyDelta = 0.0f;
    uint8_t dominantBin = 0;
    float energyAvg = 0.0f;
    if (hasAudio && newHop) {
        const float led_share = 255.0f / 12.0f;
        float chromaEnergy = 0.0f;
        float maxBinVal = 0.0f;
        for (uint8_t i = 0; i < 12; ++i) {
            float bin = ctx.audio.controlBus.chroma[i];
            float bright = bin;
            bright = bright * bright;
            bright *= 1.5f;
            if (bright > 1.0f) bright = 1.0f;
            if (bright > maxBinVal) {
                maxBinVal = bright;
                dominantBin = i;
            }
            chromaEnergy += bright * led_share;
        }
        energyNorm = chromaEnergy / 255.0f;
        if (energyNorm < 0.0f) energyNorm = 0.0f;
        if (energyNorm > 1.0f) energyNorm = 1.0f;

        m_chromaEnergySum -= m_chromaEnergyHist[m_chromaHistIdx];
        m_chromaEnergyHist[m_chromaHistIdx] = energyNorm;
        m_chromaEnergySum += energyNorm;
        m_chromaHistIdx = (m_chromaHistIdx + 1) % CHROMA_HISTORY;
        energyAvg = m_chromaEnergySum / CHROMA_HISTORY;

        energyDelta = energyNorm - energyAvg;
        if (energyDelta < 0.0f) energyDelta = 0.0f;
        m_lastChromaEnergy = energyNorm;

        float chanceF = energyDelta * 510.0f + energyAvg * 80.0f;
        if (chanceF > 255.0f) chanceF = 255.0f;
        spawnChance = (uint8_t)chanceF;
    } else if (!hasAudio) {
        spawnChance = 10;
    }

    if (m_spawnCooldown > 0) {
        m_spawnCooldown--;
    }

    // Spawn new ripples at centre (audio-reactive when available)
    if (hasAudio && !newHop) {
        spawnChance = 0;
    }

    if (spawnChance > 0 && m_spawnCooldown == 0 && random8() < spawnChance) {
        for (int i = 0; i < MAX_RIPPLES; i++) {
            if (!m_ripples[i].active) {
                float speedBase = 0.5f + (random8() / 255.0f) * 2.0f;
                float speedBoost = 1.0f;
                uint8_t intensity = 180;
                if (hasAudio) {
                    float energy = energyAvg;
                    speedBoost += energy * 1.5f + energyDelta * 1.5f;
                    float intensityF = 100.0f + energy * 155.0f + energyDelta * 100.0f;
                    if (intensityF > 255.0f) intensityF = 255.0f;
                    intensity = (uint8_t)intensityF;
                }

                m_ripples[i].radius = 0;
                m_ripples[i].speed = speedBase * speedBoost;
                if (m_ripples[i].speed > 4.0f) m_ripples[i].speed = 4.0f;
                if (hasAudio) {
                    float hueBase = (dominantBin * (255.0f / 12.0f)) + ctx.gHue;
                    m_ripples[i].hue = (uint8_t)hueBase;
                } else {
                    m_ripples[i].hue = random8();
                }
                m_ripples[i].intensity = intensity;
                m_ripples[i].active = true;
                m_spawnCooldown = hasAudio ? 1 : 4;
                break;
            }
        }
    }

    // Update and render ripples
    for (int r = 0; r < MAX_RIPPLES; r++) {
        if (!m_ripples[r].active) continue;

        m_ripples[r].radius += m_ripples[r].speed * (ctx.speed / 10.0f);

        if (m_ripples[r].radius > HALF_LENGTH) {
            m_ripples[r].active = false;
            continue;
        }

        // Draw ripple into radial history buffer
        for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
            float wavePos = (float)dist - m_ripples[r].radius;
            float waveAbs = fabsf(wavePos);
            if (waveAbs < 3.0f) {
                uint8_t brightness = 255 - (uint8_t)(waveAbs * 85.0f);
                uint8_t edgeFade = (uint8_t)((HALF_LENGTH - m_ripples[r].radius) * 255.0f / HALF_LENGTH);
                brightness = scale8(brightness, edgeFade);
                brightness = scale8(brightness, m_ripples[r].intensity);

                CRGB color = ctx.palette.getColor(
                    m_ripples[r].hue + (uint8_t)dist,
                    brightness);
                m_radial[dist] += color;
            }
        }
    }

    memcpy(m_radialAux, m_radial, sizeof(m_radial));

    // Render radial history buffer to LEDs (centre-origin)
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        SET_CENTER_PAIR(ctx, dist, m_radialAux[dist]);
    }
}

void RippleEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& RippleEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Ripple",
        "Audio-reactive ripples expanding from centre",
        plugins::EffectCategory::WATER,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
