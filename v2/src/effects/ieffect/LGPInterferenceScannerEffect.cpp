/**
 * @file LGPInterferenceScannerEffect.cpp
 * @brief LGP Interference Scanner effect implementation
 */

#include "LGPInterferenceScannerEffect.h"
#include "../CoreEffects.h"
#include "../enhancement/MotionEngine.h"
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
    m_speedScaleSmooth = 1.0f;
    return true;
}

void LGPInterferenceScannerEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN INTERFERENCE SCANNER - Creates scanning interference patterns
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

    float dt = enhancement::clampDtSeconds(ctx.deltaTimeMs * 0.001f);
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

    // JOG-DIAL FIX: Decouple speed from transients - use base speed + gentle energy modulation
    // Target range: 0.6 to 1.4 (2.3x variation max, not 10x)
    float rawSpeedScale = 0.6f + 0.8f * m_energyAvgSmooth;  // Capture raw speed for validation
    float speedTarget = rawSpeedScale;
    if (speedTarget > 1.4f) speedTarget = 1.4f;

    // Slew limiter: max 0.3 units/sec change rate
    const float maxSlewRate = 0.3f;
    float slewLimit = maxSlewRate * dt;
    float speedDelta = speedTarget - m_speedScaleSmooth;
    if (speedDelta > slewLimit) speedDelta = slewLimit;
    if (speedDelta < -slewLimit) speedDelta = -slewLimit;
    m_speedScaleSmooth += speedDelta;

    // Capture phase before update for delta calculation
    float prevPhase = m_scanPhase;
    m_scanPhase = enhancement::advancePhase(m_scanPhase, speedNorm, m_speedScaleSmooth, dt);
    float phaseDelta = m_scanPhase - prevPhase;

    // Validation instrumentation
    VALIDATION_INIT(16);  // Effect ID 16
    VALIDATION_PHASE(m_scanPhase, phaseDelta);
    VALIDATION_SPEED(rawSpeedScale, m_speedScaleSmooth);
    VALIDATION_AUDIO(m_dominantBinSmooth, m_energyAvgSmooth, m_energyDeltaSmooth);
    VALIDATION_REVERSAL_CHECK(m_prevPhaseDelta, phaseDelta);
    VALIDATION_SUBMIT(::lightwaveos::validation::g_validationRing);
    m_prevPhaseDelta = phaseDelta;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float dist = (float)centerPairDistance((uint16_t)i);

        // JOG-DIAL FIX: Single wave for clean propagation (beat patterns cause jog-dial effect)
        // sin(k*dist - phase) produces OUTWARD motion when phase increases
        const float freqBase = 0.25f;  // Moderate spatial frequency (wavelength ~25 LEDs)
        float wave1 = sinf(dist * freqBase - m_scanPhase);

        // JOG-DIAL FIX: Audio modulates BRIGHTNESS, not speed - energyDelta drives intensity bursts
#if FEATURE_AUDIO_SYNC
        float fastFlux = hasAudio ? ctx.audio.fastFlux() : 0.0f;
#else
        float fastFlux = 0.0f;
#endif
        float audioGain = 0.4f + 0.5f * m_energyAvgSmooth + 0.5f * m_energyDeltaSmooth + 0.3f * fastFlux;
        float pattern = wave1 * audioGain;

        // CRITICAL: Use tanhf for uniform brightness (like ChevronWaves)
        pattern = tanhf(pattern * 2.0f) * 0.5f + 0.5f;

        uint8_t brightness = (uint8_t)(pattern * 255.0f * intensityNorm);
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
