/**
 * @file GlitchModifier.cpp
 * @brief Controlled chaos modifier implementation
 */

#include "GlitchModifier.h"
#include <cstring>

#ifndef NATIVE_BUILD
#include <FastLED.h>
#else
#include "../../../test/unit/mocks/fastled_mock.h"
#endif

namespace lightwaveos {
namespace effects {
namespace modifiers {

GlitchModifier::GlitchModifier(GlitchMode mode, float intensity)
    : m_mode(mode)
    , m_intensity(intensity)
    , m_channelShift(3)
    , m_enabled(true)
    , m_seed(12345)
    , m_wasOnBeat(false)
{
    // Clamp intensity
    if (m_intensity < 0.0f) m_intensity = 0.0f;
    if (m_intensity > 1.0f) m_intensity = 1.0f;
}

bool GlitchModifier::init(const plugins::EffectContext& ctx) {
    // Initialize RNG seed from frame number
    m_seed = ctx.frameNumber ^ 0xDEADBEEF;
    m_wasOnBeat = false;
    return true;
}

void GlitchModifier::apply(plugins::EffectContext& ctx) {
    if (!m_enabled || m_intensity == 0.0f) return;

    // Apply glitch based on mode
    switch (m_mode) {
        case GlitchMode::PIXEL_FLIP:
            applyPixelFlip(ctx);
            break;

        case GlitchMode::CHANNEL_SHIFT:
            applyChannelShift(ctx);
            break;

        case GlitchMode::NOISE_BURST:
            applyNoiseBurst(ctx);
            break;

        case GlitchMode::BEAT_SYNC:
            applyBeatSync(ctx);
            break;
    }
}

void GlitchModifier::unapply() {
    // No cleanup needed
}

const ModifierMetadata& GlitchModifier::getMetadata() const {
    static ModifierMetadata metadata(
        "Glitch",
        "Controlled chaos effects (pixel flip, channel shift, noise, beat-sync)",
        ModifierType::GLITCH,
        1
    );
    return metadata;
}

bool GlitchModifier::setParameter(const char* name, float value) {
    if (strcmp(name, "mode") == 0) {
        setMode(static_cast<GlitchMode>(static_cast<uint8_t>(value)));
        return true;
    }
    if (strcmp(name, "intensity") == 0) {
        setIntensity(value);
        return true;
    }
    if (strcmp(name, "shift") == 0) {
        setChannelShift(static_cast<int8_t>(value));
        return true;
    }
    return false;
}

float GlitchModifier::getParameter(const char* name) const {
    if (strcmp(name, "mode") == 0) {
        return static_cast<float>(static_cast<uint8_t>(m_mode));
    }
    if (strcmp(name, "intensity") == 0) {
        return m_intensity;
    }
    if (strcmp(name, "shift") == 0) {
        return static_cast<float>(m_channelShift);
    }
    return 0.0f;
}

void GlitchModifier::setMode(GlitchMode mode) {
    m_mode = mode;
}

void GlitchModifier::setIntensity(float intensity) {
    m_intensity = intensity;
    if (m_intensity < 0.0f) m_intensity = 0.0f;
    if (m_intensity > 1.0f) m_intensity = 1.0f;
}

void GlitchModifier::setChannelShift(int8_t shift) {
    m_channelShift = shift;
}

uint32_t GlitchModifier::random() {
    // Xorshift32 PRNG (fast, deterministic)
    m_seed ^= m_seed << 13;
    m_seed ^= m_seed >> 17;
    m_seed ^= m_seed << 5;
    return m_seed;
}

void GlitchModifier::applyPixelFlip(plugins::EffectContext& ctx) {
    // Randomly invert pixels based on intensity
    uint32_t threshold = static_cast<uint32_t>(m_intensity * 4294967295.0f);

    for (uint16_t i = 0; i < ctx.ledCount; i++) {
        if (random() < threshold) {
            // Invert pixel color
            ctx.leds[i].r = 255 - ctx.leds[i].r;
            ctx.leds[i].g = 255 - ctx.leds[i].g;
            ctx.leds[i].b = 255 - ctx.leds[i].b;
        }
    }
}

void GlitchModifier::applyChannelShift(plugins::EffectContext& ctx) {
    // Shift RGB channels by different offsets
    if (ctx.ledCount < static_cast<uint16_t>(m_channelShift * 2)) return;

    // Create temporary copy of red channel
    uint8_t* redCopy = new uint8_t[ctx.ledCount];
    if (!redCopy) return;

    for (uint16_t i = 0; i < ctx.ledCount; i++) {
        redCopy[i] = ctx.leds[i].r;
    }

    // Shift channels
    for (uint16_t i = 0; i < ctx.ledCount; i++) {
        // Red channel: shift forward
        uint16_t redSrc = (i + m_channelShift) % ctx.ledCount;
        // Green channel: no shift
        // Blue channel: shift backward
        uint16_t blueSrc = (i + ctx.ledCount - m_channelShift) % ctx.ledCount;

        // Apply intensity-scaled shift
        float blend = m_intensity;
        ctx.leds[i].r = ctx.leds[i].r * (1.0f - blend) + redCopy[redSrc] * blend;
        ctx.leds[i].b = ctx.leds[i].b * (1.0f - blend) + ctx.leds[blueSrc].b * blend;
    }

    delete[] redCopy;
}

void GlitchModifier::applyNoiseBurst(plugins::EffectContext& ctx) {
    // Add random brightness noise
    uint32_t threshold = static_cast<uint32_t>(m_intensity * 4294967295.0f);

    for (uint16_t i = 0; i < ctx.ledCount; i++) {
        if (random() < threshold) {
            // Random brightness offset (-50% to +50%)
            int16_t noise = static_cast<int16_t>((random() % 256) - 128);

            // Apply to all channels
            ctx.leds[i].r = qadd8(ctx.leds[i].r, noise);
            ctx.leds[i].g = qadd8(ctx.leds[i].g, noise);
            ctx.leds[i].b = qadd8(ctx.leds[i].b, noise);
        }
    }
}

void GlitchModifier::applyBeatSync(plugins::EffectContext& ctx) {
#if FEATURE_AUDIO_SYNC
    // Trigger glitch on beat
    if (ctx.audio.available && ctx.audio.isOnBeat()) {
        // Beat detected - trigger glitch burst
        if (!m_wasOnBeat) {
            // Apply pixel flip with current intensity
            applyPixelFlip(ctx);
            m_wasOnBeat = true;
        }
    } else {
        m_wasOnBeat = false;
    }
#else
    // No audio - fall back to periodic glitch
    if ((ctx.frameNumber % 120) == 0) {
        applyPixelFlip(ctx);
    }
#endif
}

} // namespace modifiers
} // namespace effects
} // namespace lightwaveos
