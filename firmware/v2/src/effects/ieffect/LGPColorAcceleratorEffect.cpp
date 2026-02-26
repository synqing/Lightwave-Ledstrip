/**
 * @file LGPColorAcceleratorEffect.cpp
 * @brief LGP Color Accelerator effect implementation
 */

#include "LGPColorAcceleratorEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kAccelScale = 1.0f;
constexpr float kDebrisRate = 8.0f;
constexpr uint8_t kTrailLength = 20;

const plugins::EffectParameter kParameters[] = {
    {"accel_scale", "Accel Scale", 0.5f, 2.0f, kAccelScale, plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"debris_rate", "Debris Rate", 2.0f, 16.0f, kDebrisRate, plugins::EffectParameterType::FLOAT, 0.5f, "wave", "x", false},
    {"trail_length", "Trail Length", 6.0f, 40.0f, kTrailLength, plugins::EffectParameterType::INT, 1.0f, "blend", "px", false},
};
}

LGPColorAcceleratorEffect::LGPColorAcceleratorEffect()
    : m_redParticle(0.0f)
    , m_blueParticle(STRIP_LENGTH - 1.0f)
    , m_collision(false)
    , m_debrisRadius(0.0f)
    , m_accelScale(kAccelScale)
    , m_debrisRate(kDebrisRate)
    , m_trailLength(kTrailLength)
{
}

bool LGPColorAcceleratorEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_redParticle = 0.0f;
    m_blueParticle = STRIP_LENGTH - 1.0f;
    m_collision = false;
    m_debrisRadius = 0.0f;
    m_accelScale = kAccelScale;
    m_debrisRate = kDebrisRate;
    m_trailLength = kTrailLength;
    return true;
}

void LGPColorAcceleratorEffect::render(plugins::EffectContext& ctx) {
    // RGB particles accelerate from edges and collide at center
    float speed = ctx.speed / 255.0f;
    float intensity = ctx.brightness / 255.0f;

    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    if (!m_collision) {
        const float accelBase = speed * 10.0f * m_accelScale;
        m_redParticle += accelBase * (1.0f + (m_redParticle / STRIP_LENGTH));
        m_blueParticle -= accelBase * (1.0f + ((STRIP_LENGTH - m_blueParticle) / STRIP_LENGTH));

        // Draw particle trails
        for (int t = 0; t < m_trailLength; t++) {
            int redPos = (int)m_redParticle - t;
            int bluePos = (int)m_blueParticle + t;

            if (redPos >= 0 && redPos < STRIP_LENGTH) {
                uint8_t trailBright = (uint8_t)((255 - t * 12) * intensity);
                ctx.leds[redPos] = CRGB(trailBright, 0, 0);
            }

            if (bluePos >= 0 && bluePos < STRIP_LENGTH) {
                uint8_t trailBright = (uint8_t)((255 - t * 12) * intensity);
                if (bluePos + STRIP_LENGTH < ctx.ledCount) {
                    ctx.leds[bluePos + STRIP_LENGTH] = CRGB(0, 0, trailBright);
                }
            }
        }

        // Check for collision
        if (m_redParticle >= CENTER_LEFT - 5 && m_blueParticle <= CENTER_LEFT + 5) {
            m_collision = true;
            m_debrisRadius = 0.0f;
        }
    } else {
        m_debrisRadius += speed * m_debrisRate;

        for (int i = 0; i < STRIP_LENGTH; i++) {
            float distFromCenter = (float)centerPairDistance((uint16_t)i);

            if (distFromCenter <= m_debrisRadius) {
                uint8_t debrisHue = random8();
                uint8_t debrisBright = (uint8_t)(255.0f * (1.0f - distFromCenter / m_debrisRadius) * intensity);

                uint8_t brightU8 = (uint8_t)((debrisBright * ctx.brightness) / 255);
                CRGB debrisColor = ctx.palette.getColor(debrisHue, brightU8);
                if (random8(2) == 0) {
                    ctx.leds[i] = debrisColor;
                } else if (i + STRIP_LENGTH < ctx.ledCount) {
                    ctx.leds[i + STRIP_LENGTH] = debrisColor;
                }
            }
        }

        if (m_debrisRadius > HALF_LENGTH) {
            m_collision = false;
            m_redParticle = 0.0f;
            m_blueParticle = STRIP_LENGTH - 1.0f;
        }
    }
}

void LGPColorAcceleratorEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPColorAcceleratorEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Color Accelerator",
        "Color cycling with momentum",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

uint8_t LGPColorAcceleratorEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPColorAcceleratorEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPColorAcceleratorEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "accel_scale") == 0) {
        m_accelScale = constrain(value, 0.5f, 2.0f);
        return true;
    }
    if (strcmp(name, "debris_rate") == 0) {
        m_debrisRate = constrain(value, 2.0f, 16.0f);
        return true;
    }
    if (strcmp(name, "trail_length") == 0) {
        m_trailLength = (uint8_t)constrain((int)lroundf(value), 6, 40);
        return true;
    }
    return false;
}

float LGPColorAcceleratorEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "accel_scale") == 0) return m_accelScale;
    if (strcmp(name, "debris_rate") == 0) return m_debrisRate;
    if (strcmp(name, "trail_length") == 0) return m_trailLength;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
