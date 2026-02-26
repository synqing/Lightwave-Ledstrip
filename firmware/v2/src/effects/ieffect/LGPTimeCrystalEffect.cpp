/**
 * @file LGPTimeCrystalEffect.cpp
 * @brief LGP Time Crystal effect implementation
 */

#include "LGPTimeCrystalEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kPhase1Rate = 100.0f;
constexpr float kPhase2Rate = 161.8f;
constexpr float kPhase3Rate = 271.8f;
constexpr float kPaletteSpread = 20.0f;

const plugins::EffectParameter kParameters[] = {
    {"phase1_rate", "Phase1 Rate", 40.0f, 220.0f, kPhase1Rate, plugins::EffectParameterType::FLOAT, 1.0f, "timing", "", false},
    {"phase2_rate", "Phase2 Rate", 80.0f, 280.0f, kPhase2Rate, plugins::EffectParameterType::FLOAT, 1.0f, "timing", "", false},
    {"phase3_rate", "Phase3 Rate", 120.0f, 360.0f, kPhase3Rate, plugins::EffectParameterType::FLOAT, 1.0f, "timing", "", false},
    {"palette_spread", "Palette Spread", 4.0f, 64.0f, kPaletteSpread, plugins::EffectParameterType::FLOAT, 1.0f, "colour", "", true},
};
}

LGPTimeCrystalEffect::LGPTimeCrystalEffect()
    : m_phase1(0)
    , m_phase2(0)
    , m_phase3(0)
    , m_phase1Rate(kPhase1Rate)
    , m_phase2Rate(kPhase2Rate)
    , m_phase3Rate(kPhase3Rate)
    , m_paletteSpread(kPaletteSpread)
{
}

bool LGPTimeCrystalEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase1 = 0;
    m_phase2 = 0;
    m_phase3 = 0;
    m_phase1Rate = kPhase1Rate;
    m_phase2Rate = kPhase2Rate;
    m_phase3Rate = kPhase3Rate;
    m_paletteSpread = kPaletteSpread;
    return true;
}

void LGPTimeCrystalEffect::render(plugins::EffectContext& ctx) {
    // Perpetual motion patterns with non-repeating periods
    float speedNorm = ctx.speed / 50.0f;

    m_phase1 = (uint16_t)(m_phase1 + (uint16_t)(speedNorm * m_phase1Rate));
    m_phase2 = (uint16_t)(m_phase2 + (uint16_t)(speedNorm * m_phase2Rate));
    m_phase3 = (uint16_t)(m_phase3 + (uint16_t)(speedNorm * m_phase3Rate));

    uint8_t crystallinity = ctx.brightness;
    const uint8_t dimensions = 3;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i) / (float)HALF_LENGTH;

        float crystal = 0.0f;

        // Dimension 1
        crystal += sin16((uint16_t)(m_phase1 + i * 400)) / 32768.0f;

        // Dimension 2
        crystal += sin16((uint16_t)(m_phase2 + i * 650)) / 65536.0f;

        // Dimension 3
        crystal += sin16((uint16_t)(m_phase3 + i * 1050)) / 131072.0f;

        crystal = crystal / dimensions;
        uint8_t brightness = (uint8_t)(128 + (int8_t)(crystal * crystallinity));

        uint8_t paletteIndex = (uint8_t)(crystal * m_paletteSpread) + (uint8_t)(distFromCenter * m_paletteSpread);

        // Use bright color from palette instead of white (paletteIndex=0)
        if (fabsf(crystal) > 0.9f) {
            brightness = 240;  // Slightly below 255 to avoid white guardrail
            paletteIndex = (uint8_t)(fabsf(crystal) * 50.0f);  // Use crystal value for color
        }

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex), brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex + 85), brightness);
        }
    }
}

void LGPTimeCrystalEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPTimeCrystalEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Time Crystal",
        "Periodic structure in time",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPTimeCrystalEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPTimeCrystalEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPTimeCrystalEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "phase1_rate") == 0) {
        m_phase1Rate = constrain(value, 40.0f, 220.0f);
        return true;
    }
    if (strcmp(name, "phase2_rate") == 0) {
        m_phase2Rate = constrain(value, 80.0f, 280.0f);
        return true;
    }
    if (strcmp(name, "phase3_rate") == 0) {
        m_phase3Rate = constrain(value, 120.0f, 360.0f);
        return true;
    }
    if (strcmp(name, "palette_spread") == 0) {
        m_paletteSpread = constrain(value, 4.0f, 64.0f);
        return true;
    }
    return false;
}

float LGPTimeCrystalEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "phase1_rate") == 0) return m_phase1Rate;
    if (strcmp(name, "phase2_rate") == 0) return m_phase2Rate;
    if (strcmp(name, "phase3_rate") == 0) return m_phase3Rate;
    if (strcmp(name, "palette_spread") == 0) return m_paletteSpread;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
