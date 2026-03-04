/**
 * @file LGPSchlierenFlowEffect.cpp
 * @brief LGP Schlieren Flow effect implementation
 */

#include "LGPSchlierenFlowEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPSchlierenFlowEffect::LGPSchlierenFlowEffect()
    : m_controls()
    , m_ar()
    , m_t(0.0f)
{
}

bool LGPSchlierenFlowEffect::init(plugins::EffectContext& ctx) {
    m_controls.resetDefaults();
    m_ar = lowrisk_ar::ArRuntimeState{};
    m_ar.tonalHue = static_cast<float>(ctx.gHue);
    m_t = 0.0f;
    return true;
}

void LGPSchlierenFlowEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN SCHLIEREN - Knife-edge gradient flow
    const float dtSignal = AudioReactivePolicy::signalDt(ctx);
    const float dtVisual = AudioReactivePolicy::visualDt(ctx);
    const float speedNorm = ctx.speed / 50.0f;
    const lowrisk_ar::ArSignalSnapshot sig = lowrisk_ar::updateSignals(m_ar, ctx, m_controls, dtSignal);
    const float master = (ctx.brightness / 255.0f) * lowrisk_ar::clamp01f(m_ar.audioPresence);
    const float audioMix = lowrisk_ar::clamp01f(m_controls.audioMix());
    const float tonalBase = lowrisk_ar::tonalBaseHue(ctx, m_controls, m_ar);

    const float flowRate = (1.45f + 8.40f * speedNorm) * m_controls.motionRate() *
                           (0.65f + 0.35f * sig.bass + 0.22f * sig.rhythmic);
    m_t += flowRate * dtVisual;

    const float f1 = 0.060f * (1.0f + audioMix * 0.16f * sig.mid);
    const float f2 = 0.145f * (1.0f + audioMix * 0.14f * sig.treble);
    const float f3 = 0.310f * (1.0f + audioMix * 0.18f * sig.flux);
    const float edgeGain = 6.0f + audioMix * (2.5f * sig.treble + 2.2f * (ctx.audio.isHihatHit() ? 1.0f : 0.0f));
    const float turbulence = audioMix * m_controls.motionDepth() * (0.35f * sig.rhythmic + 0.28f * sig.flux + 0.22f * sig.beat);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        const float x = (float)i;
        const float dist = (float)centerPairDistance((uint16_t)i);
        const float distN = dist / static_cast<float>(HALF_LENGTH);

        float rho =
            sinf(x * f1 + m_t)
            + 0.7f * sinf(x * f2 - m_t * 1.2f)
            + 0.3f * sinf(x * f3 + m_t * 2.1f + turbulence * 2.0f);

        float grad =
            f1 * cosf(x * f1 + m_t)
            + 0.7f * f2 * cosf(x * f2 - m_t * 1.2f)
            + 0.3f * f3 * cosf(x * f3 + m_t * 2.1f);

        float edge = 0.5f + 0.5f * tanhf(grad * edgeGain);

        const float mid = (STRIP_LENGTH - 1) * 0.5f;
        const float dmid = (x - mid);
        const float melt = expf(-(dmid * dmid) * 0.0028f);

        const float ambientWave = lowrisk_ar::clamp01f(0.65f * (edge * melt + 0.25f * melt) + 0.35f * (0.5f + 0.5f * sinf(rho)));
        const float reactiveWave = lowrisk_ar::clamp01f(
            ambientWave * (0.70f + 0.30f * (0.45f + 0.55f * sig.mid)) +
            0.38f * sig.beat * edge + 0.14f * m_ar.memory
        );
        const float wave = lowrisk_ar::blendAmbientReactive(ambientWave, reactiveWave, m_controls);

        const float base = 0.08f;
        const float out = lowrisk_ar::clamp01f(base + (1.0f - base) * wave) * master;
        const uint8_t brA = (uint8_t)(255.0f * out);

        const float ambientHue = static_cast<float>(ctx.gHue) + dist * 0.7f + edge * 36.0f;
        const float anchorHue = tonalBase + distN * 18.0f + sig.harmonic * 32.0f;
        const float hueMix = audioMix * lowrisk_ar::clamp01f(m_controls.colourAnchorMix());
        const uint8_t hueA = static_cast<uint8_t>(ambientHue * (1.0f - hueMix) + anchorHue * hueMix);

        const uint8_t hueB = (uint8_t)(hueA + 5u + (uint8_t)(sig.rhythmic * 4.0f));
        const uint8_t brB = scale8_video(brA, (uint8_t)(240 + (uint8_t)(sig.treble * 10.0f)));

        ctx.leds[i] = ctx.palette.getColor(hueA, brA);
        const int j = i + STRIP_LENGTH;
        if (j < ctx.ledCount) {
            ctx.leds[j] = ctx.palette.getColor(hueB, brB);
        }
    }
}

void LGPSchlierenFlowEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPSchlierenFlowEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Schlieren Flow",
        "Knife-edge gradient flow",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPSchlierenFlowEffect::getParameterCount() const {
    return lowrisk_ar::Ar16Controls::parameterCount();
}

const plugins::EffectParameter* LGPSchlierenFlowEffect::getParameter(uint8_t index) const {
    return lowrisk_ar::Ar16Controls::parameter(index);
}

bool LGPSchlierenFlowEffect::setParameter(const char* name, float value) {
    return m_controls.set(name, value);
}

float LGPSchlierenFlowEffect::getParameter(const char* name) const {
    return m_controls.get(name);
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
