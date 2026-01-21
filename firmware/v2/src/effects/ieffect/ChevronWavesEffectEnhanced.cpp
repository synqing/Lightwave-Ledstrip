/**
 * @file ChevronWavesEffect.cpp
 * @brief LGP Chevron Waves implementation
 */

#include "ChevronWavesEffectEnhanced.h"
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

ChevronWavesEnhancedEffect::ChevronWavesEnhancedEffect()
    : m_chevronPos(0.0f)
{
}

bool ChevronWavesEnhancedEffect::init(plugins::EffectContext& ctx) {
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
    m_dominantBinSmooth = 0.0f;

    // Initialize chromagram smoothing
    for (uint8_t i = 0; i < 12; i++) {
        m_chromaFollowers[i].reset(0.0f);
        m_chromaSmoothed[i] = 0.0f;
        m_chromaTargets[i] = 0.0f;
    }
    
    // Initialize enhancement utilities
    m_phaseSpeedSpring.init(50.0f, 1.0f);  // stiffness=50, mass=1 (critically damped)
    m_phaseSpeedSpring.reset(1.0f);        // Start at base speed
    m_energyAvgFollower.reset(0.0f);
    m_energyDeltaFollower.reset(0.0f);
    m_subBassFollower.reset(0.0f);
    m_subBassEnergy = 0.0f;
    m_targetSubBass = 0.0f;
    m_snareSharpness = 0.0f;

    return true;
}

void ChevronWavesEnhancedEffect::render(plugins::EffectContext& ctx) {
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

    float dt = enhancement::getSafeDeltaSeconds(ctx.deltaTimeMs);
    float moodNorm = ctx.getMoodNormalized();

    // Smooth chromagram with AsymmetricFollower (every frame)
    if (hasAudio) {
        for (uint8_t i = 0; i < 12; i++) {
            m_chromaSmoothed[i] = m_chromaFollowers[i].updateWithMood(
                m_chromaTargets[i], dt, moodNorm);
        }
        // Enhanced: Smooth sub-bass energy
        m_subBassEnergy = m_subBassFollower.updateWithMood(m_targetSubBass, dt, moodNorm);
        
        // Enhanced: Snare hit triggers sharpness boost
        if (ctx.audio.isSnareHit()) {
            m_snareSharpness = 1.0f;
        }
        m_snareSharpness *= 0.90f;  // Decay
        if (m_snareSharpness < 0.01f) m_snareSharpness = 0.0f;
    }

    // True exponential smoothing with AsymmetricFollower (frame-rate independent)
    float energyAvgSmooth = m_energyAvgFollower.updateWithMood(m_energyAvg, dt, moodNorm);
    float energyDeltaSmooth = m_energyDeltaFollower.updateWithMood(m_energyDelta, dt, moodNorm);

    // Dominant bin smoothing
    float alphaBin = 1.0f - expf(-dt / 0.25f);  // True exponential, 250ms time constant
    m_dominantBinSmooth += (m_dominantBin - m_dominantBinSmooth) * alphaBin;
    if (m_dominantBinSmooth < 0.0f) m_dominantBinSmooth = 0.0f;
    if (m_dominantBinSmooth > 11.0f) m_dominantBinSmooth = 11.0f;

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
    float smoothedSpeed = m_phaseSpeedSpring.update(targetSpeed, dt);
    if (smoothedSpeed > 2.0f) smoothedSpeed = 2.0f;  // Hard clamp
    if (smoothedSpeed < 0.3f) smoothedSpeed = 0.3f;  // Prevent stalling
    
    // Enhanced: Use beatPhase for synchronization when tempo confidence high
    float tempoConf = hasAudio ? ctx.audio.tempoConfidence() : 0.0f;
    if (hasAudio && tempoConf > 0.6f) {
        float beatPhase = ctx.audio.beatPhase();
        m_chevronPos = beatPhase * 628.3f;  // Map 0-1 to 0-100*2π
    } else {
        m_chevronPos += speedNorm * 240.0f * smoothedSpeed * dt;
    }

    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    for (uint16_t i = 0; i < ctx.ledCount && i < STRIP_LENGTH; i++) {
        // CRITICAL FIX: Use centerPairDistance() like working effects
        float distFromCenter = (float)centerPairDistance(i);

        // CRITICAL FIX: Match reference pattern with moderate spatial frequency
        // sin(k*dist - phase) produces OUTWARD motion when phase increases
        const float freqBase = 0.25f;  // Wavelength ~25 LEDs (was ~7 with CHEVRON_COUNT)
        float chevron = sinf(distFromCenter * freqBase - m_chevronPos);

        // Enhanced: Sharp edges with snare-triggered crispness
        float tanhScale = 2.0f + m_snareSharpness * 3.0f;  // Base sharpness + snare boost
        chevron = tanhf(chevron * (tanhScale + 4.0f * energyAvgSmooth)) * 0.5f + 0.5f;

        float audioGain = 0.2f + 0.8f * energyAvgSmooth;
        uint8_t brightness = (uint8_t)(chevron * 255.0f * intensityNorm * audioGain);
        // Calculate hue with proper modular arithmetic (avoids UB from large float→uint8_t cast)
        // m_chevronPos grows unbounded; fmodf ensures values stay in [0, 256) before casting
        float rawHue = (float)ctx.gHue
                     + m_dominantBinSmooth * (255.0f / 12.0f)
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

void ChevronWavesEnhancedEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& ChevronWavesEnhancedEffect::getMetadata() const {
    static plugins::EffectMetadata meta(
        "LGP Chevron Waves Enhanced",
        "Enhanced: heavy_chroma, 64-bin sub-bass, snare sharpness boost, beatPhase sync",
        plugins::EffectCategory::GEOMETRIC,
        1
    );
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
