/**
 * @file LGPBioluminescentWavesEffect.cpp
 * @brief LGP Bioluminescent Waves effect implementation
 */

#include "LGPBioluminescentWavesEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kPhaseRate = 1.0f;
constexpr uint8_t kSpawnInterval = 12;
constexpr float kGlowDecay = 0.941f;

const plugins::EffectParameter kParameters[] = {
    {"phase_rate", "Phase Rate", 0.4f, 2.0f, kPhaseRate, plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"spawn_interval", "Spawn Interval", 4.0f, 48.0f, kSpawnInterval, plugins::EffectParameterType::INT, 1.0f, "timing", "f", false},
    {"glow_decay", "Glow Decay", 0.85f, 0.99f, kGlowDecay, plugins::EffectParameterType::FLOAT, 0.005f, "damping", "", false},
};
}

LGPBioluminescentWavesEffect::LGPBioluminescentWavesEffect()
    : m_wavePhase(0)
    , m_glowPoints{}
    , m_glowLife{}
    , m_phaseRate(kPhaseRate)
    , m_spawnInterval(kSpawnInterval)
    , m_glowDecay(kGlowDecay)
{
}

bool LGPBioluminescentWavesEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_wavePhase = 0;
    memset(m_glowPoints, 0, sizeof(m_glowPoints));
    memset(m_glowLife, 0, sizeof(m_glowLife));
    m_phaseRate = kPhaseRate;
    m_spawnInterval = kSpawnInterval;
    m_glowDecay = kGlowDecay;
    return true;
}

void LGPBioluminescentWavesEffect::render(plugins::EffectContext& ctx) {
    // Fade to prevent color accumulation from additive blending
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // Ocean waves with glowing plankton effect
    const uint16_t phaseStep = (uint16_t)fmaxf(1.0f, ctx.speed * m_phaseRate);
    m_wavePhase = (uint16_t)(m_wavePhase + phaseStep);

    const uint8_t waveCount = 4;

    // Base ocean color
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint16_t wave = 0;
        for (uint8_t w = 0; w < waveCount; w++) {
            wave += sin8(((i << 2) + (m_wavePhase >> (4 - w))) >> w);
        }
        wave /= waveCount;

        uint8_t blue = scale8((uint8_t)wave, 60);
        uint8_t green = scale8((uint8_t)wave, 20);

        ctx.leds[i] = CRGB(0, green, blue);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = CRGB(0, green >> 1, blue);
        }
    }

    // Spawn new glow points occasionally
    if ((ctx.frameNumber % m_spawnInterval) == 0) {
        for (uint8_t g = 0; g < 20; g++) {
            if (m_glowLife[g] == 0) {
                m_glowPoints[g] = random8(STRIP_LENGTH);
                m_glowLife[g] = 255;
                break;
            }
        }
    }

    // Update and render glow points
    for (uint8_t g = 0; g < 20; g++) {
        if (m_glowLife[g] > 0) {
            m_glowLife[g] = scale8(m_glowLife[g], (uint8_t)(m_glowDecay * 255.0f));

            uint8_t pos = m_glowPoints[g];
            uint8_t intensity = scale8(m_glowLife[g], ctx.brightness);

            for (int8_t spread = -3; spread <= 3; spread++) {
                int16_t p = (int16_t)pos + spread;
                if (p >= 0 && p < STRIP_LENGTH) {
                    uint8_t spreadIntensity = scale8(intensity, (uint8_t)(255 - abs(spread) * 60));
                    ctx.leds[p] = CRGB(0, spreadIntensity >> 1, spreadIntensity);
                    if (p + STRIP_LENGTH < ctx.ledCount) {
                        uint16_t idx = p + STRIP_LENGTH;
                        ctx.leds[idx] = CRGB(0, spreadIntensity >> 2, spreadIntensity);
                    }
                }
            }
        }
    }
}

void LGPBioluminescentWavesEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPBioluminescentWavesEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Bioluminescent Waves",
        "Glowing plankton in waves",
        plugins::EffectCategory::NATURE,
        1
    };
    return meta;
}

uint8_t LGPBioluminescentWavesEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPBioluminescentWavesEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPBioluminescentWavesEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "phase_rate") == 0) {
        m_phaseRate = constrain(value, 0.4f, 2.0f);
        return true;
    }
    if (strcmp(name, "spawn_interval") == 0) {
        m_spawnInterval = (uint8_t)constrain((int)lroundf(value), 4, 48);
        return true;
    }
    if (strcmp(name, "glow_decay") == 0) {
        m_glowDecay = constrain(value, 0.85f, 0.99f);
        return true;
    }
    return false;
}

float LGPBioluminescentWavesEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "phase_rate") == 0) return m_phaseRate;
    if (strcmp(name, "spawn_interval") == 0) return m_spawnInterval;
    if (strcmp(name, "glow_decay") == 0) return m_glowDecay;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
