/**
 * @file LGPInterferenceScannerEffect.cpp
 * @brief LGP Interference Scanner effect implementation
 */

#include "LGPInterferenceScannerEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

static float smoothValue(float current, float target, float rise, float fall) {
    float alpha = (target > current) ? rise : fall;
    return current + (target - current) * alpha;
}

LGPInterferenceScannerEffect::LGPInterferenceScannerEffect()
    : m_scanPhase(0.0f)
{
}

bool LGPInterferenceScannerEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_scanPhase = 0.0f;
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

void LGPInterferenceScannerEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN INTERFERENCE SCANNER - Creates scanning interference patterns
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
    float riseDelta = dt / (0.08f + dt);
    float fallDelta = dt / (0.25f + dt);
    float alphaBin = dt / (0.25f + dt);

    m_energyAvgSmooth = smoothValue(m_energyAvgSmooth, m_energyAvg, riseAvg, fallAvg);
    m_energyDeltaSmooth = smoothValue(m_energyDeltaSmooth, m_energyDelta, riseDelta, fallDelta);
    m_dominantBinSmooth += (m_dominantBin - m_dominantBinSmooth) * alphaBin;
    if (m_dominantBinSmooth < 0.0f) m_dominantBinSmooth = 0.0f;
    if (m_dominantBinSmooth > 11.0f) m_dominantBinSmooth = 11.0f;

    float speedScale = 0.4f + 1.2f * m_energyAvgSmooth + 2.0f * m_energyDeltaSmooth;
    m_scanPhase += speedNorm * 0.05f * speedScale;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float dist = (float)centerPairDistance((uint16_t)i);

        // Radial scan from centre
        float ringRadius = fmodf(m_scanPhase * 30.0f, (float)HALF_LENGTH);
        const float ringWidth = 8.0f + 20.0f * m_energyAvgSmooth;

        float pattern = 0.0f;
        if (fabsf(ringRadius - dist) < ringWidth) {
            pattern = cosf((ringRadius - dist) / ringWidth * PI / 2.0f);
        }

        // Add dual sweep interference (centre-to-edge travel)
        float wavePhase = m_scanPhase * 0.3f;  // Match ring perceptual velocity
        float wave1 = sinf(dist * 0.1f - wavePhase);
        float wave2 = sinf(dist * 0.1f - wavePhase + PI/3);  // Fixed offset, same speed
        pattern += (wave1 + wave2) * (0.15f + 0.6f * m_energyAvgSmooth);

        // Clamp
        pattern = fmaxf(-1.0f, fminf(1.0f, pattern));

        float audioGain = 0.3f + 0.7f * m_energyAvgSmooth;
        uint8_t brightness = (uint8_t)((pattern * 0.5f + 0.5f) * 255.0f * intensityNorm * audioGain);
        uint8_t paletteIndex = (uint8_t)(dist * 2.0f + pattern * 50.0f);
        uint8_t baseHue = (uint8_t)(ctx.gHue + (uint8_t)(m_dominantBinSmooth * (255.0f / 12.0f)));

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(baseHue + paletteIndex), brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(baseHue + paletteIndex + 128),
                                                             (uint8_t)(255 - brightness));
        }
    }
}

void LGPInterferenceScannerEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPInterferenceScannerEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Interference Scanner",
        "Scanning beam with interference fringes",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
