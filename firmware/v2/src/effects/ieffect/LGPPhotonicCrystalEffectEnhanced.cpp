/**
 * @file LGPPhotonicCrystalEffect.cpp
 * @brief LGP Photonic Crystal effect - v8 CORRECT audio-reactive motion
 *
 * v8: FIXED SPEED SMOOTHING ARCHITECTURE
 *
 * ROOT CAUSE of v7 jitter: Too many smoothing layers for speed
 *   v7 WRONG: heavyBass() → rolling avg → AsymmetricFollower → Spring (~630ms)
 *   v8 CORRECT: heavy_bands (pre-smoothed) → Spring ONLY (~200ms)
 *
 * The base algorithm is UNCHANGED from the original v1 lgpPhotonicCrystal():
 * - latticeSize = 4 + (complexity >> 6)  → 4-10 LEDs per cell
 * - defectProbability = variation        → random impurities
 * - inBandgap = cellPosition < (latticeSize >> 1)
 * - Allowed modes: sin8((distFromCenter << 2) - (phase >> 7))
 * - Forbidden decay: scale8(sin8(...), 255 - cellPosition * 50)
 *
 * Audio reactivity architecture (matches ChevronWaves/WaveCollision):
 * - Speed: heavy_bands[1]+[2] → Spring ONLY (0.6-1.4x, ~200ms response)
 * - Brightness: rolling avg + AsymmetricFollower (fine for visual intensity)
 * - Collision flash: snare-triggered, spatial decay from center
 * - Color offset: chroma dominant bin (smoothed over 250ms)
 */

#include "LGPPhotonicCrystalEffectEnhanced.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPPhotonicCrystalEnhancedEffect::LGPPhotonicCrystalEnhancedEffect()
    : m_phase(0.0f)
{
}

bool LGPPhotonicCrystalEnhancedEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Initialize phase
    m_phase = 0.0f;

    // Audio hop tracking
    m_lastHopSeq = 0;

    // Energy history (4-hop rolling average)
    for (uint8_t i = 0; i < ENERGY_HISTORY; ++i) {
        m_energyHist[i] = 0.0f;
    }
    m_energySum = 0.0f;
    m_energyHistIdx = 0;
    m_energyAvg = 0.0f;
    m_energyDelta = 0.0f;

    // Reset asymmetric followers
    m_energyAvgFollower.reset(0.5f);
    m_energyDeltaFollower.reset(0.0f);

    // Initialize spring with stiffness=50, critically damped
    m_speedSpring.init(50.0f, 1.0f);
    m_speedSpring.reset(1.0f);

    // Collision flash
    m_collisionBoost = 0.0f;
    
    // Enhanced: Initialize sub-bass tracking
    m_subBassFollower.reset(0.0f);
    m_subBassEnergy = 0.0f;
    m_targetSubBass = 0.0f;

    // Initialize chromagram smoothing
    for (uint8_t i = 0; i < 12; i++) {
        m_chromaFollowers[i].reset(0.0f);
        m_chromaSmoothed[i] = 0.0f;
        m_chromaTargets[i] = 0.0f;
    }
    
    // Chroma tracking
    m_dominantBin = 0;
    m_dominantBinSmooth = 0.0f;

    return true;
}

void LGPPhotonicCrystalEnhancedEffect::render(plugins::EffectContext& ctx) {
    // ========================================================================
    // SAFE DELTA TIME (clamped for physics stability)
    // ========================================================================
    float dt = ctx.getSafeDeltaSeconds();
    float moodNorm = ctx.getMoodNormalized();

    // ========================================================================
    // ORIGINAL V1 PARAMETERS (from ctx, NOT from audio)
    // These MUST stay as the original algorithm intended
    // ========================================================================
    uint8_t latticeSize = 4 + (ctx.complexity >> 6);  // 4-10 LEDs per cell
    uint8_t defectProbability = ctx.variation;         // Random impurities

    // Audio modulation values (defaults for no-audio mode)
    float speedMult = 1.0f;
    float brightnessGain = 1.0f;
    uint8_t chromaOffset = 0;

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        // Enhanced: SPEED uses 64-bin sub-bass blended with heavy_bands
        float heavyMid = (ctx.audio.controlBus.heavy_bands[1] +
                          ctx.audio.controlBus.heavy_bands[2]) / 2.0f;
        float heavyEnergy = m_subBassEnergy * 0.6f + heavyMid * 0.4f;  // Blend sub-bass with heavy_bands
        float targetSpeed = 0.6f + 0.8f * heavyEnergy;  // 0.6-1.4x range
        speedMult = m_speedSpring.update(targetSpeed, dt);
        if (speedMult > 1.6f) speedMult = 1.6f;
        if (speedMult < 0.3f) speedMult = 0.3f;

        // ================================================================
        // BRIGHTNESS: Per-hop sampling for energy baseline (separate from speed)
        // Rolling avg + AsymmetricFollower is fine for visual intensity
        // ================================================================
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;

            float currentEnergy = ctx.audio.heavyBass();

            // Rolling 4-hop average (for brightness baseline only)
            m_energySum -= m_energyHist[m_energyHistIdx];
            m_energyHist[m_energyHistIdx] = currentEnergy;
            m_energySum += currentEnergy;
            m_energyHistIdx = (m_energyHistIdx + 1) % ENERGY_HISTORY;
            m_energyAvg = m_energySum / ENERGY_HISTORY;

            // Delta = energy ABOVE average (positive only)
            m_energyDelta = currentEnergy - m_energyAvg;
            if (m_energyDelta < 0.0f) m_energyDelta = 0.0f;

            // Update chromagram targets
            for (uint8_t i = 0; i < 12; i++) {
                m_chromaTargets[i] = ctx.audio.controlBus.heavy_chroma[i];
            }
            
            // Enhanced: 64-bin Sub-Bass Detection (bins 0-5 = ~110-155 Hz)
            float subBassSum = 0.0f;
            for (uint8_t i = 0; i < 6; ++i) {
                subBassSum += ctx.audio.bin(i);
            }
            m_targetSubBass = subBassSum / 6.0f;
            
            // Dominant chroma bin detection (for color offset) - use smoothed values
            float maxChroma = 0.0f;
            for (uint8_t bin = 0; bin < 12; ++bin) {
                if (m_chromaSmoothed[bin] > maxChroma) {
                    maxChroma = m_chromaSmoothed[bin];
                    m_dominantBin = bin;
                }
            }
        }
        
        // Smooth chromagram with AsymmetricFollower (every frame)
        for (uint8_t i = 0; i < 12; i++) {
            m_chromaSmoothed[i] = m_chromaFollowers[i].updateWithMood(
                m_chromaTargets[i], dt, moodNorm);
        }
        
        // Enhanced: Smooth sub-bass energy
        m_subBassEnergy = m_subBassFollower.updateWithMood(m_targetSubBass, dt, moodNorm);

        // Asymmetric followers for BRIGHTNESS (not speed!)
        float energyAvgSmooth = m_energyAvgFollower.updateWithMood(m_energyAvg, dt, moodNorm);
        float energyDeltaSmooth = m_energyDeltaFollower.updateWithMood(m_energyDelta, dt, moodNorm);

        // Brightness modulation
        brightnessGain = 0.4f + 0.5f * energyAvgSmooth + 0.4f * energyDeltaSmooth;
        if (brightnessGain > 1.5f) brightnessGain = 1.5f;
        if (brightnessGain < 0.3f) brightnessGain = 0.3f;

        // Enhanced: Collision flash (snare-triggered with sub-bass boost)
        if (ctx.audio.isSnareHit()) {
            m_collisionBoost = 1.0f + m_subBassEnergy * 0.3f;  // Boost with sub-bass
        }
        if (m_collisionBoost > 1.3f) m_collisionBoost = 1.3f;  // Clamp
        m_collisionBoost *= 0.88f;

        // Chroma color offset (250ms smooth)
        float alphaBin = 1.0f - expf(-dt / 0.25f);
        m_dominantBinSmooth += ((float)m_dominantBin - m_dominantBinSmooth) * alphaBin;
        chromaOffset = (uint8_t)(m_dominantBinSmooth * (255.0f / 12.0f));
    }
#endif

    // Enhanced: Use beatPhase for synchronization when tempo confidence high
    // VISIBILITY FIX: Lower threshold from 0.6f to 0.2f for ambient/sparse music
    float speedNorm = ctx.speed / 50.0f;
    float tempoConf = ctx.audio.available ? ctx.audio.tempoConfidence() : 0.0f;
    if (ctx.audio.available && tempoConf > 0.2f) {
        float beatPhase = ctx.audio.beatPhase();
        m_phase = beatPhase * 628.3f;  // Map 0-1 to 0-100*2π
    } else {
        m_phase += speedNorm * 240.0f * speedMult * dt;
        if (m_phase > 628.3f) m_phase -= 628.3f;  // Wrap at ~2*PI*100
    }

    // Convert to integer phase for sin8 compatibility
    uint16_t phaseInt = (uint16_t)(m_phase * 0.408f);  // Scale to 0-256

    // ========================================================================
    // RENDER LOOP: ORIGINAL V1 ALGORITHM with audio layering
    // ========================================================================
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // CENTER ORIGIN: distance from center (79/80)
        uint16_t distFromCenter = centerPairDistance(i);

        // ================================================================
        // ORIGINAL V1: Periodic structure - bandgap simulation
        // ================================================================
        uint8_t cellPosition = distFromCenter % latticeSize;
        bool inBandgap = cellPosition < (latticeSize >> 1);

        // ================================================================
        // ORIGINAL V1: Random defects (photonic impurities)
        // ================================================================
        if (random8() < defectProbability) {
            inBandgap = !inBandgap;
        }

        // ================================================================
        // ORIGINAL V1: Photonic band structure
        // ================================================================
        uint8_t brightness = 0;
        if (inBandgap) {
            // Allowed modes - outward from center
            brightness = sin8((distFromCenter << 2) - (phaseInt >> 7));
        } else {
            // Forbidden gap - evanescent decay
            uint8_t decay = 255 - (cellPosition * 50);
            brightness = scale8(sin8((distFromCenter << 1) - (phaseInt >> 8)), decay);
        }

        // ================================================================
        // AUDIO LAYER: Apply brightness gain with minimum floor
        // VISIBILITY FIX: Ensure base brightness even with ambient/sparse music
        // ================================================================
        float brightnessFloat = (float)brightness * brightnessGain;
        brightnessFloat = fmaxf(0.2f * 255.0f, brightnessFloat);  // 20% minimum
        brightness = scale8((uint8_t)fminf(255.0f, brightnessFloat), ctx.brightness);

        // ================================================================
        // AUDIO LAYER: Collision flash (spatial decay from center)
        // exp(-dist * 0.12f) creates natural falloff
        // ================================================================
#if FEATURE_AUDIO_SYNC
        if (ctx.audio.available && m_collisionBoost > 0.01f) {
            float flash = m_collisionBoost * expf(-(float)distFromCenter * 0.12f);
            brightness = qadd8(brightness, (uint8_t)(flash * 60.0f));
        }
#endif

        // ================================================================
        // ORIGINAL V1: Color based on band structure
        // Allowed zones get gHue, forbidden zones get gHue + 128
        // AUDIO LAYER: Add chroma offset for pitch-based color variation
        // ================================================================
        uint8_t baseHue = inBandgap ? ctx.gHue : (uint8_t)(ctx.gHue + 128);
        baseHue += chromaOffset;
        uint8_t palettePos = baseHue + distFromCenter / 4;

        // Render to both strips (Strip 2 offset by 64 for complementary color)
        ctx.leds[i] = ctx.palette.getColor(palettePos, brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(
                (uint8_t)(palettePos + 64),
                brightness
            );
        }
    }
}

void LGPPhotonicCrystalEnhancedEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPPhotonicCrystalEnhancedEffect::getMetadata() const {
    static plugins::EffectMetadata meta(
        "LGP Photonic Crystal Enhanced",
        "Enhanced: heavy_chroma, 64-bin sub-bass, enhanced snare flash, beatPhase sync",
        plugins::EffectCategory::QUANTUM,
        1
    );
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
