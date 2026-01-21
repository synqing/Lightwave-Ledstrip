/**
 * @file NarrativePerlinEffect.cpp
 * @brief Narrative Perlin - Story conductor + organic Perlin noise texture
 *
 * Combines NarrativeArc state machine with Perlin noise fields for
 * dramatic, beat-responsive organic texture that builds and releases.
 *
 * Key behaviors:
 * - Beat triggers start new BUILD phase (when resting)
 * - Intensity controls noise complexity (octaves) and brightness
 * - CENTER ORIGIN: All patterns radiate from LED 79/80
 * - Strip 2 has +90 hue offset for LGP interference
 */

#include "NarrativePerlinEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

NarrativePerlinEffect::NarrativePerlinEffect()
    : m_arc()
    , m_lastBeat(false)
    , m_lastHopSeq(0)
    , m_noiseX(0)
    , m_noiseY(0)
    , m_noiseZ(0)
    , m_octaves(1)
    , m_amplitudeScale(0.0f)
    , m_rmsFollower(0.0f, 0.05f, 0.30f)
    , m_fluxFollower(0.0f, 0.08f, 0.25f)
    , m_targetRms(0.0f)
    , m_targetFlux(0.0f)
    , m_smoothRms(0.0f)
    , m_smoothFlux(0.0f)
    , m_energySpikeThreshold(0.8f)
    , m_allowEnergyTrigger(true)
{
    // Configure narrative arc durations
    // BUILD: 1.5s (tension building)
    // HOLD: 0.4s (peak moment)
    // RELEASE: 1.0s (graceful decay)
    // REST: 0.5s minimum (cooldown before next trigger)
    m_arc.setDurations(1.5f, 0.4f, 1.0f, 0.5f);
    m_arc.setBreathing(0.08f, 1.5f);  // Subtle 1.5Hz breathing at peak
}

bool NarrativePerlinEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Initialize noise field with random seeds
    m_noiseX = random16();
    m_noiseY = random16();
    m_noiseZ = random16();

    // Reset state
    m_arc.reset();
    m_lastBeat = false;
    m_lastHopSeq = 0;
    m_octaves = 1;
    m_amplitudeScale = 0.0f;

    // Reset audio smoothing
    m_rmsFollower.reset(0.0f);
    m_fluxFollower.reset(0.0f);
    m_targetRms = 0.0f;
    m_targetFlux = 0.0f;
    m_smoothRms = 0.0f;
    m_smoothFlux = 0.0f;

    return true;
}

void NarrativePerlinEffect::render(plugins::EffectContext& ctx) {
    const bool hasAudio = ctx.audio.available;
    float dt = ctx.getSafeDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.intensity / 255.0f;
    float complexityNorm = ctx.complexity / 255.0f;
    float variationNorm = ctx.variation / 255.0f;
    float moodNorm = ctx.getMoodNormalized();

    // =========================================================================
    // Audio Analysis & Beat Detection
    // =========================================================================
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        // Check for new audio hop (fresh data)
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
            m_targetRms = ctx.audio.rms();
            m_targetFlux = ctx.audio.flux();
        }

        // Smooth toward targets with MOOD-adjusted smoothing
        m_smoothRms = m_rmsFollower.updateWithMood(m_targetRms, dt, moodNorm);
        m_smoothFlux = m_fluxFollower.updateWithMood(m_targetFlux, dt, moodNorm);

        // Beat detection (rising edge only)
        bool currentBeat = ctx.audio.isOnBeat();
        bool beatRisingEdge = currentBeat && !m_lastBeat;
        m_lastBeat = currentBeat;

        // Trigger narrative arc on beat (if resting)
        if (beatRisingEdge && !m_arc.isActive()) {
            m_arc.trigger();
        }

        // Optional: Energy spike can also trigger (for sustained loud sections)
        if (m_allowEnergyTrigger && !m_arc.isActive()) {
            if (m_smoothRms > m_energySpikeThreshold && m_smoothFlux > 0.5f) {
                m_arc.trigger();
            }
        }
    } else {
        // No audio: auto-trigger periodically for ambient mode
        static uint32_t lastAutoTrigger = 0;
        if (ctx.totalTimeMs - lastAutoTrigger > 4000 && !m_arc.isActive()) {
            m_arc.trigger();
            lastAutoTrigger = ctx.totalTimeMs;
        }

        // Smooth audio values to zero
        float alpha = dt / (0.3f + dt);
        m_smoothRms += (0.0f - m_smoothRms) * alpha;
        m_smoothFlux += (0.0f - m_smoothFlux) * alpha;
    }
#else
    // No audio sync: auto-trigger periodically
    static uint32_t lastAutoTrigger = 0;
    if (ctx.totalTimeMs - lastAutoTrigger > 4000 && !m_arc.isActive()) {
        m_arc.trigger();
        lastAutoTrigger = ctx.totalTimeMs;
    }
#endif

    // =========================================================================
    // Update Narrative Arc
    // =========================================================================
    float narrativeIntensity = m_arc.update(dt);

    // Clamp intensity (can exceed 1.0 during HOLD breathing)
    if (narrativeIntensity < 0.0f) narrativeIntensity = 0.0f;
    if (narrativeIntensity > 1.2f) narrativeIntensity = 1.2f;

    // Scale intensity by global intensity parameter
    float effectiveIntensity = narrativeIntensity * intensityNorm;

    // =========================================================================
    // Modulate Perlin Parameters by Narrative Intensity
    // =========================================================================

    // Octaves: 1 at rest, 2 at mid-intensity, 3 at peak
    // More octaves = more detail/complexity
    m_octaves = 1 + (uint8_t)(effectiveIntensity * 2.0f);
    if (m_octaves > 3) m_octaves = 3;

    // Amplitude scale: controls brightness
    m_amplitudeScale = effectiveIntensity;

    // =========================================================================
    // Noise Field Advection (TIME-BASED to prevent jitter)
    // =========================================================================

    // Slow wobble for organic movement
    float angle = ctx.totalTimeMs * 0.001f;
    float wobbleX = sinf(angle * 0.11f) * 0.5f;
    float wobbleY = cosf(angle * 0.13f) * 0.5f;

    // Time-based advection (speedNorm controls drift rate)
    float baseDrift = speedNorm * 0.2f;  // calmer advection to reduce chaos
    uint16_t advX = (uint16_t)(10 + (uint16_t)(wobbleX * 6.0f) + (uint16_t)(baseDrift * 420.0f));
    uint16_t advY = (uint16_t)(8 + (uint16_t)(wobbleY * 5.0f) + (uint16_t)(baseDrift * 520.0f));
    uint16_t advZ = (uint16_t)(2 + (uint16_t)(baseDrift * 140.0f));

    // Scale drift by speed parameter
    uint16_t speedScale = (uint16_t)(2 + (uint16_t)(speedNorm * 10.0f));  // cap drift speed
    m_noiseX = (uint16_t)(m_noiseX + advX * speedScale);
    m_noiseY = (uint16_t)(m_noiseY + advY * speedScale);
    m_noiseZ = (uint16_t)(m_noiseZ + advZ * (uint16_t)(1 + (speedScale >> 2)));

    // =========================================================================
    // Rendering (CENTER ORIGIN pattern)
    // =========================================================================

    // Apply fade for trails (lower fade = longer trails)
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // If intensity is very low, just maintain the fade (no new pixels)
    if (effectiveIntensity < 0.02f) {
        return;
    }

    // Variation offset for palette diversity
    uint16_t variationOffset = (uint16_t)(ctx.variation * 197u);
    uint8_t paletteShift = (uint8_t)(variationNorm * 64.0f);

    // Detail scales with complexity
    uint16_t detail1 = (uint16_t)(18 + complexityNorm * 32.0f);
    uint16_t detail2 = (uint16_t)(32 + complexityNorm * 40.0f);

    // Brightness base scaled by narrative intensity
    uint8_t baseBrightness = (uint8_t)(effectiveIntensity * 220.0f);  // cap to avoid overexposure

    // -------------------------------------------------------------------------
    // Multi-octave Perlin noise rendering
    // -------------------------------------------------------------------------
    uint16_t x1 = (uint16_t)(m_noiseX + variationOffset);
    uint16_t x2 = (uint16_t)(m_noiseX + 8000u + (variationOffset >> 1));

    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        uint8_t dist8 = (uint8_t)dist;

        // Y coordinate varies with distance from center
        uint16_t y1 = (uint16_t)(m_noiseY + (variationOffset >> 2) + (uint16_t)(dist8 << 3));
        uint16_t y2 = (uint16_t)(m_noiseY + 4000u + (variationOffset >> 3) + (uint16_t)(dist8 << 4));

        // Z coordinate for 3D noise
        uint16_t z1 = (uint16_t)(m_noiseZ + (variationOffset >> 3));

        // ---------------------------------------------------------------------
        // Sample Perlin noise with variable octaves
        // More octaves = more detail at high intensity
        // ---------------------------------------------------------------------
        uint8_t hueNoise = 0;
        uint8_t lumNoise = 0;

        // First octave (always present)
        hueNoise = inoise8(x1, y1, z1);
        lumNoise = inoise8(x2, y2);

        // Second octave (when intensity > 0.33)
        if (m_octaves >= 2) {
            uint8_t hue2 = inoise8(x1 * 2, y1 * 2, z1 * 2);
            uint8_t lum2 = inoise8(x2 * 2, y2 * 2);
            hueNoise = (uint8_t)((hueNoise + hue2) >> 1);
            lumNoise = (uint8_t)((lumNoise + lum2) >> 1);
        }

        // Third octave (when intensity > 0.66)
        if (m_octaves >= 3) {
            uint8_t hue3 = inoise8(x1 * 4, y1 * 4, z1 * 4);
            uint8_t lum3 = inoise8(x2 * 4, y2 * 4);
            // Weight third octave less (finer detail)
            hueNoise = (uint8_t)((hueNoise * 3 + hue3) >> 2);
            lumNoise = (uint8_t)((lumNoise * 3 + lum3) >> 2);
        }

        // ---------------------------------------------------------------------
        // Color and brightness calculation
        // ---------------------------------------------------------------------

        // Palette index from hue noise + global hue cycling + variation
        uint8_t paletteIndex = hueNoise + ctx.gHue + paletteShift;

        // Luminance shaping: square for highlights, center emphasis
        uint8_t lum = scale8(lumNoise, lumNoise);  // Square for contrast

        // Center emphasis: brighter toward center
        uint8_t centerGain = (uint8_t)(255 - dist8 * 2);  // 255..97
        lum = scale8(lum, centerGain);

        // Apply narrative intensity to brightness
        uint8_t brightness = scale8(lum, baseBrightness);
        brightness = scale8(brightness, ctx.brightness);
        // Soft clamp to avoid chaotic flashing
        if (brightness > 220) brightness = 220;

        // Ensure minimum visibility when active
        if (brightness < 20 && effectiveIntensity > 0.1f) {
            brightness = 20;
        }

        // Get color from palette
        CRGB color1 = ctx.palette.getColor(paletteIndex, brightness);

        // ---------------------------------------------------------------------
        // Set LEDs with CENTER ORIGIN symmetry
        // ---------------------------------------------------------------------
        uint16_t left1 = CENTER_LEFT - dist;
        uint16_t right1 = CENTER_RIGHT + dist;

        // Strip 1
        if (left1 < ctx.ledCount) {
            ctx.leds[left1] = color1;
        }
        if (right1 < ctx.ledCount) {
            ctx.leds[right1] = color1;
        }

        // Strip 2: +90 hue offset (approx 1/3 of palette) for LGP interference
        // This creates complementary colors that interfere beautifully in the light guide
        uint8_t paletteIndex2 = (uint8_t)(paletteIndex + 90);  // +90 hue units
        CRGB color2 = ctx.palette.getColor(paletteIndex2, brightness);

        uint16_t left2 = left1 + STRIP_LENGTH;
        uint16_t right2 = right1 + STRIP_LENGTH;

        if (left2 < ctx.ledCount) {
            ctx.leds[left2] = color2;
        }
        if (right2 < ctx.ledCount) {
            ctx.leds[right2] = color2;
        }

        // Advance noise coordinates for next distance step
        x1 = (uint16_t)(x1 + detail1);
        x2 = (uint16_t)(x2 + detail2);
    }
}

void NarrativePerlinEffect::cleanup() {
    // No dynamic resources to free
}

const plugins::EffectMetadata& NarrativePerlinEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Narrative Perlin",
        "Beat-triggered organic noise with dramatic BUILD/HOLD/RELEASE arc",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
