/**
 * @file JuggleEffect.cpp
 * @brief Juggle effect implementation
 */

#include "JuggleEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <math.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

JuggleEffect::JuggleEffect()
{
}

bool JuggleEffect::init(plugins::EffectContext& ctx) {
    return true;
}

void JuggleEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN JUGGLE - Multiple dots oscillate from center
    //
    // NOTE: dothue += 32 per dot (8 dots × 32 = 256 wrap) creates different colors per dot,
    // not rainbow cycling. Each dot uses a fixed hue per frame, not cycling through the wheel.
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    uint8_t dothue = ctx.gHue;
    const bool audioOk = ctx.audio.available;
    const bool tempoOk = audioOk && (ctx.audio.tempoConfidence() >= 0.25f);
    const bool useBins =
        audioOk &&
        !ctx.audio.trinityActive &&
        (ctx.audio.bins64Adaptive() != nullptr);
    const float beatPhase01 = tempoOk ? ctx.audio.beatPhase() : 0.0f;
    const bool beatTick = tempoOk && ctx.audio.isOnBeat();

    const float* bins = useBins ? ctx.audio.bins64Adaptive() : nullptr;

    for (int i = 0; i < 8; i++) {
        float mag01 = 0.0f;
        if (bins) {
            // Map 8 dots → 8 equal bin groups (8 bins each) from the 64-bin spectrum.
            const uint8_t start = static_cast<uint8_t>(i * 8);
            float sum = 0.0f;
            for (uint8_t j = 0; j < 8; j++) {
                sum += bins[start + j];
            }
            mag01 = sum * (1.0f / 8.0f);
        } else if (audioOk) {
            // Trinity / non-FFT fallback: use the coarse band energies.
            mag01 = ctx.audio.getBand(static_cast<uint8_t>(i));
        }

        if (mag01 < 0.0f) mag01 = 0.0f;
        if (mag01 > 1.0f) mag01 = 1.0f;

        // Audio semantic link (ES Spectrum reference):
        // magnitude drives "reach" from the centre, while beat phase drives motion cadence.
        const uint16_t maxDist = static_cast<uint16_t>(HALF_LENGTH * (0.10f + 0.90f * mag01));

        uint16_t distFromCenter = 0;
        if (tempoOk) {
            // Rate multiplier spreads dots across different "juggle" speeds while remaining beat-locked.
            const float rate = 1.0f + (0.125f * static_cast<float>(i));
            float p = (beatPhase01 * rate) + (static_cast<float>(i) * 0.07f);
            p -= floorf(p);  // Keep in [0,1)

            const uint16_t angle = static_cast<uint16_t>(p * 65535.0f);
            const int16_t s = sin16(angle);
            const uint16_t s01 = static_cast<uint16_t>(s + 32768);  // 0..65535
            distFromCenter = static_cast<uint16_t>((static_cast<uint32_t>(s01) * maxDist) / 65535U);
        } else {
            // Fallback: original deterministic juggle motion.
            distFromCenter = beatsin16(i + 7, 0, maxDist);
        }

        int pos1 = CENTER_RIGHT + distFromCenter;
        int pos2 = CENTER_LEFT - distFromCenter;

        // Respect active palette and brightness scaling
        uint8_t brightU8 = ctx.brightness;
        if (audioOk) {
            float brightScale = 0.30f + (0.70f * mag01);
            if (beatTick) {
                brightScale *= 1.15f;
            }
            float b = static_cast<float>(ctx.brightness) * brightScale;
            if (b > 255.0f) b = 255.0f;
            if (b < 0.0f) b = 0.0f;
            brightU8 = static_cast<uint8_t>(b);
        }
        CRGB color = ctx.palette.getColor(dothue, brightU8);

        if (pos1 < STRIP_LENGTH) {
            ctx.leds[pos1] = color;
            if (pos1 + STRIP_LENGTH < ctx.ledCount) {
                int mirrorPos1 = pos1 + STRIP_LENGTH;
                ctx.leds[mirrorPos1] = color;
            }
        }
        if (pos2 >= 0) {
            ctx.leds[pos2] = color;
            if (pos2 + STRIP_LENGTH < ctx.ledCount) {
                int mirrorPos2 = pos2 + STRIP_LENGTH;
                ctx.leds[mirrorPos2] = color;
            }
        }

        dothue += 32;
    }
}

void JuggleEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& JuggleEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Juggle",
        "Multiple colored balls juggling",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
