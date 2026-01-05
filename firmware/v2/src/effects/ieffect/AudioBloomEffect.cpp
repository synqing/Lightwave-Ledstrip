/**
 * @file AudioBloomEffect.cpp
 * @brief Sensory Bridge-style scrolling bloom implementation
 */

#include "AudioBloomEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
#include "../../audio/contracts/ControlBus.h"

#ifdef FEATURE_EFFECT_VALIDATION
#include "../../validation/EffectValidationMacros.h"
#endif

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {

/**
 * @brief Compute palette warmth offset from chord type.
 *
 * Maps chord qualities to hue offsets for emotional color mapping:
 *   MAJOR     -> +32 (warm/orange shift)
 *   MINOR     -> -24 (cool/blue shift)
 *   DIMINISHED-> -32 (darker/cooler)
 *   AUGMENTED -> +40 (bright/ethereal)
 *   NONE      -> 0   (neutral)
 *
 * @param type Detected chord type
 * @param confidence Chord detection confidence (0.0-1.0)
 * @return int8_t Signed hue offset (-40 to +40)
 */
int8_t computeChordWarmthOffset(lightwaveos::audio::ChordType type, float confidence) {
    // Base warmth values per chord type
    int8_t baseOffset = 0;
    switch (type) {
        case lightwaveos::audio::ChordType::MAJOR:
            baseOffset = 32;   // Warm/orange
            break;
        case lightwaveos::audio::ChordType::MINOR:
            baseOffset = -24;  // Cool/blue
            break;
        case lightwaveos::audio::ChordType::DIMINISHED:
            baseOffset = -32;  // Dark/cold
            break;
        case lightwaveos::audio::ChordType::AUGMENTED:
            baseOffset = 40;   // Bright/ethereal
            break;
        case lightwaveos::audio::ChordType::NONE:
        default:
            return 0;  // No shift when no chord detected
    }

    // Scale offset by confidence (0.0-1.0) for smooth transitions
    // Minimum confidence threshold of 0.3 before applying any shift
    if (confidence < 0.3f) {
        return 0;
    }

    // Scale linearly from 0.3-1.0 confidence
    float scaledConfidence = (confidence - 0.3f) / 0.7f;
    return (int8_t)(baseOffset * scaledConfidence);
}

/**
 * @brief Compute hue offset from chord root note.
 *
 * Maps root note (0-11) to a hue rotation that complements
 * the palette. Each semitone shifts by 21 hue units (252/12).
 *
 * @param rootNote Root note 0-11 (C=0, C#=1, ..., B=11)
 * @param confidence Chord detection confidence
 * @return uint8_t Hue rotation offset (0-252)
 */
uint8_t computeRootNoteHueShift(uint8_t rootNote, float confidence) {
    if (confidence < 0.3f) {
        return 0;  // No shift below confidence threshold
    }

    // 21 hue units per semitone (252 / 12 = 21)
    // Scale by confidence for smooth transitions
    float scaledConfidence = (confidence - 0.3f) / 0.7f;
    uint8_t baseShift = rootNote * 21;
    return (uint8_t)(baseShift * scaledConfidence * 0.5f);  // 50% intensity to avoid over-rotation
}

} // anonymous namespace

bool AudioBloomEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // Initialize radial buffers to black
    memset(m_radial, 0, sizeof(m_radial));
    memset(m_radialAux, 0, sizeof(m_radialAux));
    memset(m_radialTemp, 0, sizeof(m_radialTemp));
    m_iter = 0;
    m_lastHopSeq = 0;
    m_scrollPhase = 0.0f;
    m_subBassPulse = 0.0f;
    m_burstIntensity = 0.0f;
    return true;
}

void AudioBloomEffect::render(plugins::EffectContext& ctx) {
    // Clear output buffer
    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));

#if !FEATURE_AUDIO_SYNC
    (void)ctx;
    return;
#else
    if (!ctx.audio.available) {
        return;
    }

    // Check if we have a new hop (update on hop sequence change)
    bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
    if (newHop) {
        m_lastHopSeq = ctx.audio.controlBus.hop_seq;
        m_iter++;

        // =====================================================================
        // 64-bin Sub-Bass Processing (bins 0-5 = 110-155 Hz for deep bass punch)
        // Uses fine-grained frequency data for sub-bass detail the 8-band
        // analyzer misses. This gives the effect more punch on bass drops.
        // =====================================================================
        float subBassSum = 0.0f;
        for (uint8_t i = 0; i < 6; ++i) {
            subBassSum += ctx.audio.bin(i);  // bins64[0..5]
        }
        float subBassAvg = subBassSum / 6.0f;

        // Fast attack, slow release for punchy bass response
        if (subBassAvg > m_subBassPulse) {
            m_subBassPulse = subBassAvg;  // Instant attack
        } else {
            m_subBassPulse *= 0.85f;  // ~100ms decay at 60fps
        }
    }

    // Update on even iterations (matching Sensory Bridge's bitRead(iter, 0) == 0)
    if ((m_iter & 1) == 0 && newHop) {
        // Compute sum_color from chromagram (matching Sensory Bridge light_mode_bloom)
        const float led_share = 255.0f / 12.0f;
        CRGB sum_color = CRGB(0, 0, 0);
        float brightness_sum = 0.0f;
        const bool chromaticMode = (ctx.saturation >= 128);

        // Chord-driven palette warmth adjustment
        // Maps chord type to hue offset for emotional color response
        const auto& chordState = ctx.audio.controlBus.chordState;
        int8_t warmthOffset = computeChordWarmthOffset(chordState.type, chordState.confidence);
        uint8_t rootHueShift = computeRootNoteHueShift(chordState.rootNote, chordState.confidence);

        // Combined hue adjustment: base gHue + warmth + root note influence
        // Cast to int16_t to handle signed arithmetic, then wrap to 0-255
        int16_t adjustedHue = (int16_t)ctx.gHue + warmthOffset + rootHueShift;
        if (adjustedHue < 0) adjustedHue += 256;
        if (adjustedHue > 255) adjustedHue -= 256;
        uint8_t chordAdjustedHue = (uint8_t)adjustedHue;

        for (uint8_t i = 0; i < 12; ++i) {
            float prog = i / 12.0f;
            float bin = ctx.audio.controlBus.heavy_chroma[i];

            // Apply squaring (SQUARE_ITER, typically 1)
            float bright = bin;
            bright = bright * bright;  // Square once
            bright *= 1.5f;  // Gain boost
            if (bright > 1.0f) bright = 1.0f;

            bright *= led_share;

            if (chromaticMode) {
                // Use palette for colour with chord-adjusted hue base
                // Palette index includes chord warmth for emotional color response
                uint8_t paletteIdx = (uint8_t)(prog * 255.0f + chordAdjustedHue);
                uint8_t brightU8 = (uint8_t)bright;
                brightU8 = (uint8_t)((brightU8 * ctx.brightness) / 255);
                CRGB out_col = ctx.palette.getColor(paletteIdx, brightU8);
                sum_color += out_col;
            } else {
                brightness_sum += bright;
            }
        }

        if (!chromaticMode) {
            // Non-chromatic mode: single color from palette with chord warmth
            uint8_t brightU8 = (uint8_t)brightness_sum;
            brightU8 = (uint8_t)((brightU8 * ctx.brightness) / 255);
            sum_color = ctx.palette.getColor(chordAdjustedHue, brightU8);
        }

        // Fractional scroll accumulator (smooth motion)
        float scrollRate = 0.3f + (ctx.speed / 50.0f) * 2.2f;  // 0.3-2.5 LEDs/hop
        m_scrollPhase += scrollRate;

        uint8_t step = (uint8_t)m_scrollPhase;
        m_scrollPhase -= step;  // Keep fractional remainder

        if (step > HALF_LENGTH - 1) step = HALF_LENGTH - 1;

        if (step > 0) {
            for (uint8_t i = 0; i < HALF_LENGTH - step; ++i) {
                m_radialTemp[(HALF_LENGTH - 1) - i] = m_radial[(HALF_LENGTH - 1) - i - step];
            }
            for (uint8_t i = 0; i < step; ++i) {
                m_radialTemp[i] = sum_color;
            }
        } else {
            memcpy(m_radialTemp, m_radial, sizeof(m_radialTemp));
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
        increaseSaturation(m_radial, HALF_LENGTH, 24);

        // Save to aux buffer
        memcpy(m_radialAux, m_radial, sizeof(m_radial));
    } else {
        // Alternate frames: load from aux buffer
        memcpy(m_radial, m_radialAux, sizeof(m_radial));
    }

    // Compute scroll rate for validation (same as in update block)
    float scrollRate = 0.3f + (ctx.speed / 50.0f) * 2.2f;  // 0.3-2.5 LEDs/hop

    // Find maxBin and sum from heavy_chroma for validation
    float maxBin = 0.0f;
    float sum = 0.0f;
    for (uint8_t i = 0; i < 12; ++i) {
        float v = ctx.audio.controlBus.heavy_chroma[i];
        sum += v;
        if (v > maxBin) maxBin = v;
    }

#ifdef FEATURE_EFFECT_VALIDATION
    VALIDATION_INIT(21);  // Effect ID for AudioBloom
    VALIDATION_SCROLL(m_scrollPhase);
    VALIDATION_SPEED(scrollRate, m_scrollPhase);  // Use scroll rate as speed proxy
    VALIDATION_AUDIO(maxBin, sum, 0.0f);  // maxBin is dominant frequency
    VALIDATION_SUBMIT(::lightwaveos::validation::g_validationRing);
#endif

    // Render radial buffer to LEDs (centre-origin)
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        SET_CENTER_PAIR(ctx, dist, m_radial[dist]);
    }

    // =========================================================================
    // 64-bin Sub-Bass Center Pulse
    // Adds a brightness boost to center LEDs on bass hits using fine-grained
    // sub-bass data from the 64-bin analyzer. Creates punchy bass response.
    // =========================================================================
    if (m_subBassPulse > 0.1f) {
        // Pulse radius scales with sub-bass intensity (max ~20 LEDs from center)
        uint16_t pulseRadius = (uint16_t)(m_subBassPulse * 20.0f);
        if (pulseRadius > HALF_LENGTH / 4) pulseRadius = HALF_LENGTH / 4;

        // Boost factor: subtle at low levels, strong on drops
        uint8_t boost = (uint8_t)(m_subBassPulse * 80.0f);  // 0-80 brightness add

        for (uint16_t dist = 0; dist < pulseRadius; ++dist) {
            // Fade boost toward edge of pulse
            float fadeIn = 1.0f - ((float)dist / (float)pulseRadius);
            uint8_t fadedBoost = (uint8_t)(boost * fadeIn);

            // Apply warm-tinted boost to center pair
            uint16_t leftIdx = ctx.centerPoint - 1 - dist;
            uint16_t rightIdx = ctx.centerPoint + dist;

            // Warm tint: more red, some green, minimal blue
            CRGB warmBoost = CRGB(fadedBoost, fadedBoost >> 2, fadedBoost >> 4);
            if (leftIdx < ctx.ledCount) {
                ctx.leds[leftIdx] += warmBoost;
            }
            if (rightIdx < ctx.ledCount) {
                ctx.leds[rightIdx] += warmBoost;
            }
        }
    }

    // =========================================================================
    // Percussion Burst (snare-triggered center burst)
    // Adds a bright burst at center on snare hits for enhanced rhythmic response.
    // =========================================================================
    if (ctx.audio.isSnareHit()) {
        // Trigger burst on snare hit, scale with snare energy
        float snareEnergy = ctx.audio.snare();
        m_burstIntensity = fmaxf(m_burstIntensity, snareEnergy);
    } else {
        // Decay burst over time (~150ms at 120 FPS)
        m_burstIntensity *= 0.92f;
        if (m_burstIntensity < 0.01f) m_burstIntensity = 0.0f;
    }

    if (m_burstIntensity > 0.1f) {
        // Burst radius scales with intensity (max ~15 LEDs from center)
        uint16_t burstRadius = (uint16_t)(m_burstIntensity * 15.0f);
        if (burstRadius > HALF_LENGTH / 5) burstRadius = HALF_LENGTH / 5;

        // Bright burst: white-tinted for visibility
        uint8_t burstBright = (uint8_t)(m_burstIntensity * 120.0f);  // 0-120 brightness add

        for (uint16_t dist = 0; dist < burstRadius; ++dist) {
            // Fade burst toward edge
            float fadeIn = 1.0f - ((float)dist / (float)burstRadius);
            uint8_t fadedBurst = (uint8_t)(burstBright * fadeIn);

            // Apply white-tinted burst to center pair
            uint16_t leftIdx = ctx.centerPoint - 1 - dist;
            uint16_t rightIdx = ctx.centerPoint + dist;

            // White tint: equal RGB for bright flash
            CRGB burstColor = CRGB(fadedBurst, fadedBurst, fadedBurst);
            if (leftIdx < ctx.ledCount) {
                ctx.leds[leftIdx] += burstColor;
            }
            if (rightIdx < ctx.ledCount) {
                ctx.leds[rightIdx] += burstColor;
            }
        }
    }
#endif  // FEATURE_AUDIO_SYNC
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
        hsv2rgb_spectrum(hsv, buffer[i]);
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
