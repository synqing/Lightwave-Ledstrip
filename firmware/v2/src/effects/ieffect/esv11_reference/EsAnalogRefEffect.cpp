/**
 * @file EsAnalogRefEffect.cpp
 * @brief ES v1.1 "Analog" reference show (VU dot).
 *
 * Per-zone state: vuSmooth is indexed by ctx.zoneId to prevent
 * cross-zone contamination when ZoneComposer reuses this instance.
 * Rendering logic is unchanged from the original.
 */

#include "EsAnalogRefEffect.h"
#include "EsV11RefUtil.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif
#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:EsAnalogRefEffect
namespace {
constexpr float kEsAnalogRefEffectSpeedScale = 1.0f;
constexpr float kEsAnalogRefEffectOutputGain = 1.0f;
constexpr float kEsAnalogRefEffectCentreBias = 1.0f;

float gEsAnalogRefEffectSpeedScale = kEsAnalogRefEffectSpeedScale;
float gEsAnalogRefEffectOutputGain = kEsAnalogRefEffectOutputGain;
float gEsAnalogRefEffectCentreBias = kEsAnalogRefEffectCentreBias;

const lightwaveos::plugins::EffectParameter kEsAnalogRefEffectParameters[] = {
    {"es_analog_ref_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kEsAnalogRefEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"es_analog_ref_effect_output_gain", "Output Gain", 0.25f, 2.0f, kEsAnalogRefEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"es_analog_ref_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kEsAnalogRefEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:EsAnalogRefEffect

namespace lightwaveos::effects::ieffect::esv11_reference {

using namespace lightwaveos::effects;
using namespace lightwaveos::effects::ieffect::esv11ref;

bool EsAnalogRefEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:EsAnalogRefEffect
    gEsAnalogRefEffectSpeedScale = kEsAnalogRefEffectSpeedScale;
    gEsAnalogRefEffectOutputGain = kEsAnalogRefEffectOutputGain;
    gEsAnalogRefEffectCentreBias = kEsAnalogRefEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:EsAnalogRefEffect

    for (uint8_t z = 0; z < kMaxZones; ++z) {
        m_vuSmooth[z] = 0.000001f;
    }
    return true;
}

void EsAnalogRefEffect::render(plugins::EffectContext& ctx) {
    clearAll(ctx);

    const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;

    float vu = 0.0f;
    if (ctx.audio.available) {
        // Use raw ES VU level to preserve ES semantics for this reference show.
        vu = clamp01(ctx.audio.controlBus.es_vu_level_raw);
    }

    // Keep VU tracking tied to raw signal time (not SPEED-scaled effect time).
    const float dt = ctx.getSafeRawDeltaSeconds();
    const float alpha = 1.0f - expf(-dt / 0.090f);  // ~90 ms one-pole response
    m_vuSmooth[z] += (vu - m_vuSmooth[z]) * alpha;

    const float dotPos = clamp01(m_vuSmooth[z]);
    const float radius = dotPos * static_cast<float>(HALF_LENGTH - 1);
    const float speed01 = clamp01(ctx.speed / 100.0f);
    const float dotWidth = 1.5f + speed01 * 1.5f;  // SPEED only changes visual softness.

    // Draw a soft dot at the computed radius from centre.
    // ES uses a "dot" primitive with motion blur; this is a compact approximation.
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float d = std::fabs(static_cast<float>(dist) - radius);
        if (d > dotWidth) continue;
        const float w = 1.0f - (d / dotWidth);
        const CRGB c = hsvProgress(ctx, dotPos, w);
        SET_CENTER_PAIR(ctx, dist, c);
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:EsAnalogRefEffect
uint8_t EsAnalogRefEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kEsAnalogRefEffectParameters) / sizeof(kEsAnalogRefEffectParameters[0]));
}

const plugins::EffectParameter* EsAnalogRefEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kEsAnalogRefEffectParameters[index];
}

bool EsAnalogRefEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "es_analog_ref_effect_speed_scale") == 0) {
        gEsAnalogRefEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "es_analog_ref_effect_output_gain") == 0) {
        gEsAnalogRefEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "es_analog_ref_effect_centre_bias") == 0) {
        gEsAnalogRefEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float EsAnalogRefEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "es_analog_ref_effect_speed_scale") == 0) return gEsAnalogRefEffectSpeedScale;
    if (strcmp(name, "es_analog_ref_effect_output_gain") == 0) return gEsAnalogRefEffectOutputGain;
    if (strcmp(name, "es_analog_ref_effect_centre_bias") == 0) return gEsAnalogRefEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:EsAnalogRefEffect

void EsAnalogRefEffect::cleanup() {
    // No resources to free (DRAM member array, no PSRAM allocation).
}

const plugins::EffectMetadata& EsAnalogRefEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "ES Analog (Ref)",
        "ES v1.1 reference: VU dot (centre-origin)",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

} // namespace lightwaveos::effects::ieffect::esv11_reference
