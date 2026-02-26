/**
 * @file EsOctaveRefEffect.cpp
 * @brief ES v1.1 "Octave" reference show (chromagram strip).
 *
 * Per-zone PSRAM state and dt-corrected follower coefficients.
 * Rendering logic is unchanged from the original.
 */

#include "EsOctaveRefEffect.h"
#include "EsV11RefUtil.h"

#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:EsOctaveRefEffect
namespace {
constexpr float kEsOctaveRefEffectSpeedScale = 1.0f;
constexpr float kEsOctaveRefEffectOutputGain = 1.0f;
constexpr float kEsOctaveRefEffectCentreBias = 1.0f;

float gEsOctaveRefEffectSpeedScale = kEsOctaveRefEffectSpeedScale;
float gEsOctaveRefEffectOutputGain = kEsOctaveRefEffectOutputGain;
float gEsOctaveRefEffectCentreBias = kEsOctaveRefEffectCentreBias;

const lightwaveos::plugins::EffectParameter kEsOctaveRefEffectParameters[] = {
    {"es_octave_ref_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kEsOctaveRefEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"es_octave_ref_effect_output_gain", "Output Gain", 0.25f, 2.0f, kEsOctaveRefEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"es_octave_ref_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kEsOctaveRefEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:EsOctaveRefEffect

namespace lightwaveos::effects::ieffect::esv11_reference {

using namespace lightwaveos::effects;
using namespace lightwaveos::effects::ieffect::esv11ref;

bool EsOctaveRefEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:EsOctaveRefEffect
    gEsOctaveRefEffectSpeedScale = kEsOctaveRefEffectSpeedScale;
    gEsOctaveRefEffectOutputGain = kEsOctaveRefEffectOutputGain;
    gEsOctaveRefEffectCentreBias = kEsOctaveRefEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:EsOctaveRefEffect


    if (!m_ps) {
        m_ps = static_cast<PsramData*>(
            heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
        if (!m_ps) return false;
    }
    std::memset(m_ps, 0, sizeof(PsramData));

    for (uint8_t z = 0; z < kMaxZones; ++z) {
        m_ps->maxFollower[z] = 0.15f;
    }
    return true;
}

void EsOctaveRefEffect::render(plugins::EffectContext& ctx) {
    clearAll(ctx);

    if (!m_ps) return;

    if (!ctx.audio.available) {
        return;
    }

    const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;

    // Prefer raw ES chroma, fallback to contract chroma.
    const float* chroma = ctx.audio.controlBus.es_chroma_raw;
    float maxRaw = 0.0f;
    for (uint8_t i = 0; i < 12; ++i) {
        if (chroma[i] > maxRaw) maxRaw = chroma[i];
    }
    if (maxRaw < 0.0001f) {
        chroma = ctx.audio.controlBus.chroma;
    }

    const float dt = ctx.getSafeRawDeltaSeconds();
    const float smoothAlpha = 1.0f - expf(-dt / 0.060f);  // ~60 ms one-pole
    float frameMax = 0.0f;
    for (uint8_t i = 0; i < 12; ++i) {
        m_ps->chromaSmooth[z][i] += (clamp01(chroma[i]) - m_ps->chromaSmooth[z][i]) * smoothAlpha;
        if (m_ps->chromaSmooth[z][i] > frameMax) frameMax = m_ps->chromaSmooth[z][i];
    }

    // dt-corrected asymmetric follower (attack tau ~0.058 s, decay tau ~0.547 s)
    if (frameMax > m_ps->maxFollower[z]) {
        const float attackAlpha = 1.0f - expf(-dt / kMaxFollowerAttackTau);
        m_ps->maxFollower[z] += (frameMax - m_ps->maxFollower[z]) * attackAlpha;
    } else {
        const float decayAlpha = 1.0f - expf(-dt / kMaxFollowerDecayTau);
        m_ps->maxFollower[z] -= (m_ps->maxFollower[z] - frameMax) * decayAlpha;
    }
    if (m_ps->maxFollower[z] < 0.04f) m_ps->maxFollower[z] = 0.04f;
    const float invFollower = 1.0f / m_ps->maxFollower[z];

    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        float progress = (HALF_LENGTH <= 1) ? 0.0f : (static_cast<float>(dist) / static_cast<float>(HALF_LENGTH - 1));
        float mag = clamp01(interp12(m_ps->chromaSmooth[z], progress) * invFollower);
        const CRGB c = hsvProgress(ctx, progress, mag);
        SET_CENTER_PAIR(ctx, dist, c);
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:EsOctaveRefEffect
uint8_t EsOctaveRefEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kEsOctaveRefEffectParameters) / sizeof(kEsOctaveRefEffectParameters[0]));
}

const plugins::EffectParameter* EsOctaveRefEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kEsOctaveRefEffectParameters[index];
}

bool EsOctaveRefEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "es_octave_ref_effect_speed_scale") == 0) {
        gEsOctaveRefEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "es_octave_ref_effect_output_gain") == 0) {
        gEsOctaveRefEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "es_octave_ref_effect_centre_bias") == 0) {
        gEsOctaveRefEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float EsOctaveRefEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "es_octave_ref_effect_speed_scale") == 0) return gEsOctaveRefEffectSpeedScale;
    if (strcmp(name, "es_octave_ref_effect_output_gain") == 0) return gEsOctaveRefEffectOutputGain;
    if (strcmp(name, "es_octave_ref_effect_centre_bias") == 0) return gEsOctaveRefEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:EsOctaveRefEffect

void EsOctaveRefEffect::cleanup() {
    if (m_ps) { heap_caps_free(m_ps); m_ps = nullptr; }
}

const plugins::EffectMetadata& EsOctaveRefEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "ES Octave (Ref)",
        "ES v1.1 reference: chromagram strip (centre-origin mirror)",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

} // namespace lightwaveos::effects::ieffect::esv11_reference
