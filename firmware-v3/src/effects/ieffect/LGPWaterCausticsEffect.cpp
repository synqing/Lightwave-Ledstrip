/**
 * @file LGPWaterCausticsEffect.cpp
 * @brief LGP Water Caustics effect implementation
 */

#include "LGPWaterCausticsEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPWaterCausticsEffect::LGPWaterCausticsEffect()
    : m_controls()
    , m_ar()
    , m_t1(0.0f)
    , m_t2(0.0f)
{
}

bool LGPWaterCausticsEffect::init(plugins::EffectContext& ctx) {
    m_controls.resetDefaults();
    m_ar = lowrisk_ar::ArRuntimeState{};
    m_ar.tonalHue = static_cast<float>(ctx.gHue);
    m_t1 = 0.0f;
    m_t2 = 0.0f;
    return true;
}

void LGPWaterCausticsEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN WATER CAUSTICS - Ray-envelope cusp filaments
    const float dtSignal = AudioReactivePolicy::signalDt(ctx);
    const float dtVisual = AudioReactivePolicy::visualDt(ctx);
    const float speedNorm = ctx.speed / 50.0f;
    const lowrisk_ar::ArSignalSnapshot sig = lowrisk_ar::updateSignals(m_ar, ctx, m_controls, dtSignal);
    const float master = (ctx.brightness / 255.0f) * lowrisk_ar::clamp01f(m_ar.audioPresence);
    const float audioMix = lowrisk_ar::clamp01f(m_controls.audioMix());
    const float motionRate = m_controls.motionRate();
    const float motionDepth = m_controls.motionDepth();
    const float tonalBase = lowrisk_ar::tonalBaseHue(ctx, m_controls, m_ar);

    const float t1Rate = (1.30f + 7.10f * speedNorm) * motionRate *
                         (0.65f + 0.35f * sig.mid + 0.18f * sig.rhythmic);
    const float t2Rate = (0.80f + 4.80f * speedNorm) * motionRate *
                         (0.70f + 0.25f * sig.bass + 0.18f * sig.flux);
    m_t1 += t1Rate * dtVisual;
    m_t2 += t2Rate * dtVisual;

    const float A = 0.75f + audioMix * motionDepth * (0.22f * sig.mid + 0.10f * m_ar.memory);
    const float B = 0.35f + audioMix * motionDepth * (0.18f * sig.bass + 0.16f * sig.flux);
    const float k1 = 0.18f * (1.0f + audioMix * 0.25f * sig.mid);
    const float k2 = 0.41f * (1.0f + audioMix * 0.22f * sig.flux);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float dist = (float)centerPairDistance((uint16_t)i);
        const float distN = dist / static_cast<float>(HALF_LENGTH);

        const float y =
            dist
            + A * sinf(dist * k1 + m_t1)
            + B * sinf(dist * k2 - m_t2 * 1.3f);

        const float dydx =
            1.0f
            + A * k1 * cosf(dist * k1 + m_t1)
            + B * k2 * cosf(dist * k2 - m_t2 * 1.3f);

        float density = 1.0f / (0.20f + fabsf(dydx));
        density = lowrisk_ar::clamp01f(density * 0.85f);

        const float sheet = 0.5f + 0.5f * sinf(y * 0.22f + m_t2);
        const float ambientWave = lowrisk_ar::clamp01f(0.72f * density + 0.28f * sheet);
        const float cusp = expf(-fabsf(density - 0.90f) * 14.0f) * sig.beat;
        const float reactiveWave = lowrisk_ar::clamp01f(
            ambientWave * (0.72f + 0.28f * (0.5f + 0.5f * sinf(m_t1 * 0.8f + distN * 9.0f))) +
            0.55f * cusp + 0.18f * m_ar.memory
        );
        const float wave = lowrisk_ar::blendAmbientReactive(ambientWave, reactiveWave, m_controls);

        const float base = 0.08f;
        // Apply continuous beat-strength brightness modulation (40% floor, +60% on beat)
        const float beatMod = 0.4f + 0.6f * sig.beat;
        const float out = lowrisk_ar::clamp01f(base + (1.0f - base) * wave) * master * beatMod;
        const uint8_t brA = (uint8_t)(255.0f * out);

        const float ambientHue = static_cast<float>(ctx.gHue) + y * 8.0f + density * 58.0f;
        const float anchorHue = tonalBase + distN * 20.0f + density * 32.0f;
        const float hueMix = audioMix * lowrisk_ar::clamp01f(m_controls.colourAnchorMix());
        const uint8_t hueA = static_cast<uint8_t>(ambientHue * (1.0f - hueMix) + anchorHue * hueMix);

        const uint8_t hueB = (uint8_t)(hueA + 4u + (uint8_t)(sig.harmonic * 6.0f));
        const uint8_t brB = scale8_video(brA, (uint8_t)(239 + (uint8_t)(sig.bass * 12.0f)));

        ctx.leds[i] = ctx.palette.getColor(hueA, brA);
        const int j = i + STRIP_LENGTH;
        if (j < ctx.ledCount) {
            ctx.leds[j] = ctx.palette.getColor(hueB, brB);
        }
    }
}

void LGPWaterCausticsEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPWaterCausticsEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Water Caustics",
        "Ray-envelope caustic filaments",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPWaterCausticsEffect::getParameterCount() const {
    return lowrisk_ar::Ar16Controls::parameterCount();
}

const plugins::EffectParameter* LGPWaterCausticsEffect::getParameter(uint8_t index) const {
    return lowrisk_ar::Ar16Controls::parameter(index);
}

bool LGPWaterCausticsEffect::setParameter(const char* name, float value) {
    return m_controls.set(name, value);
}

float LGPWaterCausticsEffect::getParameter(const char* name) const {
    return m_controls.get(name);
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
