/**
 * @file SbK1BloomEffect.cpp
 * @brief Canonical SB 4.1.1 light_mode_bloom port — Phase 5B PoC.
 *
 * Algorithm: see SbK1BloomEffect.h for the full canonical step list.
 *
 * Phase 5B uses V1's existing drawSprite() static method (CRGB_F port of
 * SB led_utilities.h:1247-1290) for the scroll. CRGB_F precision preserves
 * sub-byte trail propagation — the Phase 5 PoC failed because the CRGB
 * uint8 substrate truncated sub-1.0 values to zero after ~4 LEDs.
 *
 * Single PoC modification vs verbatim canonical SB: chroma input peak
 * normalisation before colour synthesis. K1 ESV11 m_chromaSmooth is RAW
 * (no max_peak tracker like SB's make_smooth_chromagram). Without input
 * scale repair, K1 chroma values 0.05–0.3 produce bin² × 1/6 ≈ 0.015
 * contributions and visibly dim bloom. The normalisation scales the
 * strongest bin to 1.0, matching SB's input scale. This is INPUT scale
 * repair — NOT post-sum totalMag normalisation (K1-team drift now removed).
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
#include "effects/PersistenceHelpers.h"
using lightwaveos::effects::persistence::fadeToBlackByDt;

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
        fadeToBlackByDt(ctx.leds, ctx.ledCount, 32, ctx.getSafeDeltaSeconds());
        return;
    }

    CRGB_F* workBuf = m_bloom->workBuffer;
    CRGB_F* prevBuf = m_bloom->prevBuffer;

    // ─────────────────────────────────────────────────────────────────
    // Canonical SB 4.1.1 light_mode_bloom port (lightshow_modes.h:398-499).
    // CRGB_F throughout. Frame-coupled timing (matches SB's fixed-rate
    // semantics; not dt-corrected for this PoC).
    // ─────────────────────────────────────────────────────────────────

    // Step 1: Clear working buffer (SB: memset leds_16, 0, 128 × CRGB16).
    std::memset(workBuf, 0, kStripLen * sizeof(CRGB_F));

    // Step 2: Sub-pixel additive scroll prevBuf → workBuf rightward.
    //   SB: draw_sprite(leds, leds_prev, 128, 128, 0.25 + 1.75*MOOD, 0.99)
    //   Uses V1's drawSprite() — CRGB_F port of SB led_utilities.h:1247-1290.
    //   Unidirectional rightward shift; left half is overwritten by mirror
    //   at step 11. Scroll position is fixed per frame (NOT audio-modulated).
    {
        const float scrollPosition = 0.25f + 1.75f * m_mood;
        drawSprite(workBuf, prevBuf, kStripLen, kStripLen,
                   scrollPosition, 0.99f);
    }

    // Step 3a: Chroma input peak normalisation (PoC modification).
    //   K1 ESV11 m_chromaSmooth is RAW chroma (NO max_peak tracker like
    //   SB's make_smooth_chromagram). K1 typical values 0.05–0.3 produce
    //   visibly dim bloom under SB's verbatim formula. Local peak-normalise
    //   to scale the strongest bin to 1.0, matching SB's input scale.
    //   Floor at 0.05 prevents amplifying noise during silence.
    float peakChroma = 0.0f;
    for (uint8_t c = 0; c < 12; ++c) {
        if (m_chromaSmooth[c] > peakChroma) peakChroma = m_chromaSmooth[c];
    }
    if (peakChroma < 0.05f) peakChroma = 0.05f;  // silence floor
    const float invPeak = 1.0f / peakChroma;

    // Step 3b: Chromagram colour synthesis (canonical SB).
    //   SB: sum_color += hsv(i/12, SAT, chroma[i]² × 1/6) for each of 12 bins
    //   NO threshold gate. NO totalMag normalisation. NO fallback colour.
    //   bin uses peak-normalised value from step 3a.
    constexpr float kBloomShare = 1.0f / 6.0f;
    const bool chromaticMode = (ctx.saturation >= 128);

    CRGB_F bloomColor = {0.0f, 0.0f, 0.0f};
    for (uint8_t c = 0; c < 12; ++c) {
        const float bin = m_chromaSmooth[c] * invPeak;     // peak-normalised
        const float val = bin * bin * kBloomShare;          // SB: bin² × 1/6
        if (val <= 0.0f) continue;

        const float prog = c / 12.0f;
        // BLOOM-SPECIFIC: cyan offset (0.5)
        float palPos = prog + 0.5f;
        if (chromaticMode) palPos += m_huePosition;

        const CRGB_F noteColor = paletteColorF(ctx.palette, palPos, val);
        bloomColor += noteColor;
    }

    // Step 4: Clip per channel at 1.0 (SB canonical).
    bloomColor.clip();

    // Step 5: SQUARE_ITER post-sum squarings (SB iterative gain).
    //   m_contrast (0.0–3.0, default 1.0) reinterpreted as integer iter count.
    {
        const int squareIter = static_cast<int>(m_contrast);
        for (int s = 0; s < squareIter; ++s) {
            bloomColor.r *= bloomColor.r;
            bloomColor.g *= bloomColor.g;
            bloomColor.b *= bloomColor.b;
        }
    }

    // Step 6: force_saturation via HSV roundtrip (SB canonical).
    {
        CRGB tempRgb = bloomColor.toCRGB();
        CHSV tempHsv = rgb2hsv_approximate(tempRgb);
        tempHsv.s = ctx.saturation;
        hsv2rgb_rainbow(tempHsv, tempRgb);
        bloomColor = CRGB_F::fromCRGB(tempRgb);
    }

    // Step 7: force_hue in non-chromatic mode (SB canonical).
    if (!chromaticMode) {
        float maxComp = fmaxf(bloomColor.r, fmaxf(bloomColor.g, bloomColor.b));
        if (maxComp < 0.001f) maxComp = 0.001f;
        const float forcedPos = m_chromaHue + m_huePosition;
        bloomColor = paletteColorF(ctx.palette, forcedPos, fminf(maxComp, 1.0f));
    }

    // PHOTONS — master brightness scale.
    const float photons = static_cast<float>(ctx.brightness) / 255.0f;
    bloomColor *= photons;
    bloomColor.clip();

    // Step 8: Direct centre injection at LEDs 79+80 (SB canonical, NO EMA).
    workBuf[kCenterLeft]  = bloomColor;
    workBuf[kCenterRight] = bloomColor;

    // Step 9: Snapshot FULL workBuf → prevBuf BEFORE edge fade.
    //   SB: memcpy(leds_16_prev, leds_16, sizeof) — full snapshot.
    //   Critical: edge fade and mirror are OUTPUT-ONLY transforms; the
    //   scroll state must remain un-faded so the next frame's drawSprite
    //   reads from a clean source.
    std::memcpy(prevBuf, workBuf, kStripLen * sizeof(CRGB_F));

    // Step 10: Quadratic edge fade on outer 40 LEDs of right half.
    //   SB: for (i=0..31) leds_16[127-i] *= (i/31)²  — 32 of 64 right-half LEDs.
    //   K1 geometric scale: 50% of right half = kHalf/2 = 40 LEDs (120..159).
    //   LED 159 (i=0)  → fade=0  (zeroed)
    //   LED 120 (i=39) → fade=1  (unchanged)
    {
        constexpr uint16_t kEdgeFadeCount = kHalf / 2;  // 40
        for (uint16_t i = 0; i < kEdgeFadeCount; ++i) {
            const float prog = static_cast<float>(i)
                             / static_cast<float>(kEdgeFadeCount - 1);
            const float fade = prog * prog;  // quadratic
            workBuf[kStripLen - 1 - i] *= fade;
        }
    }

    // Step 11: Mirror right half (80..159) to left half (79..0).
    //   SB: for (i=0..63) leds_16[i] = leds_16[127-i].
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
