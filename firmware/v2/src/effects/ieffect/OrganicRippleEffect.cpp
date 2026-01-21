/**
 * @file OrganicRippleEffect.cpp
 * @brief Organic Ripple effect implementation
 *
 * CENTER ORIGIN RIPPLE with Perlin noise modulation:
 * - Ripples spawn at center (LED 79/80) and expand outward
 * - Perlin noise modulates velocity for organic speed variation
 * - Perlin noise modulates wavefront width for natural-feeling edges
 * - Audio-reactive spawning with musical saliency awareness
 *
 * AUDIO PROTOCOL COMPLIANCE:
 * - Checks rhythmicSaliency() before spawning on beats
 * - Uses smoothed bass energy, not raw instantaneous values
 * - Applies MOOD-adjusted smoothing for reactive vs smooth feel
 */

#include "OrganicRippleEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

OrganicRippleEffect::OrganicRippleEffect()
{
    // Initialize all ripples as inactive
    for (uint8_t i = 0; i < MAX_RIPPLES; i++) {
        m_ripples[i].active = false;
        m_ripples[i].radius = 0.0f;
        m_ripples[i].baseVelocity = 0.0f;
        m_ripples[i].brightness = 0.0f;
        m_ripples[i].decay = 0.0f;
        m_ripples[i].hue = 0;
        m_ripples[i].noiseOffset = 0;
    }
    memset(m_radial, 0, sizeof(m_radial));
}

bool OrganicRippleEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Reset all ripples
    for (uint8_t i = 0; i < MAX_RIPPLES; i++) {
        m_ripples[i].active = false;
        m_ripples[i].radius = 0.0f;
        m_ripples[i].brightness = 0.0f;
    }

    m_noiseTime = 0;
    m_spawnCooldown = 0;
    m_lastHopSeq = 0;
    memset(m_radial, 0, sizeof(m_radial));

    // Reset audio followers
    m_bassFollower.reset(0.0f);
    m_fluxFollower.reset(0.0f);
    m_targetBass = 0.0f;
    m_targetFlux = 0.0f;
    m_smoothBass = 0.0f;
    m_smoothFlux = 0.0f;

    return true;
}

void OrganicRippleEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // CENTER ORIGIN ORGANIC RIPPLE
    //
    // STATEFUL EFFECT: Maintains ripple state (position, velocity, brightness)
    // across frames. Ripples spawn at center and expand outward with Perlin-
    // modulated velocity for organic, natural-feeling motion.
    // =========================================================================

    const float dt = ctx.getSafeDeltaSeconds();
    const float moodNorm = ctx.getMoodNormalized();
    const bool hasAudio = ctx.audio.available;

    // Advance global noise time (speed-dependent)
    // Higher speed = faster noise evolution
    uint32_t noiseAdvance = (uint32_t)(30 + ctx.speed * 2);
    m_noiseTime += noiseAdvance;

    // =========================================================================
    // Audio Analysis (hop_seq checking for fresh data)
    // =========================================================================
    bool newHop = false;

#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;

            // Update targets on new audio hops
            m_targetBass = ctx.audio.bass();
            m_targetFlux = ctx.audio.flux();
        }

        // Smooth toward targets every frame with MOOD-adjusted smoothing
        m_smoothBass = m_bassFollower.updateWithMood(m_targetBass, dt, moodNorm);
        m_smoothFlux = m_fluxFollower.updateWithMood(m_targetFlux, dt, moodNorm);
    } else {
        // Decay audio smoothers when no audio
        m_targetBass = 0.0f;
        m_targetFlux = 0.0f;
        float decayAlpha = dt / (0.3f + dt);
        m_smoothBass += (0.0f - m_smoothBass) * decayAlpha;
        m_smoothFlux += (0.0f - m_smoothFlux) * decayAlpha;
    }
#endif

    // =========================================================================
    // Spawn Control (audio-reactive when available)
    // =========================================================================
    if (m_spawnCooldown > 0) {
        m_spawnCooldown--;
    }

#if FEATURE_AUDIO_SYNC
    if (hasAudio && m_spawnCooldown == 0) {
        // AUDIO PROTOCOL: Check rhythmic saliency before responding to beats
        // This ensures we respond to what's musically IMPORTANT, not all beats
        bool rhythmicallyImportant = ctx.audio.rhythmicSaliency() > 0.3f;

        // Beat-triggered spawn (strong beats only)
        if (ctx.audio.isOnBeat() && rhythmicallyImportant) {
            float intensity = 0.8f + m_smoothBass * 0.2f;
            spawnRipple(ctx, intensity);
            m_spawnCooldown = 3;  // Brief cooldown
        }
        // Downbeat spawn (always spawn on downbeats for musical anchoring)
        else if (ctx.audio.isOnDownbeat()) {
            spawnRipple(ctx, 1.0f);
            m_spawnCooldown = 4;
        }
        // Flux-triggered spawn (energy transients)
        else if (m_smoothFlux > 0.6f && newHop) {
            float intensity = 0.6f + m_smoothFlux * 0.4f;
            spawnRipple(ctx, intensity);
            m_spawnCooldown = 2;
        }
    }
#endif

    // Non-audio fallback: periodic spawning based on time
    if (!hasAudio && m_spawnCooldown == 0) {
        // Spawn roughly every 0.5-1.5 seconds depending on speed
        uint8_t spawnChance = (uint8_t)(5 + ctx.speed / 5);
        if (random8() < spawnChance) {
            spawnRipple(ctx, 0.7f + random8() / 510.0f);
            m_spawnCooldown = (uint8_t)(8 - ctx.speed / 10);
            if (m_spawnCooldown < 2) m_spawnCooldown = 2;
        }
    }

    // =========================================================================
    // Update Ripples (Perlin-modulated kinematics)
    // =========================================================================
    // Speed scaling based on ctx.speed (1-50 range)
    // speedScale: 0.5 (slow) to 2.5 (fast)
    const float speedScale = 0.5f + 2.0f * (ctx.speed / 50.0f);

    for (uint8_t r = 0; r < MAX_RIPPLES; r++) {
        Ripple& ripple = m_ripples[r];
        if (!ripple.active) continue;

        // =====================================================================
        // PERLIN VELOCITY MODULATION
        // Each ripple has a unique noiseOffset, creating organic speed variation
        // Velocity varies smoothly between 0.7x and 1.3x base velocity
        // =====================================================================
        uint16_t velocityNoise = inoise16(
            ripple.noiseOffset + (uint16_t)(ripple.radius * 100.0f),
            m_noiseTime >> 4
        );
        float velocityMod = 0.7f + 0.6f * (velocityNoise / 65535.0f);

        // Update radius with modulated velocity
        float actualVelocity = ripple.baseVelocity * velocityMod * speedScale;
        ripple.radius += actualVelocity * dt;

        // Exponential brightness decay
        ripple.brightness *= expf(-ripple.decay * dt);

        // Deactivate when ripple leaves visible range or fades out
        if (ripple.radius > (float)HALF_LENGTH || ripple.brightness < 0.02f) {
            ripple.active = false;
        }
    }

    // =========================================================================
    // Render Ripples to Radial Buffer
    // =========================================================================
    // Fade radial buffer using fadeAmount parameter
    fadeToBlackBy(m_radial, HALF_LENGTH, ctx.fadeAmount);

    for (uint8_t r = 0; r < MAX_RIPPLES; r++) {
        const Ripple& ripple = m_ripples[r];
        if (!ripple.active) continue;
        renderRipple(ctx, ripple);
    }

    // =========================================================================
    // Copy Radial Buffer to LED Output (CENTER PAIR pattern)
    // =========================================================================
    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        SET_CENTER_PAIR(ctx, dist, m_radial[dist]);
    }
}

void OrganicRippleEffect::spawnRipple(plugins::EffectContext& ctx, float intensity) {
    for (uint8_t r = 0; r < MAX_RIPPLES; r++) {
        Ripple& ripple = m_ripples[r];
        if (!ripple.active) {
            ripple.active = true;
            ripple.radius = 0.0f;  // Start at center

            // Base velocity: 25-45 LEDs/sec (adjusted by intensity)
            ripple.baseVelocity = 25.0f + intensity * 20.0f + random8(10);

            // Full brightness at spawn, scaled by intensity
            ripple.brightness = intensity;

            // Decay rate: 1.2-2.5 per second (faster decay for less intense ripples)
            ripple.decay = 1.2f + (1.0f - intensity) * 1.3f + random8() / 200.0f;

            // Hue from palette, with gHue drift and slight random variation
            ripple.hue = ctx.gHue + random8(30);

            // Unique noise seed for this ripple's organic motion
            ripple.noiseOffset = random16();

            break;  // Only spawn one ripple per call
        }
    }
}

void OrganicRippleEffect::renderRipple(plugins::EffectContext& ctx, const Ripple& ripple) {
    // =========================================================================
    // PERLIN-MODULATED WAVEFRONT RENDERING
    //
    // The wavefront width varies organically with Perlin noise, creating
    // natural-looking edges that breathe and shimmer. Width varies from
    // 2-5 LEDs based on noise sampling.
    // =========================================================================

    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        float distFromWave = fabsf((float)dist - ripple.radius);

        // Sample noise for wavefront width modulation (2-5 LEDs)
        uint16_t widthNoise = inoise16(
            ripple.noiseOffset + dist * 50,
            m_noiseTime >> 5
        );
        float width = 2.0f + 3.0f * (widthNoise / 65535.0f);

        if (distFromWave < width) {
            // Brightness falloff from wavefront center
            float waveBrightness = ripple.brightness * (1.0f - distFromWave / width);

            // Additional noise modulation for organic shimmer
            uint16_t shimmerNoise = inoise16(
                ripple.noiseOffset + dist * 120 + 5000,
                m_noiseTime >> 3
            );
            float shimmerMod = 0.85f + 0.15f * (shimmerNoise / 65535.0f);
            waveBrightness *= shimmerMod;

            uint8_t brightness = (uint8_t)(waveBrightness * 255.0f);

            // Get color from palette with hue offset based on distance
            // This creates subtle color variation across the wavefront
            uint8_t paletteIndex = ripple.hue + (uint8_t)(dist / 4);
            CRGB color = ctx.palette.getColor(paletteIndex, brightness);

            // Additive blend into radial buffer
            m_radial[dist].r = qadd8(m_radial[dist].r, color.r);
            m_radial[dist].g = qadd8(m_radial[dist].g, color.g);
            m_radial[dist].b = qadd8(m_radial[dist].b, color.b);
        }
    }
}

void OrganicRippleEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& OrganicRippleEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Organic Ripple",
        "Perlin-modulated ripples with natural speed variation",
        plugins::EffectCategory::WATER,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
