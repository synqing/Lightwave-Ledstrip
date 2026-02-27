/**
 * @file JuggleEffect.cpp
 * @brief Juggle effect implementation
 */

#include "JuggleEffect.h"
#include "ChromaUtils.h"
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
    (void)ctx;
    m_chromaAngle = 0.0f;
    return true;
}

void JuggleEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN JUGGLE - Multiple dots oscillate from center
    //
    // NOTE: dothue += 32 per dot (8 dots × 32 = 256 wrap) creates different colors per dot,
    // not rainbow cycling. Each dot uses a fixed hue per frame, not cycling through the wheel.
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    const bool audioOk = ctx.audio.available;
    const bool tempoOk = audioOk && (ctx.audio.tempoConfidence() >= 0.25f);
    const bool useBins =
        audioOk &&
        !ctx.audio.trinityActive &&
        (ctx.audio.bins64Adaptive() != nullptr);
    const float beatPhase01 = tempoOk ? ctx.audio.beatPhase() : 0.0f;
    const bool beatTick = tempoOk && ctx.audio.isOnBeat();
    const float beatStrength = tempoOk ? ctx.audio.beatStrength() : 0.0f;
    const float flux = audioOk ? ctx.audio.fastFlux() : 0.0f;

    const float* bins = useBins ? ctx.audio.bins64Adaptive() : nullptr;
    const float* chroma = audioOk ? ctx.audio.controlBus.chroma : nullptr;

    // Dynamic dot count: more energy → more juggling balls.
    uint8_t dotCount = 8;
    if (audioOk) {
        float rms = ctx.audio.rms();
        if (rms < 0.0f) rms = 0.0f;
        if (rms > 1.0f) rms = 1.0f;
        dotCount = static_cast<uint8_t>(3 + static_cast<uint8_t>(rms * 5.0f));
        if (dotCount < 3) dotCount = 3;
        if (dotCount > 8) dotCount = 8;
    }

    // Chroma-anchored hue (non-rainbow): circular weighted mean for smooth, continuous colour.
    const float rawDt = ctx.getSafeRawDeltaSeconds();
    uint8_t baseHue = ctx.gHue;
    if (chroma) {
        baseHue = effects::chroma::circularChromaHueSmoothed(
            chroma, m_chromaAngle, rawDt, 0.20f);
    }

    for (uint8_t i = 0; i < dotCount; i++) {
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
        float elastic = 1.0f;
        if (tempoOk) {
            elastic = 0.70f + (0.60f * beatStrength);  // stronger beat → longer throws
        }
        float accent = 1.0f;
        if (flux > 0.20f) {
            accent = 1.0f + ((flux - 0.20f) * 0.6f);   // transient accents
            if (accent > 1.30f) accent = 1.30f;
        }
        float maxDistF = static_cast<float>(HALF_LENGTH) * (0.10f + 0.90f * mag01) * elastic * accent;
        if (maxDistF > static_cast<float>(HALF_LENGTH)) maxDistF = static_cast<float>(HALF_LENGTH);
        const uint16_t maxDist = static_cast<uint16_t>(maxDistF);

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
        const uint8_t dotHue = static_cast<uint8_t>(baseHue + (i * 12));
        CRGB color = ctx.palette.getColor(dotHue, brightU8);

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
