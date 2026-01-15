/**
 * @file BlurModifier.cpp
 * @brief Spatial smoothing modifier implementation
 */

#include "BlurModifier.h"
#include <cstring>

#ifndef NATIVE_BUILD
#include <FastLED.h>
#else
#include "../../../test/unit/mocks/fastled_mock.h"
#endif

namespace lightwaveos {
namespace effects {
namespace modifiers {

BlurModifier::BlurModifier(BlurMode mode, uint8_t radius, float strength)
    : m_mode(mode)
    , m_radius(radius > 5 ? 5 : (radius < 1 ? 1 : radius))
    , m_strength(strength < 0.0f ? 0.0f : (strength > 1.0f ? 1.0f : strength))
    , m_enabled(true)
{
}

bool BlurModifier::init(const plugins::EffectContext& ctx) {
    (void)ctx;
    return true;
}

void BlurModifier::apply(plugins::EffectContext& ctx) {
    if (!m_enabled || m_radius == 0 || m_strength <= 0.0f) return;

    switch (m_mode) {
        case BlurMode::BOX:
            applyBoxBlur(ctx);
            break;

        case BlurMode::GAUSSIAN:
            applyGaussianBlur(ctx);
            break;

        case BlurMode::MOTION:
            // Future: directional blur with velocity tracking
            applyBoxBlur(ctx);  // Fallback to box for now
            break;
    }
}

void BlurModifier::applyBoxBlur(plugins::EffectContext& ctx) {
    const uint16_t count = ctx.ledCount > MAX_LEDS ? MAX_LEDS : ctx.ledCount;

    // Copy original to temp buffer
    memcpy(m_tempBuffer, ctx.leds, count * sizeof(CRGB));

    // Box blur: average neighbors within radius
    for (uint16_t i = 0; i < count; i++) {
        uint16_t r = 0, g = 0, b = 0;
        uint8_t samples = 0;

        // Sample neighbors
        for (int8_t j = -static_cast<int8_t>(m_radius); j <= static_cast<int8_t>(m_radius); j++) {
            int16_t idx = static_cast<int16_t>(i) + j;
            if (idx >= 0 && idx < static_cast<int16_t>(count)) {
                r += m_tempBuffer[idx].r;
                g += m_tempBuffer[idx].g;
                b += m_tempBuffer[idx].b;
                samples++;
            }
        }

        // Calculate blurred color
        CRGB blurred(r / samples, g / samples, b / samples);

        // Blend original with blurred based on strength
        ctx.leds[i] = blend(m_tempBuffer[i], blurred, static_cast<uint8_t>(m_strength * 255.0f));
    }
}

void BlurModifier::applyGaussianBlur(plugins::EffectContext& ctx) {
    const uint16_t count = ctx.ledCount > MAX_LEDS ? MAX_LEDS : ctx.ledCount;

    // Copy original to temp buffer
    memcpy(m_tempBuffer, ctx.leds, count * sizeof(CRGB));

    // Gaussian kernel weights (5-tap: 1-4-6-4-1, sum=16)
    // We scale by radius to extend the kernel
    const int8_t weights[5] = {1, 4, 6, 4, 1};
    const uint8_t kernelSum = 16;

    for (uint16_t i = 0; i < count; i++) {
        uint16_t r = 0, g = 0, b = 0;
        uint8_t totalWeight = 0;

        // Apply gaussian kernel
        for (int8_t k = -2; k <= 2; k++) {
            // Scale kernel position by radius
            int16_t idx = static_cast<int16_t>(i) + (k * m_radius);
            if (idx >= 0 && idx < static_cast<int16_t>(count)) {
                uint8_t weight = weights[k + 2];
                r += m_tempBuffer[idx].r * weight;
                g += m_tempBuffer[idx].g * weight;
                b += m_tempBuffer[idx].b * weight;
                totalWeight += weight;
            }
        }

        // Normalize by actual weights used (handles edge pixels)
        if (totalWeight > 0) {
            CRGB blurred(r / totalWeight, g / totalWeight, b / totalWeight);
            ctx.leds[i] = blend(m_tempBuffer[i], blurred, static_cast<uint8_t>(m_strength * 255.0f));
        }
    }
}

void BlurModifier::unapply() {
    // No cleanup needed - temp buffer is stack allocated
}

const ModifierMetadata& BlurModifier::getMetadata() const {
    static ModifierMetadata metadata(
        "Blur",
        "Spatial smoothing for softer LED transitions",
        ModifierType::BLUR,
        1
    );
    return metadata;
}

bool BlurModifier::setParameter(const char* name, float value) {
    if (strcmp(name, "mode") == 0) {
        setMode(static_cast<BlurMode>(static_cast<uint8_t>(value)));
        return true;
    }
    if (strcmp(name, "radius") == 0) {
        setRadius(static_cast<uint8_t>(value));
        return true;
    }
    if (strcmp(name, "strength") == 0) {
        setStrength(value);
        return true;
    }
    return false;
}

float BlurModifier::getParameter(const char* name) const {
    if (strcmp(name, "mode") == 0) {
        return static_cast<float>(static_cast<uint8_t>(m_mode));
    }
    if (strcmp(name, "radius") == 0) {
        return static_cast<float>(m_radius);
    }
    if (strcmp(name, "strength") == 0) {
        return m_strength;
    }
    return 0.0f;
}

void BlurModifier::setMode(BlurMode mode) {
    m_mode = mode;
}

void BlurModifier::setRadius(uint8_t radius) {
    m_radius = radius > 5 ? 5 : (radius < 1 ? 1 : radius);
}

void BlurModifier::setStrength(float strength) {
    m_strength = strength < 0.0f ? 0.0f : (strength > 1.0f ? 1.0f : strength);
}

} // namespace modifiers
} // namespace effects
} // namespace lightwaveos
