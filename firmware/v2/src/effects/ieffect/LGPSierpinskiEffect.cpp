/**
 * @file LGPSierpinskiEffect.cpp
 * @brief LGP Sierpinski effect implementation
 */

#include "LGPSierpinskiEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kIterationStep = 0.25f;
constexpr int kMaxDepth = 5;
constexpr float kHueStep = 30.0f;

const plugins::EffectParameter kParameters[] = {
    {"iteration_step", "Iteration Step", 0.05f, 1.0f, kIterationStep, plugins::EffectParameterType::FLOAT, 0.01f, "timing", "x", false},
    {"max_depth", "Max Depth", 3.0f, 8.0f, (float)kMaxDepth, plugins::EffectParameterType::INT, 1.0f, "wave", "", false},
    {"hue_step", "Hue Step", 8.0f, 60.0f, kHueStep, plugins::EffectParameterType::FLOAT, 1.0f, "colour", "", false},
};
}

LGPSierpinskiEffect::LGPSierpinskiEffect()
    : m_iteration(0)
    , m_iterationStep(kIterationStep)
    , m_maxDepth(kMaxDepth)
    , m_hueStep(kHueStep)
{
}

bool LGPSierpinskiEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_iteration = 0;
    m_iterationStep = kIterationStep;
    m_maxDepth = kMaxDepth;
    m_hueStep = kHueStep;
    return true;
}

void LGPSierpinskiEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Fractal triangle patterns through recursive interference
    float intensityNorm = ctx.brightness / 255.0f;

    m_iteration = (uint16_t)(m_iteration + (uint16_t)(ctx.speed * m_iterationStep));

    for (int i = 0; i < STRIP_LENGTH; i++) {
        uint16_t x = centerPairDistance((uint16_t)i);
        uint16_t y = m_iteration >> 4;

        // XOR creates Sierpinski triangle
        uint16_t pattern = x ^ y;

        // Count bits for fractal depth
        uint8_t bitCount = 0;
        for (int d = 0; d < m_maxDepth; d++) {
            if (pattern & (1 << d)) bitCount++;
        }

        float smooth = sinf(bitCount * PI / (float)m_maxDepth);

        uint8_t brightness = (uint8_t)(smooth * 255.0f * intensityNorm);
        uint8_t hue = (uint8_t)(ctx.gHue + (uint8_t)(bitCount * m_hueStep));

        ctx.leds[i] = ctx.palette.getColor(hue, brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(hue + 128), brightness);
        }
    }
}

void LGPSierpinskiEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPSierpinskiEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Sierpinski",
        "Fractal triangle generation",
        plugins::EffectCategory::GEOMETRIC,
        1
    };
    return meta;
}

uint8_t LGPSierpinskiEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPSierpinskiEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPSierpinskiEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "iteration_step") == 0) {
        m_iterationStep = constrain(value, 0.05f, 1.0f);
        return true;
    }
    if (strcmp(name, "max_depth") == 0) {
        m_maxDepth = (int)constrain(value, 3.0f, 8.0f);
        return true;
    }
    if (strcmp(name, "hue_step") == 0) {
        m_hueStep = constrain(value, 8.0f, 60.0f);
        return true;
    }
    return false;
}

float LGPSierpinskiEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "iteration_step") == 0) return m_iterationStep;
    if (strcmp(name, "max_depth") == 0) return (float)m_maxDepth;
    if (strcmp(name, "hue_step") == 0) return m_hueStep;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
