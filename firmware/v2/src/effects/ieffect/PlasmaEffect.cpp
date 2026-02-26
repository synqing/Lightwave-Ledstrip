/**
 * @file PlasmaEffect.cpp
 * @brief Plasma effect implementation
 */

#include "PlasmaEffect.h"
#include "../CoreEffects.h"
#include "../utils/FastLEDOptim.h"
#include "../../core/narrative/NarrativeEngine.h"
#include <FastLED.h>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:PlasmaEffect
namespace {
constexpr float kPlasmaEffectSpeedScale = 1.0f;
constexpr float kPlasmaEffectOutputGain = 1.0f;
constexpr float kPlasmaEffectCentreBias = 1.0f;

float gPlasmaEffectSpeedScale = kPlasmaEffectSpeedScale;
float gPlasmaEffectOutputGain = kPlasmaEffectOutputGain;
float gPlasmaEffectCentreBias = kPlasmaEffectCentreBias;

const lightwaveos::plugins::EffectParameter kPlasmaEffectParameters[] = {
    {"plasma_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kPlasmaEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"plasma_effect_output_gain", "Output Gain", 0.25f, 2.0f, kPlasmaEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"plasma_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kPlasmaEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:PlasmaEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {
PlasmaEffect::PlasmaEffect()
    : m_plasmaTime(0)
{
}

bool PlasmaEffect::init(plugins::EffectContext& ctx) {
    // AUTO_TUNABLES_BULK_RESET_BEGIN:PlasmaEffect
    gPlasmaEffectSpeedScale = kPlasmaEffectSpeedScale;
    gPlasmaEffectOutputGain = kPlasmaEffectOutputGain;
    gPlasmaEffectCentreBias = kPlasmaEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:PlasmaEffect


    m_plasmaTime = 0;
    return true;
}

void PlasmaEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN PLASMA - Plasma field from center
    // Narrative integration: speed modulated by tempo multiplier
    using namespace utils;
    using namespace lightwaveos::narrative;
    
    // Apply narrative tempo multiplier to speed (opt-in, backward compatible)
    float narrativeSpeed = ctx.speed;
    if (NARRATIVE.isEnabled()) {
        narrativeSpeed *= NARRATIVE.getTempoMultiplier();
    }
    
    m_plasmaTime += (uint16_t)narrativeSpeed;
    if (m_plasmaTime > 65535) m_plasmaTime = m_plasmaTime % 65536;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        // Use utility function for center-based sin16 calculations
        float v1 = fastled_center_sin16(i, CENTER_LEFT, HALF_LENGTH, 8.0f, m_plasmaTime);
        float v2 = fastled_center_sin16(i, CENTER_LEFT, HALF_LENGTH, 5.0f, (uint16_t)(-m_plasmaTime * 0.75f));
        float v3 = fastled_center_sin16(i, CENTER_LEFT, HALF_LENGTH, 3.0f, (uint16_t)(m_plasmaTime * 0.5f));

        uint8_t paletteIndex = (uint8_t)((v1 + v2 + v3) * 10.0f + 15.0f) + ctx.gHue;
        uint8_t brightness = (uint8_t)((v1 + v2 + 2.0f) * 63.75f);

        CRGB color = ctx.palette.getColor(paletteIndex, brightness);

        ctx.leds[i] = color;
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = color;
        }
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:PlasmaEffect
uint8_t PlasmaEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kPlasmaEffectParameters) / sizeof(kPlasmaEffectParameters[0]));
}

const plugins::EffectParameter* PlasmaEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kPlasmaEffectParameters[index];
}

bool PlasmaEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "plasma_effect_speed_scale") == 0) {
        gPlasmaEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "plasma_effect_output_gain") == 0) {
        gPlasmaEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "plasma_effect_centre_bias") == 0) {
        gPlasmaEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float PlasmaEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "plasma_effect_speed_scale") == 0) return gPlasmaEffectSpeedScale;
    if (strcmp(name, "plasma_effect_output_gain") == 0) return gPlasmaEffectOutputGain;
    if (strcmp(name, "plasma_effect_centre_bias") == 0) return gPlasmaEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:PlasmaEffect

void PlasmaEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& PlasmaEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Plasma",
        "Smoothly shifting color plasma",
        plugins::EffectCategory::AMBIENT,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

