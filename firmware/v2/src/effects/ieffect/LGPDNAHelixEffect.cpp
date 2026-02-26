/**
 * @file LGPDNAHelixEffect.cpp
 * @brief LGP DNA Helix effect implementation
 */

#include "LGPDNAHelixEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kRotationRate = 0.05f;
constexpr float kHelixPitch = 20.0f;
constexpr float kLinkDensity = 1.0f;

const plugins::EffectParameter kParameters[] = {
    {"rotation_rate", "Rotation Rate", 0.01f, 0.20f, kRotationRate, plugins::EffectParameterType::FLOAT, 0.01f, "timing", "x", false},
    {"helix_pitch", "Helix Pitch", 10.0f, 40.0f, kHelixPitch, plugins::EffectParameterType::FLOAT, 1.0f, "wave", "px", false},
    {"link_density", "Link Density", 0.5f, 2.0f, kLinkDensity, plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
};
}

LGPDNAHelixEffect::LGPDNAHelixEffect()
    : m_rotation(0.0f)
    , m_rotationRate(kRotationRate)
    , m_helixPitch(kHelixPitch)
    , m_linkDensity(kLinkDensity)
{
}

bool LGPDNAHelixEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_rotation = 0.0f;
    m_rotationRate = kRotationRate;
    m_helixPitch = kHelixPitch;
    m_linkDensity = kLinkDensity;
    return true;
}

void LGPDNAHelixEffect::render(plugins::EffectContext& ctx) {
    // Double helix with color base pairing
    float speed = ctx.speed / 255.0f;
    float intensity = ctx.brightness / 255.0f;

    m_rotation += speed * m_rotationRate;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);

        float angle1 = (distFromCenter / m_helixPitch) * TWO_PI + m_rotation;
        float angle2 = angle1 + PI;

        uint8_t paletteOffset1;
        uint8_t paletteOffset2;
        if (sinf(angle1 * 2.0f) > 0.0f) {
            paletteOffset1 = 0;
            paletteOffset2 = 15;
        } else {
            paletteOffset1 = 10;
            paletteOffset2 = 25;
        }

        float strand1Intensity = (sinf(angle1) + 1.0f) * 0.5f;
        float strand2Intensity = (sinf(angle2) + 1.0f) * 0.5f;

        float connectionIntensity = 0.0f;
        if (fmodf(distFromCenter, (m_helixPitch / 4.0f) / m_linkDensity) < 2.0f) {
            connectionIntensity = 1.0f;
        }

        uint8_t brightness = (uint8_t)(255.0f * intensity);

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteOffset1),
                                           (uint8_t)(brightness * strand1Intensity));
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteOffset2),
                                                             (uint8_t)(brightness * strand2Intensity));
        }

        if (connectionIntensity > 0.0f) {
            CRGB conn2 = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteOffset2), brightness);
            CRGB conn1 = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteOffset1), brightness);
            ctx.leds[i] = blend(ctx.leds[i], conn2, 128);
            if (i + STRIP_LENGTH < ctx.ledCount) {
                ctx.leds[i + STRIP_LENGTH] = blend(ctx.leds[i + STRIP_LENGTH], conn1, 128);
            }
        }
    }
}

void LGPDNAHelixEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPDNAHelixEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP DNA Helix",
        "Double helix structure",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

uint8_t LGPDNAHelixEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPDNAHelixEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPDNAHelixEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "rotation_rate") == 0) {
        m_rotationRate = constrain(value, 0.01f, 0.20f);
        return true;
    }
    if (strcmp(name, "helix_pitch") == 0) {
        m_helixPitch = constrain(value, 10.0f, 40.0f);
        return true;
    }
    if (strcmp(name, "link_density") == 0) {
        m_linkDensity = constrain(value, 0.5f, 2.0f);
        return true;
    }
    return false;
}

float LGPDNAHelixEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "rotation_rate") == 0) return m_rotationRate;
    if (strcmp(name, "helix_pitch") == 0) return m_helixPitch;
    if (strcmp(name, "link_density") == 0) return m_linkDensity;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
