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

#include "LGPPhotonicCrystalEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kPhaseRate = 240.0f;
constexpr float kSpeedFloor = 0.6f;
constexpr float kSpeedRange = 0.8f;
constexpr float kBrightnessBase = 0.4f;
constexpr float kBrightnessAvgGain = 0.5f;
constexpr float kBrightnessDeltaGain = 0.4f;
constexpr float kCollisionDecay = 0.88f;

const plugins::EffectParameter kParameters[] = {
    {"phase_rate", "Phase Rate", 120.0f, 320.0f, kPhaseRate, plugins::EffectParameterType::FLOAT, 1.0f, "timing", "", true},
    {"speed_floor", "Speed Floor", 0.2f, 1.2f, kSpeedFloor, plugins::EffectParameterType::FLOAT, 0.02f, "timing", "x", true},
    {"speed_range", "Speed Range", 0.2f, 1.4f, kSpeedRange, plugins::EffectParameterType::FLOAT, 0.02f, "timing", "x", true},
    {"brightness_base", "Brightness Base", 0.1f, 1.0f, kBrightnessBase, plugins::EffectParameterType::FLOAT, 0.02f, "blend", "x", true},
    {"brightness_avg_gain", "Brightness Avg Gain", 0.0f, 1.0f, kBrightnessAvgGain, plugins::EffectParameterType::FLOAT, 0.02f, "blend", "x", true},
    {"brightness_delta_gain", "Brightness Delta Gain", 0.0f, 1.2f, kBrightnessDeltaGain, plugins::EffectParameterType::FLOAT, 0.02f, "blend", "x", true},
    {"collision_decay", "Collision Decay", 0.70f, 0.99f, kCollisionDecay, plugins::EffectParameterType::FLOAT, 0.005f, "blend", "", true},
};
}

LGPPhotonicCrystalEffect::LGPPhotonicCrystalEffect()
    : m_phase(0.0f)
{
}

bool LGPPhotonicCrystalEffect::init(plugins::EffectContext& ctx) {
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
    m_lastFastFlux = 0.0f;

    // Chroma tracking
    m_chromaAngle = 0.0f;
    m_phaseRate = kPhaseRate;
    m_speedFloor = kSpeedFloor;
    m_speedRange = kSpeedRange;
    m_brightnessBase = kBrightnessBase;
    m_brightnessAvgGain = kBrightnessAvgGain;
    m_brightnessDeltaGain = kBrightnessDeltaGain;
    m_collisionDecay = kCollisionDecay;

    return true;
}

void LGPPhotonicCrystalEffect::render(plugins::EffectContext& ctx) {
    // ========================================================================
    // SAFE DELTA TIME (clamped for physics stability)
    // ========================================================================
    float rawDt = ctx.getSafeRawDeltaSeconds();
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
        // ================================================================
        // v8 FIX: SPEED uses heavy_bands DIRECTLY → Spring (NO extra smoothing!)
        // This matches ChevronWaves/WaveCollision/InterferenceScanner
        // heavy_bands are PRE-SMOOTHED by ControlBus (80ms rise / 15ms fall)
        // ================================================================
        float heavyEnergy = (ctx.audio.controlBus.heavy_bands[1] +
                             ctx.audio.controlBus.heavy_bands[2]) / 2.0f;
        float targetSpeed = m_speedFloor + m_speedRange * heavyEnergy;
        speedMult = m_speedSpring.update(targetSpeed, rawDt);
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

        }

        // Asymmetric followers for BRIGHTNESS (not speed!)
        float energyAvgSmooth = m_energyAvgFollower.updateWithMood(m_energyAvg, rawDt, moodNorm);
        float energyDeltaSmooth = m_energyDeltaFollower.updateWithMood(m_energyDelta, rawDt, moodNorm);

        // Brightness modulation
        brightnessGain = m_brightnessBase +
                         m_brightnessAvgGain * energyAvgSmooth +
                         m_brightnessDeltaGain * energyDeltaSmooth;
        if (brightnessGain > 1.5f) brightnessGain = 1.5f;
        if (brightnessGain < 0.3f) brightnessGain = 0.3f;

        // Collision flash:
        // - Primary: explicit snare trigger (legacy LWLS pipeline)
        // - Fallback: flux spikes with some treble/mid content (ES backend)
        bool collisionHit = ctx.audio.isSnareHit();
        float flux = ctx.audio.fastFlux();
        float fluxDelta = flux - m_lastFastFlux;
        m_lastFastFlux = flux;
        if (!collisionHit) {
            if (fluxDelta > 0.22f && flux > 0.25f) {
                // Require some high-frequency energy so bass drops do not spam flashes.
                if (ctx.audio.treble() > 0.20f || ctx.audio.mid() > 0.28f) {
                    collisionHit = true;
                }
            }
        }
        if (collisionHit) {
            m_collisionBoost = 1.0f;
        }
        m_collisionBoost = effects::chroma::dtDecay(m_collisionBoost, m_collisionDecay, rawDt);

        // Circular chroma hue (replaces argmax + linear EMA to eliminate bin-flip rainbow sweeps)
        chromaOffset = effects::chroma::circularChromaHueSmoothed(
            ctx.audio.controlBus.chroma, m_chromaAngle, rawDt, 0.20f);
    }
#endif

    // ========================================================================
    // PHASE ADVANCEMENT (proven formula from working effects)
    // m_phase += speedNorm * 240.0f * speedMult * dt
    // ========================================================================
    float speedNorm = ctx.speed / 50.0f;
    m_phase += speedNorm * m_phaseRate * speedMult * dt;
    if (m_phase > 628.3f) m_phase -= 628.3f;  // Wrap at ~2*PI*100

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
        // AUDIO LAYER: Apply brightness gain
        // ================================================================
        brightness = scale8(brightness, (uint8_t)(ctx.brightness * brightnessGain));

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

void LGPPhotonicCrystalEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPPhotonicCrystalEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Photonic Crystal",
        "v8: Fixed speed smoothing - heavy_bands direct to Spring (matches working effects)",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPPhotonicCrystalEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPPhotonicCrystalEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPPhotonicCrystalEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "phase_rate") == 0) {
        m_phaseRate = constrain(value, 120.0f, 320.0f);
        return true;
    }
    if (strcmp(name, "speed_floor") == 0) {
        m_speedFloor = constrain(value, 0.2f, 1.2f);
        return true;
    }
    if (strcmp(name, "speed_range") == 0) {
        m_speedRange = constrain(value, 0.2f, 1.4f);
        return true;
    }
    if (strcmp(name, "brightness_base") == 0) {
        m_brightnessBase = constrain(value, 0.1f, 1.0f);
        return true;
    }
    if (strcmp(name, "brightness_avg_gain") == 0) {
        m_brightnessAvgGain = constrain(value, 0.0f, 1.0f);
        return true;
    }
    if (strcmp(name, "brightness_delta_gain") == 0) {
        m_brightnessDeltaGain = constrain(value, 0.0f, 1.2f);
        return true;
    }
    if (strcmp(name, "collision_decay") == 0) {
        m_collisionDecay = constrain(value, 0.70f, 0.99f);
        return true;
    }
    return false;
}

float LGPPhotonicCrystalEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "phase_rate") == 0) return m_phaseRate;
    if (strcmp(name, "speed_floor") == 0) return m_speedFloor;
    if (strcmp(name, "speed_range") == 0) return m_speedRange;
    if (strcmp(name, "brightness_base") == 0) return m_brightnessBase;
    if (strcmp(name, "brightness_avg_gain") == 0) return m_brightnessAvgGain;
    if (strcmp(name, "brightness_delta_gain") == 0) return m_brightnessDeltaGain;
    if (strcmp(name, "collision_decay") == 0) return m_collisionDecay;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
