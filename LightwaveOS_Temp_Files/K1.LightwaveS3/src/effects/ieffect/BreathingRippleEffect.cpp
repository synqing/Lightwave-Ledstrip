/**
 * @file BreathingRippleEffect.cpp
 * @brief Breathing Ripple effect implementation
 *
 * CENTER ORIGIN COMPLIANT: All ripples originate from LED 79/80 and propagate
 * outward to edges (0 and 159). The breathing oscillation occurs around an
 * expanding baseRadius, creating pulsing wavefronts.
 *
 * NO RAINBOWS: Uses palette-based coloring with musical hue selection.
 */

#include "BreathingRippleEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
#include <FastLED.h>
#include <algorithm>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

BreathingRippleEffect::BreathingRippleEffect()
    : m_spawnCooldown(0)
    , m_lastSpawnTime(0)
    , m_harmonicSmoothed(0.0f)
    , m_rhythmicSmoothed(0.0f)
    , m_lastHopSeq(0)
{
    // Initialize all ripples as inactive
    for (uint8_t i = 0; i < MAX_RIPPLES; i++) {
        m_ripples[i].active = false;
        m_ripples[i].baseRadius = 0.0f;
        m_ripples[i].breathBpm = 20;
        m_ripples[i].amplitude = 10;
        m_ripples[i].phaseOffset = 0;
        m_ripples[i].brightness = 0.0f;
        m_ripples[i].decay = 1.0f;
        m_ripples[i].hue = 0;
    }
    memset(m_radial, 0, sizeof(m_radial));
}

bool BreathingRippleEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;  // Unused in init

    // Reset all ripples
    for (uint8_t i = 0; i < MAX_RIPPLES; i++) {
        m_ripples[i].active = false;
        m_ripples[i].baseRadius = 0.0f;
        m_ripples[i].brightness = 0.0f;
    }

    // Reset spawn control
    m_spawnCooldown = 0;
    m_lastSpawnTime = 0;

    // Reset radial buffer
    memset(m_radial, 0, sizeof(m_radial));

    // Reset audio smoothing
    m_harmonicFollower.reset(0.0f);
    m_rhythmicFollower.reset(0.0f);
    m_harmonicSmoothed = 0.0f;
    m_rhythmicSmoothed = 0.0f;
    m_lastHopSeq = 0;

    return true;
}

void BreathingRippleEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // CENTER ORIGIN BREATHING RIPPLE
    //
    // Each ripple has:
    // - baseRadius: Slowly drifts outward (radial expansion)
    // - oscillation: beatsin16 adds +/- amplitude around baseRadius
    // - currentRadius = baseRadius + oscillation (breathing effect)
    //
    // The result: wavefronts that pulse in/out while propagating to edges.
    // =========================================================================

    float dt = ctx.getSafeDeltaSeconds();
    float moodNorm = ctx.getMoodNormalized();
    const bool hasAudio = ctx.audio.available;

    // Fade radial buffer for trails (controlled by fadeAmount)
    fadeToBlackBy(m_radial, HALF_LENGTH, ctx.fadeAmount);

    // =========================================================================
    // Audio Processing (Musical Saliency)
    // =========================================================================
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
        }

        // Smooth harmonic saliency (affects breathing amplitude)
        float targetHarmonic = ctx.audio.harmonicSaliency();
        m_harmonicSmoothed = m_harmonicFollower.updateWithMood(targetHarmonic, dt, moodNorm);

        // Smooth rhythmic saliency (affects breathing rate)
        float targetRhythmic = ctx.audio.rhythmicSaliency();
        m_rhythmicSmoothed = m_rhythmicFollower.updateWithMood(targetRhythmic, dt, moodNorm);
    }
#endif

    // =========================================================================
    // Spawn Control
    // =========================================================================
    if (m_spawnCooldown > 0) {
        m_spawnCooldown--;
    }

    // Audio-triggered spawning
    bool shouldSpawn = false;
#if FEATURE_AUDIO_SYNC
    if (hasAudio && m_spawnCooldown == 0) {
        // Spawn on beat with high confidence
        if (ctx.audio.isOnBeat() && ctx.audio.tempoConfidence() > 0.4f) {
            shouldSpawn = true;
        }
        // Also spawn on significant harmonic changes
        else if (m_harmonicSmoothed > 0.6f && ctx.audio.isHarmonicDominant()) {
            shouldSpawn = true;
        }
        // Spawn on snare hits for punctuation
        else if (ctx.audio.isSnareHit()) {
            shouldSpawn = true;
        }
    }
#endif

    // Fallback spawning when no audio (periodic, slow)
    if (!hasAudio && m_spawnCooldown == 0) {
        // Spawn every ~1.5 seconds (time-based for FPS safety)
        static uint32_t lastAmbientSpawn = 0;
        if (ctx.totalTimeMs - lastAmbientSpawn > 1500) {
            shouldSpawn = true;
            lastAmbientSpawn = ctx.totalTimeMs;
        }
    }

    // Safety: if no active ripples, force a spawn after cooldown
    if (!shouldSpawn && m_spawnCooldown == 0) {
        bool anyActive = false;
        for (uint8_t i = 0; i < MAX_RIPPLES; i++) {
            if (m_ripples[i].active) {
                anyActive = true;
                break;
            }
        }
        if (!anyActive) {
            shouldSpawn = true;
        }
    }

    if (shouldSpawn) {
        spawnRipple(ctx);
    }

    // =========================================================================
    // Update Ripples (physics + decay)
    // =========================================================================
    updateRipples(ctx, dt);

    // =========================================================================
    // Render Ripples to Radial Buffer
    // =========================================================================
    renderRipples(ctx);

    // =========================================================================
    // Map Radial Buffer to LED Strips (CENTER ORIGIN)
    // =========================================================================
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        SET_CENTER_PAIR(ctx, dist, m_radial[dist]);
    }
}

void BreathingRippleEffect::spawnRipple(plugins::EffectContext& ctx) {
    const bool hasAudio = ctx.audio.available;

    for (uint8_t i = 0; i < MAX_RIPPLES; i++) {
        if (!m_ripples[i].active) {
            BreathingRipple& ripple = m_ripples[i];

            ripple.active = true;
            ripple.baseRadius = 3.0f;  // Start just outside center for visibility
            ripple.brightness = 1.0f;

            // Audio-adaptive parameters
            if (hasAudio) {
                // Breathing rate: 15-35 BPM, biased by rhythmic saliency
                // Higher rhythmic = faster breathing
                float rhythmBias = m_rhythmicSmoothed * 15.0f;
                ripple.breathBpm = std::min<uint8_t>(28, std::max<uint8_t>(12, (uint8_t)(15 + (uint8_t)rhythmBias + random8(8))));

                // Amplitude: 6-18 LEDs, boosted by harmonic saliency
                // Higher harmonic = wider oscillation
                float ampBias = m_harmonicSmoothed * 8.0f;
                ripple.amplitude = std::min<uint8_t>(14, std::max<uint8_t>(4, (uint8_t)(6 + (uint8_t)ampBias + random8(4))));

                // Decay: 0.6-1.2, faster when rhythmic is high
                ripple.decay = std::min<float>(1.0f, std::max<float>(0.55f, 0.6f + m_rhythmicSmoothed * 0.3f + (random8() / 512.0f)));

                // Hue: Based on chord root or beat phase
                if (ctx.audio.hasChord() && ctx.audio.chordConfidence() > 0.3f) {
                    // Map root note (0-11) to palette index
                    ripple.hue = ctx.audio.rootNote() * 21 + ctx.gHue;
                    // Warm shift for major, cool for minor
                    if (ctx.audio.isMajor()) {
                        ripple.hue += 20;
                    } else if (ctx.audio.isMinor()) {
                        ripple.hue -= 20;
                    }
                } else {
                    // Use beat phase for hue variation
                    ripple.hue = ctx.gHue + (uint8_t)(ctx.audio.beatPhase() * 64.0f);
                }
            } else {
                // Non-audio defaults
                ripple.breathBpm = std::min<uint8_t>(28, std::max<uint8_t>(12, (uint8_t)(18 + random8(10))));   // 18-28 BPM
                ripple.amplitude = std::min<uint8_t>(14, std::max<uint8_t>(4, (uint8_t)(8 + random8(6))));     // 8-14 LEDs
                ripple.decay = std::min<float>(1.0f, std::max<float>(0.55f, 0.7f + (random8() / 512.0f)));
                ripple.hue = ctx.gHue + random8(40);
            }

            // Random phase offset for visual variety
            ripple.phaseOffset = random16();

            // Set spawn cooldown (shorter when audio-reactive)
            m_spawnCooldown = hasAudio ? 4 : 8;
            m_lastSpawnTime = ctx.totalTimeMs;

            break;  // Only spawn one ripple per call
        }
    }
}

void BreathingRippleEffect::updateRipples(plugins::EffectContext& ctx, float dt) {
    // Speed scaling from user parameter (1.0 to 3.0x)
    const float speedScale = 1.0f + 1.5f * (ctx.speed / 50.0f);

    for (uint8_t i = 0; i < MAX_RIPPLES; i++) {
        if (!m_ripples[i].active) continue;

        BreathingRipple& ripple = m_ripples[i];

        // =====================================================================
        // Base radius drifts outward slowly (radial expansion)
        // Speed: 8-12 LEDs/second, scaled by user speed parameter
        // =====================================================================
        float driftSpeed = 8.0f * speedScale;
        ripple.baseRadius += driftSpeed * dt;

        // =====================================================================
        // Brightness decays exponentially
        // =====================================================================
        ripple.brightness *= expf(-ripple.decay * dt);
        if (ripple.brightness < 0.0f) ripple.brightness = 0.0f;

        // =====================================================================
        // Deactivate when faded out or past edge
        // =====================================================================
        if (ripple.baseRadius > (HALF_LENGTH + ripple.amplitude) ||
            ripple.brightness < 0.02f) {
            ripple.active = false;
        }
    }
}

void BreathingRippleEffect::renderRipples(plugins::EffectContext& ctx) {
    for (uint8_t i = 0; i < MAX_RIPPLES; i++) {
        if (!m_ripples[i].active) continue;

        const BreathingRipple& ripple = m_ripples[i];

        // =====================================================================
        // CORE INNOVATION: Breathing radius calculation
        //
        // currentRadius = baseRadius + beatsin16(breathBpm, -amplitude, +amplitude)
        //
        // beatsin16 oscillates between -amplitude and +amplitude at breathBpm,
        // creating the "breathing" effect where the wavefront pulses in/out.
        // =====================================================================
        int16_t oscillation = beatsin16(
            ripple.breathBpm,
            -ripple.amplitude,
            ripple.amplitude,
            0,                    // Time offset (0 = use millis())
            ripple.phaseOffset    // Phase offset for variety
        );

        float currentRadius = ripple.baseRadius + (float)oscillation;

        // Clamp to valid range (0 to HALF_LENGTH)
        if (currentRadius < 0.0f) currentRadius = 0.0f;
        if (currentRadius > HALF_LENGTH) currentRadius = HALF_LENGTH;

        // =====================================================================
        // Render wavefront with soft falloff
        // Wavefront width: 4 LEDs for smooth appearance
        // =====================================================================
        constexpr float WAVEFRONT_WIDTH = 4.0f;

        for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
            float distFromWave = fabsf((float)dist - currentRadius);

            if (distFromWave < WAVEFRONT_WIDTH) {
                // Soft falloff: 1.0 at wavefront, 0.0 at edge
                float waveBrightness = ripple.brightness * (1.0f - distFromWave / WAVEFRONT_WIDTH);

                // Edge fade: dim as ripple approaches strip edge
                float edgeFade = 1.0f;
                if (ripple.baseRadius > HALF_LENGTH - 20) {
                    edgeFade = (HALF_LENGTH - ripple.baseRadius) / 20.0f;
                    if (edgeFade < 0.0f) edgeFade = 0.0f;
                }
                waveBrightness *= edgeFade;

                // Calculate final brightness (0-255)
                uint8_t brightness = (uint8_t)(waveBrightness * 255.0f);
                if (brightness < 2) continue;

                // Slight hue shift across wavefront for depth
                uint8_t hueOffset = (uint8_t)(distFromWave * 3.0f);

                // Get color from palette
                CRGB color = ctx.palette.getColor(
                    ripple.hue + hueOffset,
                    brightness
                );

                // Additive blend into radial buffer
                m_radial[dist].r = qadd8(m_radial[dist].r, color.r);
                m_radial[dist].g = qadd8(m_radial[dist].g, color.g);
                m_radial[dist].b = qadd8(m_radial[dist].b, color.b);
            }
        }
    }
}

void BreathingRippleEffect::cleanup() {
    // No dynamic resources to free
}

const plugins::EffectMetadata& BreathingRippleEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Breathing Ripple",
        "Oscillating ripples that pulse while expanding from center",
        plugins::EffectCategory::WATER,
        1,
        nullptr  // No specific author
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
