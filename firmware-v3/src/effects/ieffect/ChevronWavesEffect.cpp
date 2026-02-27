/**
 * @file ChevronWavesEffect.cpp
 * @brief LGP Chevron Waves implementation
 */

#include "ChevronWavesEffect.h"
#include "ChromaUtils.h"
#include "../CoreEffects.h"
#include "../utils/FastLEDOptim.h"
#include "../../config/features.h"
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

ChevronWavesEffect::ChevronWavesEffect()
    : m_chevronPos(0.0f)
{
}

bool ChevronWavesEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
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
    m_chromaAngle = 0.0f;
    m_chromaHue = 0.0f;

    // Initialize enhancement utilities
    m_phaseSpeedSpring.init(50.0f, 1.0f);  // stiffness=50, mass=1 (critically damped)
    m_phaseSpeedSpring.reset(1.0f);        // Start at base speed
    m_energyAvgFollower.reset(0.0f);
    m_energyDeltaFollower.reset(0.0f);

    return true;
}

void ChevronWavesEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN - V-shaped patterns from centre

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
        // dt-corrected decay when audio unavailable
        float dtFallback = enhancement::getSafeDeltaSeconds(ctx.rawDeltaTimeSeconds);
        m_energyAvg *= powf(0.98f, dtFallback * 60.0f);
        m_energyDelta = 0.0f;
    }

    float rawDt = enhancement::getSafeDeltaSeconds(ctx.rawDeltaTimeSeconds);
    float dt = enhancement::getSafeDeltaSeconds(ctx.deltaTimeSeconds);

    // True exponential smoothing with AsymmetricFollower (frame-rate independent)
    float moodNorm = ctx.mood / 255.0f;  // 0=reactive, 1=smooth
    float energyAvgSmooth = m_energyAvgFollower.updateWithMood(m_energyAvg, rawDt, moodNorm);
    float energyDeltaSmooth = m_energyDeltaFollower.updateWithMood(m_energyDelta, rawDt, moodNorm);

    // Circular chroma hue smoothing (replaces linear EMA on bin index)
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        m_chromaHue = static_cast<float>(effects::chroma::circularChromaHueSmoothed(
            ctx.audio.controlBus.chroma, m_chromaAngle, rawDt, 0.20f));
    }
#endif

    // Use heavy_bands instead of raw chroma/energyAvg to eliminate jitter
    float heavyEnergy = 0.0f;
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        heavyEnergy = (ctx.audio.controlBus.heavy_bands[1] +
                       ctx.audio.controlBus.heavy_bands[2]) / 2.0f;
    }
#endif
    float targetSpeed = 0.6f + 1.2f * heavyEnergy;  // Reduced range for stability

    // Spring physics for speed modulation (natural momentum, no jitter)
    float smoothedSpeed = m_phaseSpeedSpring.update(targetSpeed, rawDt);
    if (smoothedSpeed > 2.0f) smoothedSpeed = 2.0f;  // Hard clamp
    if (smoothedSpeed < 0.3f) smoothedSpeed = 0.3f;  // Prevent stalling
    m_chevronPos += speedNorm * 240.0f * smoothedSpeed * dt;  // dt-corrected: 240/sec at speedNorm=1

    fadeToBlackBy(ctx.leds, ctx.ledCount, FADE_AMOUNT);

    for (uint16_t i = 0; i < ctx.ledCount && i < STRIP_LENGTH; i++) {
        // CRITICAL FIX: Use centerPairDistance() like working effects
        float distFromCenter = (float)centerPairDistance(i);

        // CRITICAL FIX: Match reference pattern with moderate spatial frequency
        // sin(k*dist - phase) produces OUTWARD motion when phase increases
        const float freqBase = 0.25f;  // Wavelength ~25 LEDs (was ~7 with CHEVRON_COUNT)
        float chevron = sinf(distFromCenter * freqBase - m_chevronPos);

        // Sharp edges with snare-triggered crispness
        float tanhScale = 2.0f;  // Base sharpness
#if FEATURE_AUDIO_SYNC
        if (hasAudio && ctx.audio.isSnareHit()) {
            tanhScale = 5.0f;  // Sharp, crisp chevrons on snare
        }
#endif
        chevron = tanhf(chevron * (tanhScale + 4.0f * energyAvgSmooth)) * 0.5f + 0.5f;

        float audioGain = 0.2f + 0.8f * energyAvgSmooth;
        uint8_t brightness = (uint8_t)(chevron * 255.0f * intensityNorm * audioGain);
        // Calculate hue with proper modular arithmetic (avoids UB from large float->uint8_t cast)
        // m_chromaHue is already 0-255 from circular chroma smoothing
        float rawHue = (float)ctx.gHue
                     + m_chromaHue
                     + distFromCenter * 2.0f
                     + fmodf(m_chevronPos * 0.5f, 256.0f);
        uint8_t hue = (uint8_t)fmodf(rawHue, 256.0f);

        // Direct assignment - fadeToBlackBy clears buffer each frame
        ctx.leds[i] = ctx.palette.getColor(hue, brightness);

        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(hue + 90, brightness);
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
