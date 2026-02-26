/**
 * @file OceanEffect.cpp
 * @brief Ocean effect implementation
 */

#include "OceanEffect.h"
#include "../CoreEffects.h"
#include "../../core/narrative/NarrativeEngine.h"
#include <FastLED.h>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:OceanEffect
namespace {
constexpr float kOceanEffectSpeedScale = 1.0f;
constexpr float kOceanEffectOutputGain = 1.0f;
constexpr float kOceanEffectCentreBias = 1.0f;

float gOceanEffectSpeedScale = kOceanEffectSpeedScale;
float gOceanEffectOutputGain = kOceanEffectOutputGain;
float gOceanEffectCentreBias = kOceanEffectCentreBias;

const lightwaveos::plugins::EffectParameter kOceanEffectParameters[] = {
    {"ocean_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kOceanEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"ocean_effect_output_gain", "Output Gain", 0.25f, 2.0f, kOceanEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"ocean_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kOceanEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:OceanEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {
OceanEffect::OceanEffect()
    : m_waterOffset(0)
{
}

bool OceanEffect::init(plugins::EffectContext& ctx) {
    // AUTO_TUNABLES_BULK_RESET_BEGIN:OceanEffect
    gOceanEffectSpeedScale = kOceanEffectSpeedScale;
    gOceanEffectOutputGain = kOceanEffectOutputGain;
    gOceanEffectCentreBias = kOceanEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:OceanEffect


    m_waterOffset = 0;
    return true;
}

void OceanEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN OCEAN - Waves emanate from center 79/80
    // Narrative integration: wave intensity modulated by narrative intensity
    using namespace lightwaveos::narrative;
    float dt = ctx.getSafeDeltaSeconds();

    m_waterOffset += (uint32_t)(ctx.speed / 2 * 60.0f * dt);  // dt-corrected

    if (m_waterOffset > 65535) m_waterOffset = m_waterOffset % 65536;
    
    // Get narrative intensity for wave modulation (opt-in, backward compatible)
    float narrativeIntensity = 1.0f;
    if (NARRATIVE.isEnabled()) {
        narrativeIntensity = NARRATIVE.getIntensity();
    }

    for (int i = 0; i < STRIP_LENGTH; i++) {
        // Calculate distance from CENTER PAIR
        float distFromCenter = (float)centerPairDistance((uint16_t)i);

        // Create wave motion from center outward
        uint8_t wave1 = sin8((uint16_t)(distFromCenter * 10) + m_waterOffset);
        uint8_t wave2 = sin8((uint16_t)(distFromCenter * 7) - m_waterOffset * 2);
        uint8_t combinedWave = (wave1 + wave2) / 2;
        
        // Apply narrative intensity modulation to wave amplitude
        combinedWave = (uint8_t)(combinedWave * narrativeIntensity);

        // Ocean colors from deep blue to cyan - use palette system
        uint8_t hue = 160 + (combinedWave >> 3);
        uint8_t brightness = 100 + (uint8_t)((combinedWave >> 1) * narrativeIntensity);
        uint8_t brightU8 = (uint8_t)((brightness * ctx.brightness) / 255);
        uint8_t paletteIdx = (uint8_t)(hue + ctx.gHue);
        CRGB color = ctx.palette.getColor(paletteIdx, brightU8);

        // Both strips
        ctx.leds[i] = color;
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = color;
        }
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:OceanEffect
uint8_t OceanEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kOceanEffectParameters) / sizeof(kOceanEffectParameters[0]));
}

const plugins::EffectParameter* OceanEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kOceanEffectParameters[index];
}

bool OceanEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "ocean_effect_speed_scale") == 0) {
        gOceanEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "ocean_effect_output_gain") == 0) {
        gOceanEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "ocean_effect_centre_bias") == 0) {
        gOceanEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float OceanEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "ocean_effect_speed_scale") == 0) return gOceanEffectSpeedScale;
    if (strcmp(name, "ocean_effect_output_gain") == 0) return gOceanEffectOutputGain;
    if (strcmp(name, "ocean_effect_centre_bias") == 0) return gOceanEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:OceanEffect

void OceanEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& OceanEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Ocean",
        "Deep ocean wave patterns from centre point",
        plugins::EffectCategory::WATER,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
