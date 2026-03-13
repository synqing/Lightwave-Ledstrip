/**
 * @file SbK1BloomEffect.cpp
 * @brief K1.Lightwave Bloom mode — parity port of lightshow_modes.h:502-647
 *
 * Algorithm: centre-origin scrolling trail with chromagram colour synthesis.
 * See SbK1BloomEffect.h for full algorithm description.
 */

#include "SbK1BloomEffect.h"

#include "../AudioReactivePolicy.h"
#include "../../CoreEffects.h"
#include "../../../config/features.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#include <esp_heap_caps.h>
#endif

#include <cmath>
#include <cstring>

namespace lightwaveos::effects::ieffect::sensorybridge_reference {

// =========================================================================
// Parameter descriptors
// =========================================================================

const plugins::EffectParameter SbK1BloomEffect::s_params[kParamCount] = {
    {"mood",        "Scroll Speed", 0.0f, 1.0f, 0.16f, plugins::EffectParameterType::FLOAT, 0.01f, "animation", "",  false},
    {"contrast",    "Contrast",     0.0f, 3.0f, 1.0f,  plugins::EffectParameterType::FLOAT, 0.25f, "visual",    "x", false},
    {"saturation",  "Saturation",   0.0f, 1.0f, 1.0f,  plugins::EffectParameterType::FLOAT, 0.05f, "colour",    "",  false},
    {"chromaHue",   "Hue Offset",   0.0f, 1.0f, 0.0f,  plugins::EffectParameterType::FLOAT, 0.01f, "colour",    "",  false},
    {"incandescent","Warm Filter",  0.0f, 1.0f, 0.0f,  plugins::EffectParameterType::FLOAT, 0.05f, "colour",    "",  false},
    {"prismCount",  "Prism Layers", 0.0f, 5.0f, 1.42f, plugins::EffectParameterType::FLOAT, 0.1f,  "visual",    "",  false},
    {"bulbOpacity", "Bulb Cover",   0.0f, 1.0f, 0.0f,  plugins::EffectParameterType::FLOAT, 0.05f, "visual",    "",  false},
};

// =========================================================================
// Lifecycle
// =========================================================================

bool SbK1BloomEffect::init(plugins::EffectContext& ctx) {
    // Base class allocates PSRAM for chromagram pipeline
    if (!SbK1BaseEffect::init(ctx)) return false;

#ifndef NATIVE_BUILD
    if (!m_bloom) {
        m_bloom = static_cast<SbK1BloomPsram*>(
            heap_caps_malloc(sizeof(SbK1BloomPsram), MALLOC_CAP_SPIRAM));
        if (!m_bloom) {
            SbK1BaseEffect::cleanup();
            return false;
        }
    }
    std::memset(m_bloom, 0, sizeof(SbK1BloomPsram));
#endif

    // Reset parameters to defaults
    m_mood        = 0.16f;
    m_contrast    = 1.0f;
    m_saturation  = 1.0f;
    m_chromaHue   = 0.0f;
    m_incandescent = 0.0f;
    m_prismCount  = 1.42f;
    m_bulbOpacity = 0.0f;

    return true;
}

void SbK1BloomEffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_bloom) {
        heap_caps_free(m_bloom);
        m_bloom = nullptr;
    }
#endif
    SbK1BaseEffect::cleanup();
}

// =========================================================================
// Sub-pixel additive sprite blit (K1 draw_sprite parity)
// =========================================================================

void SbK1BloomEffect::drawSprite(CRGB_F* dest, const CRGB_F* sprite,
                                  int destLen, int spriteLen,
                                  float position, float alpha) {
    const int posWhole = (int)position;
    const float posFract = position - (float)posWhole;
    const float mixLeft  = (1.0f - posFract) * alpha;
    const float mixRight = posFract * alpha;

    for (int i = 0; i < spriteLen; ++i) {
        const int pL = i + posWhole;
        const int pR = i + posWhole + 1;

        if (pL >= 0 && pL < destLen) {
            dest[pL] += sprite[i] * mixLeft;
        }
        if (pR >= 0 && pR < destLen) {
            dest[pR] += sprite[i] * mixRight;
        }
    }
}

// =========================================================================
// Prism transform: scale_image_to_half → shift_leds_up → mirror_downwards
// =========================================================================

void SbK1BloomEffect::prismTransform(CRGB_F* buf, CRGB_F* tmp) {
    // Scale to half: average adjacent pairs into first 80 pixels
    for (uint16_t i = 0; i < kHalf; ++i) {
        tmp[i].r = buf[i * 2].r * 0.5f + buf[i * 2 + 1].r * 0.5f;
        tmp[i].g = buf[i * 2].g * 0.5f + buf[i * 2 + 1].g * 0.5f;
        tmp[i].b = buf[i * 2].b * 0.5f + buf[i * 2 + 1].b * 0.5f;
    }
    std::memset(&tmp[kHalf], 0, kHalf * sizeof(CRGB_F));
    std::memcpy(buf, tmp, kStripLen * sizeof(CRGB_F));

    // Shift up by half: move 80 pixels to upper half, clear lower half
    std::memcpy(tmp, buf, kStripLen * sizeof(CRGB_F));
    std::memcpy(&buf[kHalf], tmp, kHalf * sizeof(CRGB_F));
    std::memset(buf, 0, kHalf * sizeof(CRGB_F));

    // Mirror downwards: upper half mirrors to lower half
    for (uint16_t i = 0; i < kHalf; ++i) {
        tmp[kHalf + i] = buf[kHalf + i];
        tmp[kHalf - 1 - i] = buf[kHalf + i];
    }
    std::memcpy(buf, tmp, kStripLen * sizeof(CRGB_F));
}

// =========================================================================
// Render (called by base class after baseProcessAudio)
// =========================================================================

void SbK1BloomEffect::renderEffect(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_bloom) return;
#else
    (void)ctx;
    return;
#endif

#ifndef NATIVE_BUILD
#if !FEATURE_AUDIO_SYNC
    (void)ctx;
    return;
#else
    if (!ctx.audio.available) {
        fadeToBlackBy(ctx.leds, ctx.ledCount, 32);
        return;
    }

    CRGB_F* workBuf = m_bloom->workBuffer;
    CRGB_F* prevBuf = m_bloom->prevBuffer;

    // -----------------------------------------------------------------
    // Step 1: Clear working buffer
    // -----------------------------------------------------------------
    std::memset(workBuf, 0, kStripLen * sizeof(CRGB_F));

    // -----------------------------------------------------------------
    // Step 2: Fractional-pixel scroll — right half outward
    //   Sub-pixel accumulator makes scroll speed continuous instead of
    //   binary 1px/2px step. MOOD controls base speed (0.5–2.0 px/frame).
    // -----------------------------------------------------------------
    {
        float scrollSpeed = 0.5f + m_mood * 1.5f;
        m_scrollAccum += scrollSpeed;
        int pixelsToScroll = static_cast<int>(m_scrollAccum);
        m_scrollAccum -= static_cast<float>(pixelsToScroll);
        if (pixelsToScroll > 3) pixelsToScroll = 3;  // Prevent buffer overrun

        if (pixelsToScroll > 0) {
            for (int j = kStripLen - 1; j >= (int)(kHalf + pixelsToScroll); --j) {
                workBuf[j] = prevBuf[j - pixelsToScroll];
            }
        }
    }

    // -----------------------------------------------------------------
    // Step 3: K1-parity colour synthesis from 12-bin chromagram
    //   Additive accumulation of palette colours (one per active bin),
    //   then normalize by total_magnitude. K1's normalization acts as
    //   automatic brightness control — pushes output toward full brightness
    //   regardless of absolute bin values. This is critical for trail
    //   propagation distance.
    // -----------------------------------------------------------------
    const bool chromaticMode = (ctx.saturation >= 128);

    CRGB_F bloomColor = {0.0f, 0.0f, 0.0f};
    float totalMag = 0.0f;

    for (uint8_t c = 0; c < 12; ++c) {
        float bin = m_chromaSmooth[c];
        bin = applyContrast(bin, m_contrast);

        if (bin > 0.05f) {
            float prog = c / 12.0f;
            // BLOOM-SPECIFIC: start at cyan (0.5 offset), not red
            float palPos = prog + 0.5f;

            // K1 parity: only apply auto colour shift in chromatic mode
            if (chromaticMode) {
                palPos += m_huePosition;
            }

            // K1 parity: palette lookup at bin brightness, then accumulate
            // (matches K1's hsv(note_hue, SATURATION, bin) + additive sum)
            CRGB_F noteColor = paletteColorF(ctx.palette, palPos, bin);
            bloomColor += noteColor;
            totalMag += bin;
        }
    }

    // K1 parity: normalize by total_magnitude (lines 572-576)
    // This is the key to K1's brightness — dividing by totalMag pushes
    // the max channel toward 1.0, ensuring bright centre inserts for
    // trail propagation.
    if (totalMag > 0.01f) {
        bloomColor.r /= totalMag;
        bloomColor.g /= totalMag;
        bloomColor.b /= totalMag;
    }

    // Clip to [0,1]
    bloomColor.clip();

    // K1 parity: force_saturation — ensure vivid colours from palette
#ifndef NATIVE_BUILD
    {
        CRGB tempRgb = bloomColor.toCRGB();
        CHSV tempHsv = rgb2hsv_approximate(tempRgb);
        tempHsv.s = ctx.saturation;
        hsv2rgb_rainbow(tempHsv, tempRgb);
        bloomColor = CRGB_F::fromCRGB(tempRgb);
    }
#endif

    // FAILSAFE: if chromagram produced no colour but audio is present,
    // generate a fallback colour from hue position + RMS brightness
    if (totalMag < 0.01f && ctx.audio.rms() > 0.02f) {
        float fbPos = m_chromaHue + m_huePosition + 0.5f;
        bloomColor = paletteColorF(ctx.palette, fbPos, ctx.audio.rms());
        totalMag = ctx.audio.rms();
    }

    // -----------------------------------------------------------------
    // Step 4: Non-chromatic mode — force palette position from CHROMA knob
    //   K1 parity: force_hue() preserves brightness, replaces hue.
    //   Palette equivalent: lookup at forced position using mixed brightness.
    // -----------------------------------------------------------------
    if (!chromaticMode) {
        float maxComp = fmaxf(bloomColor.r, fmaxf(bloomColor.g, bloomColor.b));
        if (maxComp < 0.001f) maxComp = 0.001f;
        float forcedPos = m_chromaHue + m_huePosition;
        bloomColor = paletteColorF(ctx.palette, forcedPos, fminf(maxComp, 1.0f));
    }

    // -----------------------------------------------------------------
    // Step 6: Apply PHOTONS brightness
    //   Scale by master brightness
    // -----------------------------------------------------------------
    const float photons = (float)ctx.brightness / 255.0f;
    bloomColor *= photons;
    bloomColor.clip();

    // -----------------------------------------------------------------
    // Step 7: Insert colour at centre (K1 algo 0 parity)
    // -----------------------------------------------------------------
    // Smooth centre colour transitions (tau ~30ms — responsive but not jarring)
    {
        float blendAlpha = 1.0f - expf(-m_dt / 0.030f);
        workBuf[kHalf].r = prevBuf[kHalf].r + (bloomColor.r - prevBuf[kHalf].r) * blendAlpha;
        workBuf[kHalf].g = prevBuf[kHalf].g + (bloomColor.g - prevBuf[kHalf].g) * blendAlpha;
        workBuf[kHalf].b = prevBuf[kHalf].b + (bloomColor.b - prevBuf[kHalf].b) * blendAlpha;
    }

    // -----------------------------------------------------------------
    // Step 8: Save undistorted right half to scroll state
    //   Only save BEFORE distortion so sqrt remap doesn't accumulate.
    //   Only the right half matters — left half is always mirrored.
    // -----------------------------------------------------------------
    std::memcpy(&prevBuf[kHalf], &workBuf[kHalf], kHalf * sizeof(CRGB_F));

    // -----------------------------------------------------------------
    // Step 8.5: Sqrt spatial distortion on right half (K1 parity)
    //   Remaps pixel positions: sqrt(prog) stretches near center,
    //   compresses at edges. Creates visible ripple/band separation.
    // -----------------------------------------------------------------
    {
        CRGB_F* distBuf = m_bloom->prismFxBuf;  // Reuse as scratch
        for (uint16_t i = 0; i < kHalf; ++i) {
            float prog = (float)i / (float)(kHalf - 1);
            float prog_d = sqrtf(prog);
            float srcF = (float)kHalf + prog_d * (float)(kHalf - 1);
            if (srcF > (float)(kStripLen - 2)) srcF = (float)(kStripLen - 2);

            // Sub-pixel linear interpolation
            int srcLow = (int)srcF;
            float frac = srcF - (float)srcLow;
            int srcHigh = srcLow + 1;

            distBuf[kHalf + i].r = workBuf[srcLow].r * (1.0f - frac) + workBuf[srcHigh].r * frac;
            distBuf[kHalf + i].g = workBuf[srcLow].g * (1.0f - frac) + workBuf[srcHigh].g * frac;
            distBuf[kHalf + i].b = workBuf[srcLow].b * (1.0f - frac) + workBuf[srcHigh].b * frac;
        }
        std::memcpy(&workBuf[kHalf], &distBuf[kHalf], kHalf * sizeof(CRGB_F));
    }

    // -----------------------------------------------------------------
    // Step 9: Edge fade (linear, center=bright edge=dark — K1 parity)
    // -----------------------------------------------------------------
    for (uint16_t i = 0; i < kHalf; ++i) {
        float fade = (float)(kHalf - 1 - i) / (float)(kHalf - 1);
        workBuf[kHalf + i] *= fade;
    }

    // -----------------------------------------------------------------
    // Step 10: Mirror right half to left half
    // -----------------------------------------------------------------
    for (uint16_t i = 0; i < kHalf; ++i) {
        workBuf[kCenterLeft - i] = workBuf[kCenterRight + i];
    }

    // -----------------------------------------------------------------
    // Prism effect (K1 post-processing parity)
    //   scale→shift→mirror→additive blend, with per-iteration hue shift
    // -----------------------------------------------------------------
    if (m_prismCount > 0.01f) {
        CRGB_F* fxBuf  = m_bloom->prismFxBuf;
        CRGB_F* tmpBuf = m_bloom->prismTmpBuf;

        const uint8_t wholeIter = (uint8_t)m_prismCount;
        const float fractIter = m_prismCount - floorf(m_prismCount);

        for (uint8_t iter = 0; iter < wholeIter; ++iter) {
            std::memcpy(fxBuf, workBuf, kStripLen * sizeof(CRGB_F));
            prismTransform(fxBuf, tmpBuf);

            // Hue shift: +5% per iteration (iter 0 = no shift)
            const float hueShift = iter * 0.05f;
#ifndef NATIVE_BUILD
            if (hueShift > 0.001f) {
                const uint8_t hShift8 = (uint8_t)(hueShift * 255.0f);
                for (uint16_t j = 0; j < kStripLen; ++j) {
                    if (fxBuf[j].r > 0.002f || fxBuf[j].g > 0.002f || fxBuf[j].b > 0.002f) {
                        CRGB rgb = fxBuf[j].toCRGB();
                        CHSV hsv = rgb2hsv_approximate(rgb);
                        hsv.h += hShift8;
                        hsv2rgb_rainbow(hsv, rgb);
                        fxBuf[j] = CRGB_F::fromCRGB(rgb);
                    }
                }
            }
#endif
            // Additive blend at 0.25 opacity (K1 parity)
            for (uint16_t j = 0; j < kStripLen; ++j) {
                workBuf[j].r += fxBuf[j].r * 0.25f;
                workBuf[j].g += fxBuf[j].g * 0.25f;
                workBuf[j].b += fxBuf[j].b * 0.25f;
            }
        }

        // Fractional iteration at reduced opacity
        if (fractIter > 0.01f) {
            std::memcpy(fxBuf, workBuf, kStripLen * sizeof(CRGB_F));
            prismTransform(fxBuf, tmpBuf);

            const float fractOpacity = 0.25f * fractIter;
            for (uint16_t j = 0; j < kStripLen; ++j) {
                workBuf[j].r += fxBuf[j].r * fractOpacity;
                workBuf[j].g += fxBuf[j].g * fractOpacity;
                workBuf[j].b += fxBuf[j].b * fractOpacity;
            }
        }
    }

    // -----------------------------------------------------------------
    // Bulb cover: 4-LED repeating pattern [0.25, 1.0, 0.25, 0.0]
    //   Simulates discrete incandescent bulbs with hot centres and gaps
    // -----------------------------------------------------------------
    if (m_bulbOpacity > 0.001f) {
        static constexpr float kBulbPattern[4] = {0.25f, 1.0f, 0.25f, 0.0f};
        const float opInv = 1.0f - m_bulbOpacity;

        for (uint16_t i = 0; i < kStripLen; ++i) {
            const float cover = kBulbPattern[i & 3];
            workBuf[i].r = workBuf[i].r * opInv + workBuf[i].r * cover * m_bulbOpacity;
            workBuf[i].g = workBuf[i].g * opInv + workBuf[i].g * cover * m_bulbOpacity;
            workBuf[i].b = workBuf[i].b * opInv + workBuf[i].b * cover * m_bulbOpacity;
        }
    }

    // -----------------------------------------------------------------
    // Incandescent warm-white filter (post-processing)
    // -----------------------------------------------------------------
    if (m_incandescent > 0.001f) {
        const float inv = 1.0f - m_incandescent;
        const float rScale = inv + m_incandescent * kIncanR;
        const float gScale = inv + m_incandescent * kIncanG;
        const float bScale = inv + m_incandescent * kIncanB;

        for (uint16_t i = 0; i < kStripLen; ++i) {
            workBuf[i].r *= rScale;
            workBuf[i].g *= gScale;
            workBuf[i].b *= bScale;
        }
    }

    // -----------------------------------------------------------------
    // Step 11: Convert to ctx.leds and copy to strip 2
    // -----------------------------------------------------------------
    const uint16_t ledCount = ctx.ledCount;
    for (uint16_t i = 0; i < kStripLen && i < ledCount; ++i) {
        ctx.leds[i] = workBuf[i].toCRGB();
    }
    // Mirror to strip 2 (LEDs 160-319)
    for (uint16_t i = 0; i < kStripLen && (kStripLen + i) < ledCount; ++i) {
        ctx.leds[kStripLen + i] = ctx.leds[i];
    }

#endif // FEATURE_AUDIO_SYNC
#endif // NATIVE_BUILD
}

// =========================================================================
// Metadata
// =========================================================================

const plugins::EffectMetadata& SbK1BloomEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "K1 Bloom",
        "K1 Lightwave bloom scrolling trail mode (parity port)",
        plugins::EffectCategory::PARTY,
        1,
        "K1.Lightwave"
    };
    return meta;
}

// =========================================================================
// Parameter interface
// =========================================================================

uint8_t SbK1BloomEffect::getParameterCount() const {
    return kParamCount;
}

const plugins::EffectParameter* SbK1BloomEffect::getParameter(uint8_t index) const {
    if (index >= kParamCount) return nullptr;
    return &s_params[index];
}

bool SbK1BloomEffect::setParameter(const char* name, float value) {
    if (!name) return false;

    if (std::strcmp(name, "mood") == 0) {
        m_mood = fminf(fmaxf(value, 0.0f), 1.0f);
        return true;
    }
    if (std::strcmp(name, "contrast") == 0) {
        m_contrast = fminf(fmaxf(value, 0.0f), 3.0f);
        return true;
    }
    if (std::strcmp(name, "saturation") == 0) {
        m_saturation = fminf(fmaxf(value, 0.0f), 1.0f);
        return true;
    }
    if (std::strcmp(name, "chromaHue") == 0) {
        m_chromaHue = fminf(fmaxf(value, 0.0f), 1.0f);
        return true;
    }
    if (std::strcmp(name, "incandescent") == 0) {
        m_incandescent = fminf(fmaxf(value, 0.0f), 1.0f);
        return true;
    }
    if (std::strcmp(name, "prismCount") == 0) {
        m_prismCount = fminf(fmaxf(value, 0.0f), 5.0f);
        return true;
    }
    if (std::strcmp(name, "bulbOpacity") == 0) {
        m_bulbOpacity = fminf(fmaxf(value, 0.0f), 1.0f);
        return true;
    }
    return false;
}

float SbK1BloomEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;

    if (std::strcmp(name, "mood") == 0)         return m_mood;
    if (std::strcmp(name, "contrast") == 0)     return m_contrast;
    if (std::strcmp(name, "saturation") == 0)   return m_saturation;
    if (std::strcmp(name, "chromaHue") == 0)    return m_chromaHue;
    if (std::strcmp(name, "incandescent") == 0) return m_incandescent;
    if (std::strcmp(name, "prismCount") == 0)  return m_prismCount;
    if (std::strcmp(name, "bulbOpacity") == 0) return m_bulbOpacity;
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
