/**
 * @file FireEffect.cpp
 * @brief Fire effect implementation
 */

#include "FireEffect.h"
#include "../CoreEffects.h"
#include "../../core/narrative/NarrativeEngine.h"
#include <FastLED.h>
#include <cstring>

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>


// AUTO_TUNABLES_BULK_BEGIN:FireEffect
namespace {
constexpr float kFireEffectSpeedScale = 1.0f;
constexpr float kFireEffectOutputGain = 1.0f;
constexpr float kFireEffectCentreBias = 1.0f;

float gFireEffectSpeedScale = kFireEffectSpeedScale;
float gFireEffectOutputGain = kFireEffectOutputGain;
float gFireEffectCentreBias = kFireEffectCentreBias;

const lightwaveos::plugins::EffectParameter kFireEffectParameters[] = {
    {"fire_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kFireEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"fire_effect_output_gain", "Output Gain", 0.25f, 2.0f, kFireEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"fire_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kFireEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:FireEffect
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {
FireEffect::FireEffect() {}

bool FireEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:FireEffect
    gFireEffectSpeedScale = kFireEffectSpeedScale;
    gFireEffectOutputGain = kFireEffectOutputGain;
    gFireEffectCentreBias = kFireEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:FireEffect


#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<FirePsram*>(
            heap_caps_malloc(sizeof(FirePsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) return false;
    }
    memset(m_ps, 0, sizeof(FirePsram));
#endif
    return true;
}

void FireEffect::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;
    using namespace lightwaveos::narrative;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        m_ps->fireHeat[i] = qsub8(m_ps->fireHeat[i], random8(0, ((55 * 10) / STRIP_LENGTH) + 2));
    }

    for (int k = 1; k < STRIP_LENGTH - 1; k++) {
        m_ps->fireHeat[k] = (m_ps->fireHeat[k - 1] + m_ps->fireHeat[k] + m_ps->fireHeat[k + 1]) / 3;
    }

    float narrativeTension = 1.0f;
    if (NARRATIVE.isEnabled()) {
        narrativeTension = NARRATIVE.getTension();
    }
    uint8_t sparkChance = (uint8_t)((80 + ctx.speed) * (0.5f + narrativeTension * 0.5f));
    if (random8() < sparkChance) {
        int center = CENTER_LEFT + random8(2);
        m_ps->fireHeat[center] = qadd8(m_ps->fireHeat[center], random8(160, 255));
    }

    for (int i = 0; i < STRIP_LENGTH; i++) {
        CRGB color = HeatColor(m_ps->fireHeat[i]);
        ctx.leds[i] = color;
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = color;
        }
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:FireEffect
uint8_t FireEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kFireEffectParameters) / sizeof(kFireEffectParameters[0]));
}

const plugins::EffectParameter* FireEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kFireEffectParameters[index];
}

bool FireEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "fire_effect_speed_scale") == 0) {
        gFireEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "fire_effect_output_gain") == 0) {
        gFireEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "fire_effect_centre_bias") == 0) {
        gFireEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float FireEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "fire_effect_speed_scale") == 0) return gFireEffectSpeedScale;
    if (strcmp(name, "fire_effect_output_gain") == 0) return gFireEffectOutputGain;
    if (strcmp(name, "fire_effect_centre_bias") == 0) return gFireEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:FireEffect

void FireEffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
}

const plugins::EffectMetadata& FireEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Fire",
        "Realistic fire simulation radiating from centre",
        plugins::EffectCategory::FIRE,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

