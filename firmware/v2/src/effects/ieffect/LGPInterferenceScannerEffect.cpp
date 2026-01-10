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
    m_dominantBinSmooth = 0.0f;
    m_bassWavelength = 0.0f;
    m_trebleOverlay = 0.0f;

    // Initialize chromagram smoothing
    for (uint8_t i = 0; i < 12; i++) {
        m_chromaFollowers[i].reset(0.0f);
        m_chromaSmoothed[i] = 0.0f;
        m_chromaTargets[i] = 0.0f;
    }
    
    // Initialize smoothing followers
    m_bassFollower.reset(0.0f);
    m_trebleFollower.reset(0.0f);
    m_targetBass = 0.0f;
    m_targetTreble = 0.0f;
    
    // Initialize enhancement utilities
    m_speedSpring.init(50.0f, 1.0f);  // stiffness=50, mass=1 (critically damped)
    m_speedSpring.reset(1.0f);         // Start at base speed
    m_energyAvgFollower.reset(0.0f);
    m_energyDeltaFollower.reset(0.0f);
    return true;
}

void LGPInterferenceScannerEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN INTERFERENCE SCANNER - Creates scanning interference patterns
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;
    float complexityNorm = ctx.complexity / 255.0f;
    float variationNorm = ctx.variation / 255.0f;
    const bool hasAudio = ctx.audio.available;
    bool newHop = false;

#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;

            // LGP-SMOOTH: Use heavy_bands mid-frequency for smoother energy signal
            // This replaces raw chroma processing which is prone to spikes
            float heavyMid = ctx.audio.heavyMid();  // Accessor: (heavy_bands[2-4]) / 3.0f
            float energyNorm = heavyMid;
            if (energyNorm < 0.0f) energyNorm = 0.0f;
            if (energyNorm > 1.0f) energyNorm = 1.0f;

            // Update chromagram targets
            for (uint8_t i = 0; i < 12; i++) {
                m_chromaTargets[i] = ctx.audio.controlBus.heavy_chroma[i];
            }
            
            // Still track dominant chroma bin for color mapping (use smoothed values)
            float maxBinVal = 0.0f;
            uint8_t dominantBin = 0;
            for (uint8_t i = 0; i < 12; ++i) {
                float bin = m_chromaSmoothed[i];
                if (bin > maxBinVal) {
                    maxBinVal = bin;
                    dominantBin = i;
                }
            }

            // =================================================================
            // 64-bin Sub-Bass Wavelength Modulation (bins 0-5 = 110-155 Hz)
            // Deep bass widens the interference pattern - kick drums create
            // slow, majestic wave expansion instead of tight fringes.
            // =================================================================
            float bassSum = 0.0f;
            for (uint8_t i = 0; i < 6; ++i) {
                bassSum += ctx.audio.bin(i);
            }
            m_targetBass = bassSum / 6.0f;

            // =================================================================
            // 64-bin Treble Overlay (bins 48-63 = 1.3-4.2 kHz)
            // Hi-hat and cymbal energy adds high-frequency sparkle on top
            // of the interference pattern.
            // =================================================================
            float trebleSum = 0.0f;
            for (uint8_t i = 48; i < 64; ++i) {
                trebleSum += ctx.audio.bin(i);
            }
            m_targetTreble = trebleSum / 16.0f;

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
    }

    // Smooth bass and treble with AsymmetricFollower
    m_bassWavelength = m_bassFollower.updateWithMood(m_targetBass, dt, moodNorm);
    m_trebleOverlay = m_trebleFollower.updateWithMood(m_targetTreble, dt, moodNorm);

    // True exponential smoothing with AsymmetricFollower (frame-rate independent)
    float energyAvgSmooth = m_energyAvgFollower.updateWithMood(m_energyAvg, dt, moodNorm);
    float energyDeltaSmooth = m_energyDeltaFollower.updateWithMood(m_energyDelta, dt, moodNorm);

    // Dominant bin smoothing
    float alphaBin = 1.0f - expf(-dt / 0.25f);  // True exponential, 250ms time constant
    m_dominantBinSmooth += (m_dominantBin - m_dominantBinSmooth) * alphaBin;
    if (m_dominantBinSmooth < 0.0f) m_dominantBinSmooth = 0.0f;
    if (m_dominantBinSmooth > 11.0f) m_dominantBinSmooth = 11.0f;

    // Speed modulation with Spring physics (natural momentum, no jitter)
    // Target range: 0.6 to 1.4 (2.3x variation max, not 10x)
    float rawSpeedScale = 0.6f + 0.8f * energyAvgSmooth;  // Capture raw speed for validation
    float speedTarget = rawSpeedScale;
    if (speedTarget > 1.4f) speedTarget = 1.4f;

    // Spring physics for speed modulation (replaces linear slew limiting)
    float smoothedSpeed = m_speedSpring.update(speedTarget, dt);
    if (smoothedSpeed > 1.4f) smoothedSpeed = 1.4f;  // Hard clamp
    if (smoothedSpeed < 0.3f) smoothedSpeed = 0.3f;  // Prevent stalling

    // Capture phase before update for delta calculation
    float prevPhase = m_scanPhase;
    // FIX: Use 240.0f multiplier like ChevronWaves (was 3.6f via advancePhase - 67x slower!)
    // Fast phase accumulation makes forward motion perceptually dominant
    m_scanPhase += speedNorm * 240.0f * smoothedSpeed * dt;
    if (m_scanPhase > 628.3f) m_scanPhase -= 628.3f;  // Wrap at 100*2π to prevent float overflow
    float phaseDelta = m_scanPhase - prevPhase;

    // Validation instrumentation
    VALIDATION_INIT(16);  // Effect ID 16
    VALIDATION_PHASE(m_scanPhase, phaseDelta);
    VALIDATION_SPEED(rawSpeedScale, smoothedSpeed);
    VALIDATION_AUDIO(m_dominantBinSmooth, energyAvgSmooth, energyDeltaSmooth);
    VALIDATION_REVERSAL_CHECK(m_prevPhaseDelta, phaseDelta);
    VALIDATION_SUBMIT(::lightwaveos::validation::g_validationRing);
    m_prevPhaseDelta = phaseDelta;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float dist = (float)centerPairDistance((uint16_t)i);

        // INTERFERENCE PATTERN: Two wavelengths create moiré beating patterns
        // sin(k*dist - phase) produces OUTWARD motion when phase increases
        //
        // =====================================================================
        // 64-bin BASS WAVELENGTH MODULATION: Sub-bass (bins 0-5) widens pattern
        // On kick drum hits, the interference fringes expand majestically.
        // Bass energy reduces frequency → larger wavelength → wider pattern.
        // =====================================================================
        float freq1 = (0.16f + 0.08f * complexityNorm) - 0.05f * m_bassWavelength;
        float freq2 = (0.28f + 0.10f * complexityNorm) - 0.08f * m_bassWavelength;
        float wave1 = sinf(dist * freq1 - m_scanPhase);
        float wave2 = sinf(dist * freq2 - m_scanPhase * 1.2f);  // Slight phase offset
        float interference = wave1 + wave2 * 0.6f;  // Combine with weight for moiré

        // JOG-DIAL FIX: Audio modulates BRIGHTNESS, not speed - energyDelta drives intensity bursts
#if FEATURE_AUDIO_SYNC
        float fastFlux = hasAudio ? ctx.audio.fastFlux() : 0.0f;
#else
        float fastFlux = 0.0f;
#endif
        float audioGain = 0.4f + 0.5f * energyAvgSmooth + 0.5f * energyDeltaSmooth + 0.3f * fastFlux;

        // =====================================================================
        // 64-bin TREBLE SHIMMER: High frequencies (bins 48-63) add sparkle
        // Hi-hat and cymbal energy creates high-frequency brightness overlay.
        // =====================================================================
        if (m_trebleOverlay > 0.1f) {
            float shimmerFreq = 1.2f + variationNorm * 0.9f;
            float shimmer = m_trebleOverlay * sinf(dist * shimmerFreq + m_scanPhase * 4.0f);
            audioGain += shimmer * 0.35f;
        }

        // PERCUSSION BOOST: Add amplitude spike on snare hit for visual punch
#if FEATURE_AUDIO_SYNC
        if (hasAudio && ctx.audio.isSnareHit()) {
            audioGain += 0.8f;  // Burst on snare
        }
#endif
        audioGain = fminf(audioGain, 2.0f);  // Clamp max to prevent oversaturation

        // VISIBILITY FIX: Ensure minimum interference amplitude
        float interferenceAbs = fabsf(interference);
        if (interferenceAbs < 0.2f) {
            interference = (interference >= 0.0f) ? 0.2f : -0.2f;
        }

        float pattern = interference * audioGain;

        // CRITICAL: Use tanhf for uniform brightness (like ChevronWaves)
        pattern = tanhf(pattern * 2.0f) * 0.5f + 0.5f;

        // VISIBILITY FIX: Ensure minimum brightness floor
        pattern = fmaxf(0.2f, pattern);

        uint8_t brightness = (uint8_t)(pattern * 255.0f * intensityNorm);
        uint8_t paletteIndex = (uint8_t)(dist * 2.0f + pattern * 50.0f);
        uint8_t baseHue = (uint8_t)(ctx.gHue + (uint8_t)(m_dominantBinSmooth * (255.0f / 12.0f)));

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(baseHue + paletteIndex), brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            // FIX: Use SAME brightness for both strips (was inverted, causing visual backward motion)
            // Hue offset +90 matches ChevronWaves pattern (was +128)
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(baseHue + paletteIndex + 90),
                                                             brightness);
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
