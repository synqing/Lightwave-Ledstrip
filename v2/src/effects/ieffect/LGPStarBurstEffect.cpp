/**
 * @file LGPStarBurstEffect.cpp
 * @brief LGP Star Burst effect implementation
 */

#include "LGPStarBurstEffect.h"
#include "../CoreEffects.h"
#include "../enhancement/MotionEngine.h"
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

LGPStarBurstEffect::LGPStarBurstEffect()
    : m_phase(0.0f)
{
}

bool LGPStarBurstEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    m_burst = 0.0f;
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
    m_hihatFlash = 0.0f;
    return true;
}

void LGPStarBurstEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN - Star-like patterns radiating from centre
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

            if (m_energyDelta > 0.05f) {
                m_burst = 1.0f;
            }

            // Force max burst on snare hit
            if (ctx.audio.isSnareHit()) {
                m_burst = 1.0f;
            }

            // Hi-hat triggers brightness overlay
            if (ctx.audio.isHihatHit()) {
                m_hihatFlash = 0.5f;
            }
        }
    } else
#endif
    {
        m_energyAvg *= 0.98f;
        m_energyDelta = 0.0f;
    }

    // Hi-hat flash decay (runs every frame for smooth decay)
    m_hihatFlash *= 0.85f;

    float dt = enhancement::clampDtSeconds(ctx.deltaTimeMs * 0.001f);
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

    float speedScale = 0.3f + 1.2f * m_energyAvgSmooth + 2.0f * m_energyDeltaSmooth;
    m_phase = enhancement::advancePhase(m_phase, speedNorm, speedScale, dt);
    m_burst += m_energyDeltaSmooth * 0.9f;
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        float fastFlux = ctx.audio.fastFlux();
        m_burst += fastFlux * 0.4f;
    }
#endif
    if (m_burst > 1.0f) m_burst = 1.0f;
    m_burst *= 0.90f;

    fadeToBlackBy(ctx.leds, ctx.ledCount, 20);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);

        // DEFINITIVE FIX: NO decay envelope - match ChevronWaves pattern
        // sin(k*dist - phase) produces OUTWARD motion when phase increases
        const float freqBase = 0.3f;
        float star = sinf(distFromCenter * freqBase - m_phase);

        // Audio modulation (amplitude, not synchronized pulsing)
        star *= (0.4f + 0.6f * m_energyAvgSmooth);
        star *= (1.0f + 0.8f * m_burst);

        // CRITICAL: Use tanhf for uniform brightness (like ChevronWaves)
        star = tanhf(star * 2.0f) * 0.5f + 0.5f;

        // Apply hi-hat flash overlay to brightness
        float brightnessWithFlash = star * intensityNorm * (1.0f + m_hihatFlash);
        if (brightnessWithFlash > 1.0f) brightnessWithFlash = 1.0f;
        uint8_t brightness = (uint8_t)(brightnessWithFlash * 255.0f);

        uint8_t paletteIndex = (uint8_t)(distFromCenter + star * 50.0f);

        // Use chord detection for hue when confidence is sufficient
        uint8_t baseHue;
#if FEATURE_AUDIO_SYNC
        if (hasAudio && ctx.audio.chordConfidence() > 0.3f) {
            // Root note (0-11) mapped to hue range (0-255)
            uint8_t rootHue = ctx.audio.rootNote() * 21;  // 12 notes -> 256 hue range
            // Apply chord mood shift (palette-relative offset)
            if (ctx.audio.isMajor()) {
                rootHue += 30;       // Warm shift for major chords
            } else if (ctx.audio.isMinor()) {
                rootHue -= 20;       // Cool shift for minor chords
            }
            baseHue = ctx.gHue + rootHue;
        } else
#endif
        {
            // Fallback to smoothed dominant bin when no chord detected
            baseHue = (uint8_t)(ctx.gHue + (uint8_t)(m_dominantBinSmooth * (255.0f / 12.0f)));
        }

        ctx.leds[i] += ctx.palette.getColor((uint8_t)(baseHue + paletteIndex), brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] += ctx.palette.getColor((uint8_t)(baseHue + paletteIndex + 85), brightness);
        }
    }
}

void LGPStarBurstEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPStarBurstEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Star Burst",
        "Explosive radial lines",
        plugins::EffectCategory::GEOMETRIC,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
