/**
 * @file AudioBloomEffect.cpp
 * @brief Sensory Bridge-style scrolling bloom implementation
 */

#include "AudioBloomEffect.h"
#include "../CoreEffects.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool AudioBloomEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // Initialize radial buffers to black
    memset(m_radial, 0, sizeof(m_radial));
    memset(m_radialAux, 0, sizeof(m_radialAux));
    memset(m_radialTemp, 0, sizeof(m_radialTemp));
    m_iter = 0;
    m_lastHopSeq = 0;
    return true;
}

void AudioBloomEffect::render(plugins::EffectContext& ctx) {
    // Clear output buffer
    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));

    if (!ctx.audio.available) {
        return;
    }

    // Check if we have a new hop (update on hop sequence change)
    bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
    if (newHop) {
        m_lastHopSeq = ctx.audio.controlBus.hop_seq;
        m_iter++;
    }

    // Update on even iterations (matching Sensory Bridge's bitRead(iter, 0) == 0)
    if ((m_iter & 1) == 0 && newHop) {
        // Compute sum_color from chromagram (matching Sensory Bridge light_mode_bloom)
        const float led_share = 255.0f / 12.0f;
        CRGB sum_color = CRGB(0, 0, 0);
        float brightness_sum = 0.0f;
        const bool chromaticMode = (ctx.saturation >= 128);

        for (uint8_t i = 0; i < 12; ++i) {
            float prog = i / 12.0f;
            float bin = ctx.audio.controlBus.chroma[i];

            // Apply squaring (SQUARE_ITER, typically 1)
            float bright = bin;
            bright = bright * bright;  // Square once
            bright *= 1.5f;  // Gain boost
            if (bright > 1.0f) bright = 1.0f;

            bright *= led_share;

            if (chromaticMode) {
                // Use palette for colour (no hue wheel)
                uint8_t paletteIdx = (uint8_t)(prog * 255.0f + ctx.gHue);
                uint8_t brightU8 = (uint8_t)bright;
                brightU8 = (uint8_t)((brightU8 * ctx.brightness) / 255);
                CRGB out_col = ctx.palette.getColor(paletteIdx, brightU8);
                sum_color += out_col;
            } else {
                brightness_sum += bright;
            }
        }

        if (!chromaticMode) {
            uint8_t brightU8 = (uint8_t)brightness_sum;
            brightU8 = (uint8_t)((brightU8 * ctx.brightness) / 255);
            sum_color = ctx.palette.getColor(ctx.gHue, brightU8);
        }

        // Determine scroll step (fast/slow based on speed)
        // Map ctx.speed (1-50) to step: low speed = 1, high speed = 2
        uint8_t step = (ctx.speed > 25) ? 2 : 1;

        // Shift radial buffer outward and insert sum_color at start
        if (step == 2) {
            // Fast mode: shift 2 LEDs
            for (uint8_t i = 0; i < HALF_LENGTH - 2; ++i) {
                m_radialTemp[(HALF_LENGTH - 1) - i] = m_radial[(HALF_LENGTH - 1) - i - 2];
            }
            m_radialTemp[0] = sum_color;
            m_radialTemp[1] = sum_color;
        } else {
            // Slow mode: shift 1 LED
            for (uint8_t i = 0; i < HALF_LENGTH - 1; ++i) {
                m_radialTemp[(HALF_LENGTH - 1) - i] = m_radial[(HALF_LENGTH - 1) - i - 1];
            }
            m_radialTemp[0] = sum_color;
        }

        // Copy temp to main radial buffer
        memcpy(m_radial, m_radialTemp, sizeof(m_radial));

        // Apply post-processing (matching Sensory Bridge)
        // 1. Logarithmic distortion
        distortLogarithmic(m_radial, m_radialTemp, HALF_LENGTH);
        memcpy(m_radial, m_radialTemp, sizeof(m_radial));

        // 2. Fade top half (toward edge)
        fadeTopHalf(m_radial, HALF_LENGTH);

        // 3. Increase saturation
        increaseSaturation(m_radial, HALF_LENGTH, 32);

        // Save to aux buffer
        memcpy(m_radialAux, m_radial, sizeof(m_radial));
    } else {
        // Alternate frames: load from aux buffer
        memcpy(m_radial, m_radialAux, sizeof(m_radial));
    }

    // Render radial buffer to LEDs (centre-origin)
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        SET_CENTER_PAIR(ctx, dist, m_radial[dist]);
    }
}

void AudioBloomEffect::distortLogarithmic(CRGB* src, CRGB* dst, uint16_t len) {
    // Logarithmic distortion: sqrt remap + lerp
    // Maps linear position to sqrt(position) for compression toward centre
    for (uint16_t i = 0; i < len; ++i) {
        float prog = (float)i / (float)(len - 1);
        float prog_distorted = sqrtf(prog);  // sqrt compresses toward 0
        
        // Linear interpolation to find source position
        float srcPos = prog_distorted * (len - 1);
        uint16_t srcIdx = (uint16_t)srcPos;
        float fract = srcPos - srcIdx;
        
        if (srcIdx >= len - 1) {
            dst[i] = src[len - 1];
        } else {
            // Lerp between src[srcIdx] and src[srcIdx+1]
            CRGB c1 = src[srcIdx];
            CRGB c2 = src[srcIdx + 1];
            dst[i] = CRGB(
                (uint8_t)(c1.r * (1.0f - fract) + c2.r * fract),
                (uint8_t)(c1.g * (1.0f - fract) + c2.g * fract),
                (uint8_t)(c1.b * (1.0f - fract) + c2.b * fract)
            );
        }
    }
}

void AudioBloomEffect::fadeTopHalf(CRGB* buffer, uint16_t len) {
    // Fade toward edge (top half = outer half in radial space)
    uint16_t halfLen = len >> 1;
    for (uint8_t i = 0; i < halfLen; ++i) {
        float fade = (float)i / (float)halfLen;
        uint16_t idx = (len - 1) - i;  // Index from edge toward centre
        buffer[idx].r = (uint8_t)(buffer[idx].r * fade);
        buffer[idx].g = (uint8_t)(buffer[idx].g * fade);
        buffer[idx].b = (uint8_t)(buffer[idx].b * fade);
    }
}

void AudioBloomEffect::increaseSaturation(CRGB* buffer, uint16_t len, uint8_t amount) {
    for (uint16_t i = 0; i < len; ++i) {
        CHSV hsv = rgb2hsv_approximate(buffer[i]);
        hsv.s = qadd8(hsv.s, amount);
        buffer[i] = hsv;
    }
}

void AudioBloomEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& AudioBloomEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Audio Bloom",
        "Scrolling bloom effect with chromagram-driven colour, centre-origin push-outwards",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
