/**
 * @file SbK1BloomV2Effect.cpp
 * @brief Faithful port of K1's bloom_algo_center_scroll (algorithm 0)
 *
 * Ported line-by-line from K1.Lightwave lightshow_modes.h:593-655.
 * See SbK1BloomV2Effect.h for algorithm description.
 */

#include "SbK1BloomV2Effect.h"

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

const plugins::EffectParameter SbK1BloomV2Effect::s_params[kParamCount] = {
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

bool SbK1BloomV2Effect::init(plugins::EffectContext& ctx) {
    if (!SbK1BaseEffect::init(ctx)) return false;

#ifndef NATIVE_BUILD
    if (!m_ps2) {
        m_ps2 = static_cast<BloomV2Psram*>(
            heap_caps_malloc(sizeof(BloomV2Psram), MALLOC_CAP_SPIRAM));
        if (!m_ps2) {
            SbK1BaseEffect::cleanup();
            return false;
        }
    }
    std::memset(m_ps2, 0, sizeof(BloomV2Psram));
#endif

    m_iter = 0;

    // Reset parameters to defaults
    m_mood         = 0.16f;
    m_contrast     = 1.0f;
    m_saturation   = 1.0f;
    m_chromaHue    = 0.0f;
    m_incandescent = 0.0f;
    m_prismCount   = 1.42f;
    m_bulbOpacity  = 0.0f;
    m_localPeak    = 0.01f;

    return true;
}

void SbK1BloomV2Effect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps2) {
        heap_caps_free(m_ps2);
        m_ps2 = nullptr;
    }
#endif
    SbK1BaseEffect::cleanup();
}

// =========================================================================
// Prism transform: scale_image_to_half -> shift_leds_up -> mirror_downwards
// Exact port of K1's apply_prism_effect inner transform chain.
// =========================================================================

void SbK1BloomV2Effect::prismTransform(CRGB_F* buf, CRGB_F* tmp) {
    // scale_image_to_half: average adjacent pairs into first 80 pixels
    for (uint16_t i = 0; i < kHalf; ++i) {
        tmp[i].r = buf[i * 2].r * 0.5f + buf[i * 2 + 1].r * 0.5f;
        tmp[i].g = buf[i * 2].g * 0.5f + buf[i * 2 + 1].g * 0.5f;
        tmp[i].b = buf[i * 2].b * 0.5f + buf[i * 2 + 1].b * 0.5f;
    }
    std::memset(&tmp[kHalf], 0, kHalf * sizeof(CRGB_F));
    std::memcpy(buf, tmp, kStripLen * sizeof(CRGB_F));

    // shift_leds_up(half): move content up by half, clear bottom
    std::memcpy(tmp, buf, kStripLen * sizeof(CRGB_F));
    std::memcpy(&buf[kHalf], tmp, kHalf * sizeof(CRGB_F));
    std::memset(buf, 0, kHalf * sizeof(CRGB_F));

    // mirror_image_downwards: upper half mirrors to lower half
    for (uint16_t i = 0; i < kHalf; ++i) {
        tmp[kHalf + i] = buf[kHalf + i];
        tmp[kHalf - 1 - i] = buf[kHalf + i];
    }
    std::memcpy(buf, tmp, kStripLen * sizeof(CRGB_F));
}

// =========================================================================
// Render — EXACT port of K1's bloom_algo_center_scroll + post-processing
// =========================================================================

void SbK1BloomV2Effect::renderEffect(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps2) return;
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

    CRGB_F* workBuf    = m_ps2->workBuffer;
    CRGB_F* scrollBuf  = m_ps2->scrollState;
    CRGB_F* auxBuf     = m_ps2->auxBuffer;
    CRGB_F* fxBuf      = m_ps2->fxBuffer;

    m_iter++;

    // =====================================================================
    // K1 parity: alternating frames. Only update on EVEN frames.
    // K1: if (bitRead(iter, 0) == 0) { ... } else { memcpy from aux }
    // =====================================================================
    if ((m_iter & 1) == 0) {

        // -----------------------------------------------------------------
        // calc_bloom_intensity: 3 selectable energy modes.
        // Mode 0 "Mean":    Average of 12 contrast-applied bins (original K1 parity)
        // Mode 1 "Max Bin": Single strongest bin drives intensity (more dynamic)
        // Mode 2 "Re-norm": Local fast-decay peak tracker re-stretches compressed data
        // -----------------------------------------------------------------
        float totalEnergy = 0.0f;

        if (m_energyMode == BloomEnergyMode::MAX_BIN) {
            // MODE 1: Max Bin — use the single strongest bin after contrast
            float maxVal = 0.0f;
            for (int c = 0; c < 12; ++c) {
                float bin = applyContrast(m_chromaSmooth[c], m_contrast);
                if (bin > maxVal) maxVal = bin;
            }
            totalEnergy = maxVal;
        } else if (m_energyMode == BloomEnergyMode::RE_NORM) {
            // MODE 2: Re-norm — fast-decay peak tracker to re-stretch dynamic range
            float maxRaw = 0.0f;
            for (int c = 0; c < 12; ++c) {
                if (m_chromaSmooth[c] > maxRaw) maxRaw = m_chromaSmooth[c];
            }
            // Fast attack, moderate decay (0.95 vs SB side-car's 0.999)
            if (maxRaw > m_localPeak) {
                m_localPeak = maxRaw;
            } else {
                m_localPeak *= 0.95f;
                if (m_localPeak < 0.01f) m_localPeak = 0.01f;
            }
            // Re-normalize all bins against local peak, then average with contrast
            for (int c = 0; c < 12; ++c) {
                float bin = m_chromaSmooth[c] / m_localPeak;
                bin = applyContrast(bin, m_contrast);
                totalEnergy += bin;
            }
            totalEnergy /= 12.0f;
        } else {
            // MODE 0: Mean — original K1 parity (average of 12 bins)
            for (int c = 0; c < 12; ++c) {
                float bin = applyContrast(m_chromaSmooth[c], m_contrast);
                totalEnergy += bin;
            }
            totalEnergy /= 12.0f;
        }
        if (totalEnergy > 1.0f) totalEnergy = 1.0f;

        // Grayscale pixel: r=g=b=intensity (K1 parity: CRGB16 gray = {intensity, intensity, intensity})
        CRGB_F gray = {totalEnergy, totalEnergy, totalEnergy};

        // -----------------------------------------------------------------
        // Integer scroll: shift right half outward by 1 or 2 pixels
        // K1: reads from bloom_scroll (our scrollBuf), writes to leds_16 (our workBuf)
        // -----------------------------------------------------------------
        const bool fast = (m_mood > 0.5f);

        if (fast) {
            // K1: for j = N-1 down to half+2: leds[j] = bloom_scroll[j-2]
            for (int j = kStripLen - 1; j >= (int)(kHalf + 2); --j) {
                workBuf[j] = scrollBuf[j - 2];
            }
            workBuf[kHalf]     = gray;
            workBuf[kHalf + 1] = gray;
        } else {
            // K1: for j = N-1 down to half+1: leds[j] = bloom_scroll[j-1]
            for (int j = kStripLen - 1; j >= (int)(kHalf + 1); --j) {
                workBuf[j] = scrollBuf[j - 1];
            }
            workBuf[kHalf] = gray;
        }

        // -----------------------------------------------------------------
        // Save undistorted right half back to scroll state with decay
        // Decay creates visible separation between color bands — without it
        // the trail is a solid sheet of uniform brightness.
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            scrollBuf[kHalf + i] = workBuf[kHalf + i] * 0.995f;
        }

        // -----------------------------------------------------------------
        // Sqrt distortion on right half (DISPLAY ONLY, not saved to scroll state)
        // K1: prog = i/(half-1), prog_d = sqrt(prog), src = half + prog_d*(half-1)
        //     leds_fx[half+i] = lerp(src, leds)   (sub-pixel interpolation)
        //     Then copy leds_fx right half back to leds
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            float prog = (float)i / (float)(kHalf - 1);
            float prog_d = sqrtf(prog);
            float src = (float)kHalf + prog_d * (float)(kHalf - 1);
            if (src > (float)(kStripLen - 2)) src = (float)(kStripLen - 2);

            // Sub-pixel linear interpolation (K1's lerp_led_16)
            int srcLow = (int)src;
            float frac = src - (float)srcLow;
            int srcHigh = srcLow + 1;

            fxBuf[kHalf + i].r = workBuf[srcLow].r * (1.0f - frac) + workBuf[srcHigh].r * frac;
            fxBuf[kHalf + i].g = workBuf[srcLow].g * (1.0f - frac) + workBuf[srcHigh].g * frac;
            fxBuf[kHalf + i].b = workBuf[srcLow].b * (1.0f - frac) + workBuf[srcHigh].b * frac;
        }
        std::memcpy(&workBuf[kHalf], &fxBuf[kHalf], kHalf * sizeof(CRGB_F));

        // -----------------------------------------------------------------
        // Linear edge fade: center=bright, edge=dark
        // K1: fade = (half-1-i) / (half-1)
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            float fade = (float)(kHalf - 1 - i) / (float)(kHalf - 1);
            workBuf[kHalf + i] *= fade;
        }

        // -----------------------------------------------------------------
        // Mirror right half to left half
        // K1: mirror_image_downwards(leds_16)
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            workBuf[kHalf - 1 - i] = workBuf[kHalf + i];
        }

        // -----------------------------------------------------------------
        // bloom_apply_color: apply colour to grayscale transport at output
        // K1: finds dominant chroma bin for hue, then for each pixel:
        //   - palette mode: spatial+intensity position -> palette lookup
        //   - chromatic mode: note_hue + hue_position -> hsv
        //   - non-chromatic: CONFIG.CHROMA + hue_position -> hsv
        // We use the base class chromagram and hue position.
        // -----------------------------------------------------------------
        {
            // Find dominant chromagram bin (K1 parity)
            float maxBin = 0.0f;
            uint8_t dominant = 0;
            for (uint8_t c = 0; c < 12; ++c) {
                if (m_chromaSmooth[c] > maxBin) {
                    maxBin = m_chromaSmooth[c];
                    dominant = c;
                }
            }

            const bool chromaticMode = (ctx.saturation >= 128);

            for (uint16_t i = 0; i < kStripLen; ++i) {
                // Extract intensity from grayscale (all channels equal)
                float bright = workBuf[i].r;
                if (bright < 0.001f) continue; // Skip black pixels

                if (ctx.palette.isValid()) {
                    // Palette mode: use distance from center for symmetry
                    float distFromCenter = fabsf((float)i - ((float)kHalf - 0.5f)) / (float)kHalf;
                    float palPos = (distFromCenter * 200.0f + bright * 55.0f) / 255.0f;
                    workBuf[i] = paletteColorF(ctx.palette, palPos, bright);
                } else {
                    float noteHue;
                    if (chromaticMode) {
                        // K1: note_colors[dominant*8] + hue_position
                        noteHue = kNoteHues[dominant] + m_huePosition;
                        if (noteHue > 1.0f) noteHue -= 1.0f;
                    } else {
                        // K1: CONFIG.CHROMA + hue_position
                        noteHue = m_chromaHue + m_huePosition;
                        if (noteHue > 1.0f) noteHue -= 1.0f;
                        if (noteHue < 0.0f) noteHue += 1.0f;
                    }
                    workBuf[i] = hsvToRgbF(noteHue, m_saturation, bright);
                }
            }
        }

        // -----------------------------------------------------------------
        // Save rendered frame to aux for odd-frame replay
        // K1: memcpy(bloom_aux, leds_16, sizeof(CRGB16) * NATIVE_RESOLUTION)
        // -----------------------------------------------------------------
        std::memcpy(auxBuf, workBuf, kStripLen * sizeof(CRGB_F));

    } else {
        // -----------------------------------------------------------------
        // Odd frame: replay last rendered frame
        // K1: memcpy(leds_16, bloom_aux, sizeof(CRGB16) * NATIVE_RESOLUTION)
        // -----------------------------------------------------------------
        std::memcpy(workBuf, auxBuf, kStripLen * sizeof(CRGB_F));
    }

    // =====================================================================
    // POST-PROCESSING (applied every frame, after bloom returns)
    // K1 main.cpp lines 558-564: prism + bulb after effect function
    // =====================================================================

    // -----------------------------------------------------------------
    // Prism effect: K1's apply_prism_effect(PRISM_COUNT, 0.25)
    // For each iteration: copy -> scale_to_half -> shift_up(half) ->
    //   mirror_downwards -> hue shift -> additive blend at 0.25 opacity
    // -----------------------------------------------------------------
    if (m_prismCount > 0.01f) {
        CRGB_F* prismFx  = m_ps2->fxBuffer;
        CRGB_F* prismTmp = m_ps2->tmpBuffer;

        const uint8_t wholeIter = (uint8_t)m_prismCount;
        const float fractIter = m_prismCount - floorf(m_prismCount);

        for (uint8_t it = 0; it < wholeIter; ++it) {
            std::memcpy(prismFx, workBuf, kStripLen * sizeof(CRGB_F));
            prismTransform(prismFx, prismTmp);

            // K1: hue_shift = i * 0.05 per iteration
            const float hueShift = it * 0.05f;
#ifndef NATIVE_BUILD
            if (hueShift > 0.001f) {
                const uint8_t hShift8 = (uint8_t)(hueShift * 255.0f);
                for (uint16_t j = 0; j < kStripLen; ++j) {
                    if (prismFx[j].r > 0.002f || prismFx[j].g > 0.002f || prismFx[j].b > 0.002f) {
                        CRGB rgb = prismFx[j].toCRGB();
                        CHSV hsv = rgb2hsv_approximate(rgb);
                        hsv.h += hShift8;
                        hsv2rgb_rainbow(hsv, rgb);
                        prismFx[j] = CRGB_F::fromCRGB(rgb);
                    }
                }
            }
#endif
            // Additive blend at 0.25 opacity (K1 parity)
            for (uint16_t j = 0; j < kStripLen; ++j) {
                workBuf[j].r += prismFx[j].r * 0.25f;
                workBuf[j].g += prismFx[j].g * 0.25f;
                workBuf[j].b += prismFx[j].b * 0.25f;
            }
        }

        // Fractional iteration at reduced opacity
        if (fractIter > 0.01f) {
            std::memcpy(prismFx, workBuf, kStripLen * sizeof(CRGB_F));
            prismTransform(prismFx, prismTmp);

            // K1: hue shift for fractional = wholeIter * 0.05
            const float hueShift = wholeIter * 0.05f;
#ifndef NATIVE_BUILD
            if (hueShift > 0.001f) {
                const uint8_t hShift8 = (uint8_t)(hueShift * 255.0f);
                for (uint16_t j = 0; j < kStripLen; ++j) {
                    if (prismFx[j].r > 0.002f || prismFx[j].g > 0.002f || prismFx[j].b > 0.002f) {
                        CRGB rgb = prismFx[j].toCRGB();
                        CHSV hsv = rgb2hsv_approximate(rgb);
                        hsv.h += hShift8;
                        hsv2rgb_rainbow(hsv, rgb);
                        prismFx[j] = CRGB_F::fromCRGB(rgb);
                    }
                }
            }
#endif
            const float fractOpacity = 0.25f * fractIter;
            for (uint16_t j = 0; j < kStripLen; ++j) {
                workBuf[j].r += prismFx[j].r * fractOpacity;
                workBuf[j].g += prismFx[j].g * fractOpacity;
                workBuf[j].b += prismFx[j].b * fractOpacity;
            }
        }
    }

    // -----------------------------------------------------------------
    // Bulb cover: K1's render_bulb_cover()
    // 4-LED repeating pattern [0.25, 1.0, 0.25, 0.0] with opacity blend
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
    // Output: convert to ctx.leds and copy to strip 2
    // -----------------------------------------------------------------
    const uint16_t ledCount = ctx.ledCount;
    for (uint16_t i = 0; i < kStripLen && i < ledCount; ++i) {
        workBuf[i].clip();
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

const plugins::EffectMetadata& SbK1BloomV2Effect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "K1 Bloom V2",
        "K1 Lightwave bloom algo 0 (center scroll, faithful port)",
        plugins::EffectCategory::PARTY,
        1,
        "K1.Lightwave"
    };
    return meta;
}

// =========================================================================
// Parameter interface
// =========================================================================

uint8_t SbK1BloomV2Effect::getParameterCount() const {
    return kParamCount;
}

const plugins::EffectParameter* SbK1BloomV2Effect::getParameter(uint8_t index) const {
    if (index >= kParamCount) return nullptr;
    return &s_params[index];
}

bool SbK1BloomV2Effect::setParameter(const char* name, float value) {
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

float SbK1BloomV2Effect::getParameter(const char* name) const {
    if (!name) return 0.0f;

    if (std::strcmp(name, "mood") == 0)         return m_mood;
    if (std::strcmp(name, "contrast") == 0)     return m_contrast;
    if (std::strcmp(name, "saturation") == 0)   return m_saturation;
    if (std::strcmp(name, "chromaHue") == 0)    return m_chromaHue;
    if (std::strcmp(name, "incandescent") == 0) return m_incandescent;
    if (std::strcmp(name, "prismCount") == 0)   return m_prismCount;
    if (std::strcmp(name, "bulbOpacity") == 0)  return m_bulbOpacity;
    return 0.0f;
}

// =========================================================================
// Variant metadata
// =========================================================================

const plugins::EffectMetadata& SbK1BloomV2MaxBinEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "K1 Bloom MaxBin",
        "K1 Bloom V2 — max bin energy (single strongest chroma bin)",
        plugins::EffectCategory::PARTY,
        1,
        "K1.Lightwave"
    };
    return meta;
}

const plugins::EffectMetadata& SbK1BloomV2ReNormEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "K1 Bloom ReNorm",
        "K1 Bloom V2 — re-normalized energy (fast-decay local peak tracker)",
        plugins::EffectCategory::PARTY,
        1,
        "K1.Lightwave"
    };
    return meta;
}

// =========================================================================
// SbK1BloomV2BeatPulseEffect — onset-gated bloom variant
// =========================================================================

// Parameter descriptors: base 7 params + beatThreshold
const plugins::EffectParameter SbK1BloomV2BeatPulseEffect::s_beatPulseParams[kBeatPulseParamCount] = {
    {"mood",          "Scroll Speed",    0.0f, 1.0f, 0.16f, plugins::EffectParameterType::FLOAT, 0.01f, "animation", "",  false},
    {"contrast",      "Contrast",        0.0f, 3.0f, 1.0f,  plugins::EffectParameterType::FLOAT, 0.25f, "visual",    "x", false},
    {"saturation",    "Saturation",      0.0f, 1.0f, 1.0f,  plugins::EffectParameterType::FLOAT, 0.05f, "colour",    "",  false},
    {"chromaHue",     "Hue Offset",      0.0f, 1.0f, 0.0f,  plugins::EffectParameterType::FLOAT, 0.01f, "colour",    "",  false},
    {"incandescent",  "Warm Filter",     0.0f, 1.0f, 0.0f,  plugins::EffectParameterType::FLOAT, 0.05f, "colour",    "",  false},
    {"prismCount",    "Prism Layers",    0.0f, 5.0f, 1.42f, plugins::EffectParameterType::FLOAT, 0.1f,  "visual",    "",  false},
    {"bulbOpacity",   "Bulb Cover",      0.0f, 1.0f, 0.0f,  plugins::EffectParameterType::FLOAT, 0.05f, "visual",    "",  false},
    {"beatThreshold", "Beat Threshold",  0.0f, 1.0f, 0.15f, plugins::EffectParameterType::FLOAT, 0.05f, "animation", "",  false},
};

const plugins::EffectMetadata& SbK1BloomV2BeatPulseEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "K1 Bloom Beat Pulse",
        "K1 Bloom V2 — onset-gated injection (discrete blobs on transients)",
        plugins::EffectCategory::PARTY,
        1,
        "K1.Lightwave"
    };
    return meta;
}

// -------------------------------------------------------------------------
// Parameter interface (extends base with beatThreshold)
// -------------------------------------------------------------------------

uint8_t SbK1BloomV2BeatPulseEffect::getParameterCount() const {
    return kBeatPulseParamCount;
}

const plugins::EffectParameter* SbK1BloomV2BeatPulseEffect::getParameter(uint8_t index) const {
    if (index >= kBeatPulseParamCount) return nullptr;
    return &s_beatPulseParams[index];
}

bool SbK1BloomV2BeatPulseEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (std::strcmp(name, "beatThreshold") == 0) {
        m_beatThreshold = fminf(fmaxf(value, 0.0f), 1.0f);
        return true;
    }
    // Delegate the base 7 parameters to parent
    return SbK1BloomV2Effect::setParameter(name, value);
}

float SbK1BloomV2BeatPulseEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (std::strcmp(name, "beatThreshold") == 0) return m_beatThreshold;
    return SbK1BloomV2Effect::getParameter(name);
}

// -------------------------------------------------------------------------
// Render — same as BloomV2 but with novelty-gated center pixel injection
// -------------------------------------------------------------------------

void SbK1BloomV2BeatPulseEffect::renderEffect(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps2) return;
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

    CRGB_F* workBuf    = m_ps2->workBuffer;
    CRGB_F* scrollBuf  = m_ps2->scrollState;
    CRGB_F* auxBuf     = m_ps2->auxBuffer;
    CRGB_F* fxBuf      = m_ps2->fxBuffer;

    m_iter++;

    // =====================================================================
    // Alternating frames — only update on EVEN frames (K1 parity)
    // =====================================================================
    if ((m_iter & 1) == 0) {

        // -----------------------------------------------------------------
        // BEAT PULSE: gate injection on novelty curve
        // -----------------------------------------------------------------
        const float noveltyNow = m_noveltyCurve[0];
        const bool beatActive = (noveltyNow >= m_beatThreshold);

        // Compute max-bin energy (used for brightness when beat IS active)
        float totalEnergy = 0.0f;
        if (beatActive) {
            float maxVal = 0.0f;
            for (int c = 0; c < 12; ++c) {
                float bin = applyContrast(m_chromaSmooth[c], m_contrast);
                if (bin > maxVal) maxVal = bin;
            }
            totalEnergy = maxVal;
            if (totalEnergy > 1.0f) totalEnergy = 1.0f;
        }
        // When !beatActive, totalEnergy stays 0 -> black center pixel

        CRGB_F gray = {totalEnergy, totalEnergy, totalEnergy};

        // -----------------------------------------------------------------
        // Integer scroll: shift right half outward by 1 or 2 pixels
        // -----------------------------------------------------------------
        const bool fast = (m_mood > 0.5f);

        if (fast) {
            for (int j = kStripLen - 1; j >= (int)(kHalf + 2); --j) {
                workBuf[j] = scrollBuf[j - 2];
            }
            workBuf[kHalf]     = gray;
            workBuf[kHalf + 1] = gray;
        } else {
            for (int j = kStripLen - 1; j >= (int)(kHalf + 1); --j) {
                workBuf[j] = scrollBuf[j - 1];
            }
            workBuf[kHalf] = gray;
        }

        // -----------------------------------------------------------------
        // Save undistorted right half back to scroll state with decay
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            scrollBuf[kHalf + i] = workBuf[kHalf + i] * 0.995f;
        }

        // -----------------------------------------------------------------
        // Sqrt distortion on right half (display only, not saved to scroll)
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            float prog = (float)i / (float)(kHalf - 1);
            float prog_d = sqrtf(prog);
            float src = (float)kHalf + prog_d * (float)(kHalf - 1);
            if (src > (float)(kStripLen - 2)) src = (float)(kStripLen - 2);

            int srcLow = (int)src;
            float frac = src - (float)srcLow;
            int srcHigh = srcLow + 1;

            fxBuf[kHalf + i].r = workBuf[srcLow].r * (1.0f - frac) + workBuf[srcHigh].r * frac;
            fxBuf[kHalf + i].g = workBuf[srcLow].g * (1.0f - frac) + workBuf[srcHigh].g * frac;
            fxBuf[kHalf + i].b = workBuf[srcLow].b * (1.0f - frac) + workBuf[srcHigh].b * frac;
        }
        std::memcpy(&workBuf[kHalf], &fxBuf[kHalf], kHalf * sizeof(CRGB_F));

        // -----------------------------------------------------------------
        // Linear edge fade
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            float fade = (float)(kHalf - 1 - i) / (float)(kHalf - 1);
            workBuf[kHalf + i] *= fade;
        }

        // -----------------------------------------------------------------
        // Mirror right half to left half
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            workBuf[kHalf - 1 - i] = workBuf[kHalf + i];
        }

        // -----------------------------------------------------------------
        // Apply colour to grayscale transport
        // -----------------------------------------------------------------
        {
            float maxBin = 0.0f;
            uint8_t dominant = 0;
            for (uint8_t c = 0; c < 12; ++c) {
                if (m_chromaSmooth[c] > maxBin) {
                    maxBin = m_chromaSmooth[c];
                    dominant = c;
                }
            }

            const bool chromaticMode = (ctx.saturation >= 128);

            for (uint16_t i = 0; i < kStripLen; ++i) {
                float bright = workBuf[i].r;
                if (bright < 0.001f) continue;

                if (ctx.palette.isValid()) {
                    float distFromCenter = fabsf((float)i - ((float)kHalf - 0.5f)) / (float)kHalf;
                    float palPos = (distFromCenter * 200.0f + bright * 55.0f) / 255.0f;
                    workBuf[i] = paletteColorF(ctx.palette, palPos, bright);
                } else {
                    float noteHue;
                    if (chromaticMode) {
                        noteHue = kNoteHues[dominant] + m_huePosition;
                        if (noteHue > 1.0f) noteHue -= 1.0f;
                    } else {
                        noteHue = m_chromaHue + m_huePosition;
                        if (noteHue > 1.0f) noteHue -= 1.0f;
                        if (noteHue < 0.0f) noteHue += 1.0f;
                    }
                    workBuf[i] = hsvToRgbF(noteHue, m_saturation, bright);
                }
            }
        }

        // Save rendered frame to aux for odd-frame replay
        std::memcpy(auxBuf, workBuf, kStripLen * sizeof(CRGB_F));

    } else {
        // Odd frame: replay last rendered frame
        std::memcpy(workBuf, auxBuf, kStripLen * sizeof(CRGB_F));
    }

    // =====================================================================
    // POST-PROCESSING (prism, bulb, incandescent) — identical to base
    // =====================================================================

    // Prism effect
    if (m_prismCount > 0.01f) {
        CRGB_F* prismFx  = m_ps2->fxBuffer;
        CRGB_F* prismTmp = m_ps2->tmpBuffer;

        const uint8_t wholeIter = (uint8_t)m_prismCount;
        const float fractIter = m_prismCount - floorf(m_prismCount);

        for (uint8_t it = 0; it < wholeIter; ++it) {
            std::memcpy(prismFx, workBuf, kStripLen * sizeof(CRGB_F));
            prismTransform(prismFx, prismTmp);

            const float hueShift = it * 0.05f;
#ifndef NATIVE_BUILD
            if (hueShift > 0.001f) {
                const uint8_t hShift8 = (uint8_t)(hueShift * 255.0f);
                for (uint16_t j = 0; j < kStripLen; ++j) {
                    if (prismFx[j].r > 0.002f || prismFx[j].g > 0.002f || prismFx[j].b > 0.002f) {
                        CRGB rgb = prismFx[j].toCRGB();
                        CHSV hsv = rgb2hsv_approximate(rgb);
                        hsv.h += hShift8;
                        hsv2rgb_rainbow(hsv, rgb);
                        prismFx[j] = CRGB_F::fromCRGB(rgb);
                    }
                }
            }
#endif
            for (uint16_t j = 0; j < kStripLen; ++j) {
                workBuf[j].r += prismFx[j].r * 0.25f;
                workBuf[j].g += prismFx[j].g * 0.25f;
                workBuf[j].b += prismFx[j].b * 0.25f;
            }
        }

        if (fractIter > 0.01f) {
            std::memcpy(prismFx, workBuf, kStripLen * sizeof(CRGB_F));
            prismTransform(prismFx, prismTmp);

            const float hueShift = wholeIter * 0.05f;
#ifndef NATIVE_BUILD
            if (hueShift > 0.001f) {
                const uint8_t hShift8 = (uint8_t)(hueShift * 255.0f);
                for (uint16_t j = 0; j < kStripLen; ++j) {
                    if (prismFx[j].r > 0.002f || prismFx[j].g > 0.002f || prismFx[j].b > 0.002f) {
                        CRGB rgb = prismFx[j].toCRGB();
                        CHSV hsv = rgb2hsv_approximate(rgb);
                        hsv.h += hShift8;
                        hsv2rgb_rainbow(hsv, rgb);
                        prismFx[j] = CRGB_F::fromCRGB(rgb);
                    }
                }
            }
#endif
            const float fractOpacity = 0.25f * fractIter;
            for (uint16_t j = 0; j < kStripLen; ++j) {
                workBuf[j].r += prismFx[j].r * fractOpacity;
                workBuf[j].g += prismFx[j].g * fractOpacity;
                workBuf[j].b += prismFx[j].b * fractOpacity;
            }
        }
    }

    // Bulb cover
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

    // Incandescent warm-white filter
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

    // Output: convert to ctx.leds and mirror to strip 2
    const uint16_t ledCount = ctx.ledCount;
    for (uint16_t i = 0; i < kStripLen && i < ledCount; ++i) {
        workBuf[i].clip();
        ctx.leds[i] = workBuf[i].toCRGB();
    }
    for (uint16_t i = 0; i < kStripLen && (kStripLen + i) < ledCount; ++i) {
        ctx.leds[kStripLen + i] = ctx.leds[i];
    }

#endif // FEATURE_AUDIO_SYNC
#endif // NATIVE_BUILD
}

// =========================================================================
// SbK1BloomV2ColorHistoryEffect — full-color spectrogram scroll
//
// Key difference from base BloomV2:
//   - Center pixel is injected as FULL COLOR (not grayscale)
//   - Scroll buffer stores full color — no bloom_apply_color pass at output
//   - The strip becomes a time-history of what notes were playing
// =========================================================================

const plugins::EffectMetadata& SbK1BloomV2ColorHistoryEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "K1 Bloom Color History",
        "K1 Bloom V2 — scrolling spectrogram (full-color injection at center)",
        plugins::EffectCategory::PARTY,
        1,
        "K1.Lightwave"
    };
    return meta;
}

void SbK1BloomV2ColorHistoryEffect::renderEffect(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps2) return;
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

    CRGB_F* workBuf    = m_ps2->workBuffer;
    CRGB_F* scrollBuf  = m_ps2->scrollState;
    CRGB_F* auxBuf     = m_ps2->auxBuffer;
    CRGB_F* fxBuf      = m_ps2->fxBuffer;

    m_iter++;

    // =====================================================================
    // Alternating frames — only update on EVEN frames (K1 parity)
    // =====================================================================
    if ((m_iter & 1) == 0) {

        // -----------------------------------------------------------------
        // Find dominant chromagram bin and compute brightness (MaxBin mode)
        // -----------------------------------------------------------------
        float maxVal = 0.0f;
        uint8_t dominant = 0;
        for (int c = 0; c < 12; ++c) {
            float bin = applyContrast(m_chromaSmooth[c], m_contrast);
            if (bin > maxVal) {
                maxVal = bin;
                dominant = (uint8_t)c;
            }
        }
        float brightness = maxVal;
        if (brightness > 1.0f) brightness = 1.0f;

        // -----------------------------------------------------------------
        // KEY DIFFERENCE: inject FULL COLOR at center (not grayscale).
        // Convert dominant note to color, scaled by brightness.
        // -----------------------------------------------------------------
        CRGB_F centerPixel;
        if (brightness < 0.001f) {
            centerPixel = {0.0f, 0.0f, 0.0f};
        } else if (ctx.palette.isValid()) {
            // Palette mode: map dominant note to palette position
            float palPos = (float)dominant / 12.0f;
            centerPixel = paletteColorF(ctx.palette, palPos, brightness);
        } else {
            const bool chromaticMode = (ctx.saturation >= 128);
            float noteHue;
            if (chromaticMode) {
                noteHue = kNoteHues[dominant] + m_huePosition;
                if (noteHue > 1.0f) noteHue -= 1.0f;
            } else {
                noteHue = m_chromaHue + m_huePosition;
                if (noteHue > 1.0f) noteHue -= 1.0f;
                if (noteHue < 0.0f) noteHue += 1.0f;
            }
            centerPixel = hsvToRgbF(noteHue, m_saturation, brightness);
        }

        // -----------------------------------------------------------------
        // Integer scroll: shift right half outward by 1 or 2 pixels
        // Same mechanism as base BloomV2, but scrolling full-color pixels
        // -----------------------------------------------------------------
        const bool fast = (m_mood > 0.5f);

        if (fast) {
            for (int j = kStripLen - 1; j >= (int)(kHalf + 2); --j) {
                workBuf[j] = scrollBuf[j - 2];
            }
            workBuf[kHalf]     = centerPixel;
            workBuf[kHalf + 1] = centerPixel;
        } else {
            for (int j = kStripLen - 1; j >= (int)(kHalf + 1); --j) {
                workBuf[j] = scrollBuf[j - 1];
            }
            workBuf[kHalf] = centerPixel;
        }

        // -----------------------------------------------------------------
        // Save undistorted right half back to scroll state with decay
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            scrollBuf[kHalf + i] = workBuf[kHalf + i] * 0.995f;
        }

        // -----------------------------------------------------------------
        // Sqrt distortion on right half (display only, not saved to scroll)
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            float prog = (float)i / (float)(kHalf - 1);
            float prog_d = sqrtf(prog);
            float src = (float)kHalf + prog_d * (float)(kHalf - 1);
            if (src > (float)(kStripLen - 2)) src = (float)(kStripLen - 2);

            int srcLow = (int)src;
            float frac = src - (float)srcLow;
            int srcHigh = srcLow + 1;

            fxBuf[kHalf + i].r = workBuf[srcLow].r * (1.0f - frac) + workBuf[srcHigh].r * frac;
            fxBuf[kHalf + i].g = workBuf[srcLow].g * (1.0f - frac) + workBuf[srcHigh].g * frac;
            fxBuf[kHalf + i].b = workBuf[srcLow].b * (1.0f - frac) + workBuf[srcHigh].b * frac;
        }
        std::memcpy(&workBuf[kHalf], &fxBuf[kHalf], kHalf * sizeof(CRGB_F));

        // -----------------------------------------------------------------
        // Linear edge fade
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            float fade = (float)(kHalf - 1 - i) / (float)(kHalf - 1);
            workBuf[kHalf + i] *= fade;
        }

        // -----------------------------------------------------------------
        // Mirror right half to left half
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            workBuf[kHalf - 1 - i] = workBuf[kHalf + i];
        }

        // -----------------------------------------------------------------
        // NO bloom_apply_color — scrolled pixels already carry full color
        // -----------------------------------------------------------------

        // Save rendered frame to aux for odd-frame replay
        std::memcpy(auxBuf, workBuf, kStripLen * sizeof(CRGB_F));

    } else {
        // Odd frame: replay last rendered frame
        std::memcpy(workBuf, auxBuf, kStripLen * sizeof(CRGB_F));
    }

    // =====================================================================
    // POST-PROCESSING (prism, bulb, incandescent) — identical to base
    // =====================================================================

    // Prism effect
    if (m_prismCount > 0.01f) {
        CRGB_F* prismFx  = m_ps2->fxBuffer;
        CRGB_F* prismTmp = m_ps2->tmpBuffer;

        const uint8_t wholeIter = (uint8_t)m_prismCount;
        const float fractIter = m_prismCount - floorf(m_prismCount);

        for (uint8_t it = 0; it < wholeIter; ++it) {
            std::memcpy(prismFx, workBuf, kStripLen * sizeof(CRGB_F));
            prismTransform(prismFx, prismTmp);

            const float hueShift = it * 0.05f;
#ifndef NATIVE_BUILD
            if (hueShift > 0.001f) {
                const uint8_t hShift8 = (uint8_t)(hueShift * 255.0f);
                for (uint16_t j = 0; j < kStripLen; ++j) {
                    if (prismFx[j].r > 0.002f || prismFx[j].g > 0.002f || prismFx[j].b > 0.002f) {
                        CRGB rgb = prismFx[j].toCRGB();
                        CHSV hsv = rgb2hsv_approximate(rgb);
                        hsv.h += hShift8;
                        hsv2rgb_rainbow(hsv, rgb);
                        prismFx[j] = CRGB_F::fromCRGB(rgb);
                    }
                }
            }
#endif
            for (uint16_t j = 0; j < kStripLen; ++j) {
                workBuf[j].r += prismFx[j].r * 0.25f;
                workBuf[j].g += prismFx[j].g * 0.25f;
                workBuf[j].b += prismFx[j].b * 0.25f;
            }
        }

        if (fractIter > 0.01f) {
            std::memcpy(prismFx, workBuf, kStripLen * sizeof(CRGB_F));
            prismTransform(prismFx, prismTmp);

            const float hueShift = wholeIter * 0.05f;
#ifndef NATIVE_BUILD
            if (hueShift > 0.001f) {
                const uint8_t hShift8 = (uint8_t)(hueShift * 255.0f);
                for (uint16_t j = 0; j < kStripLen; ++j) {
                    if (prismFx[j].r > 0.002f || prismFx[j].g > 0.002f || prismFx[j].b > 0.002f) {
                        CRGB rgb = prismFx[j].toCRGB();
                        CHSV hsv = rgb2hsv_approximate(rgb);
                        hsv.h += hShift8;
                        hsv2rgb_rainbow(hsv, rgb);
                        prismFx[j] = CRGB_F::fromCRGB(rgb);
                    }
                }
            }
#endif
            const float fractOpacity = 0.25f * fractIter;
            for (uint16_t j = 0; j < kStripLen; ++j) {
                workBuf[j].r += prismFx[j].r * fractOpacity;
                workBuf[j].g += prismFx[j].g * fractOpacity;
                workBuf[j].b += prismFx[j].b * fractOpacity;
            }
        }
    }

    // Bulb cover
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

    // Incandescent warm-white filter
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

    // Output: convert to ctx.leds and mirror to strip 2
    const uint16_t ledCount = ctx.ledCount;
    for (uint16_t i = 0; i < kStripLen && i < ledCount; ++i) {
        workBuf[i].clip();
        ctx.leds[i] = workBuf[i].toCRGB();
    }
    for (uint16_t i = 0; i < kStripLen && (kStripLen + i) < ledCount; ++i) {
        ctx.leds[kStripLen + i] = ctx.leds[i];
    }

#endif // FEATURE_AUDIO_SYNC
#endif // NATIVE_BUILD
}

// =========================================================================
// SbK1BloomV2SpectralDeltaEffect — delta-driven bloom variant
// =========================================================================

bool SbK1BloomV2SpectralDeltaEffect::init(plugins::EffectContext& ctx) {
    if (!SbK1BloomV2Effect::init(ctx)) return false;
    std::memset(m_prevChroma, 0, sizeof(m_prevChroma));
    return true;
}

const plugins::EffectMetadata& SbK1BloomV2SpectralDeltaEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "K1 Bloom Spectral Delta",
        "K1 Bloom V2 — sparkle on change (onset-only, sustained notes invisible)",
        plugins::EffectCategory::PARTY,
        1,
        "K1.Lightwave"
    };
    return meta;
}

// -------------------------------------------------------------------------
// Render — same scroll/mirror/post-processing as BloomV2 but energy is
// driven by frame-to-frame positive change in chromagram bins.
// -------------------------------------------------------------------------

void SbK1BloomV2SpectralDeltaEffect::renderEffect(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps2) return;
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

    CRGB_F* workBuf    = m_ps2->workBuffer;
    CRGB_F* scrollBuf  = m_ps2->scrollState;
    CRGB_F* auxBuf     = m_ps2->auxBuffer;
    CRGB_F* fxBuf      = m_ps2->fxBuffer;

    m_iter++;

    // =====================================================================
    // Alternating frames — only update on EVEN frames (K1 parity)
    // =====================================================================
    if ((m_iter & 1) == 0) {

        // -----------------------------------------------------------------
        // SPECTRAL DELTA: intensity from frame-to-frame positive change.
        // Only onsets/transitions produce output; sustained notes go dark.
        // -----------------------------------------------------------------
        float maxDelta = 0.0f;
        for (int c = 0; c < 12; ++c) {
            float delta = m_chromaSmooth[c] - m_prevChroma[c];
            if (delta > maxDelta) maxDelta = delta;
        }

        // Save current chromagram for next frame's delta computation
        std::memcpy(m_prevChroma, m_chromaSmooth, sizeof(m_prevChroma));

        // Apply contrast to the delta value
        float totalEnergy = applyContrast(maxDelta, m_contrast);
        if (totalEnergy > 1.0f) totalEnergy = 1.0f;

        CRGB_F gray = {totalEnergy, totalEnergy, totalEnergy};

        // -----------------------------------------------------------------
        // Integer scroll: shift right half outward by 1 or 2 pixels
        // -----------------------------------------------------------------
        const bool fast = (m_mood > 0.5f);

        if (fast) {
            for (int j = kStripLen - 1; j >= (int)(kHalf + 2); --j) {
                workBuf[j] = scrollBuf[j - 2];
            }
            workBuf[kHalf]     = gray;
            workBuf[kHalf + 1] = gray;
        } else {
            for (int j = kStripLen - 1; j >= (int)(kHalf + 1); --j) {
                workBuf[j] = scrollBuf[j - 1];
            }
            workBuf[kHalf] = gray;
        }

        // -----------------------------------------------------------------
        // Save undistorted right half back to scroll state with decay
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            scrollBuf[kHalf + i] = workBuf[kHalf + i] * 0.995f;
        }

        // -----------------------------------------------------------------
        // Sqrt distortion on right half (display only, not saved to scroll)
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            float prog = (float)i / (float)(kHalf - 1);
            float prog_d = sqrtf(prog);
            float src = (float)kHalf + prog_d * (float)(kHalf - 1);
            if (src > (float)(kStripLen - 2)) src = (float)(kStripLen - 2);

            int srcLow = (int)src;
            float frac = src - (float)srcLow;
            int srcHigh = srcLow + 1;

            fxBuf[kHalf + i].r = workBuf[srcLow].r * (1.0f - frac) + workBuf[srcHigh].r * frac;
            fxBuf[kHalf + i].g = workBuf[srcLow].g * (1.0f - frac) + workBuf[srcHigh].g * frac;
            fxBuf[kHalf + i].b = workBuf[srcLow].b * (1.0f - frac) + workBuf[srcHigh].b * frac;
        }
        std::memcpy(&workBuf[kHalf], &fxBuf[kHalf], kHalf * sizeof(CRGB_F));

        // -----------------------------------------------------------------
        // Linear edge fade
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            float fade = (float)(kHalf - 1 - i) / (float)(kHalf - 1);
            workBuf[kHalf + i] *= fade;
        }

        // -----------------------------------------------------------------
        // Mirror right half to left half
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            workBuf[kHalf - 1 - i] = workBuf[kHalf + i];
        }

        // -----------------------------------------------------------------
        // Apply colour to grayscale transport
        // -----------------------------------------------------------------
        {
            float maxBin = 0.0f;
            uint8_t dominant = 0;
            for (uint8_t c = 0; c < 12; ++c) {
                if (m_chromaSmooth[c] > maxBin) {
                    maxBin = m_chromaSmooth[c];
                    dominant = c;
                }
            }

            const bool chromaticMode = (ctx.saturation >= 128);

            for (uint16_t i = 0; i < kStripLen; ++i) {
                float bright = workBuf[i].r;
                if (bright < 0.001f) continue;

                if (ctx.palette.isValid()) {
                    float distFromCenter = fabsf((float)i - ((float)kHalf - 0.5f)) / (float)kHalf;
                    float palPos = (distFromCenter * 200.0f + bright * 55.0f) / 255.0f;
                    workBuf[i] = paletteColorF(ctx.palette, palPos, bright);
                } else {
                    float noteHue;
                    if (chromaticMode) {
                        noteHue = kNoteHues[dominant] + m_huePosition;
                        if (noteHue > 1.0f) noteHue -= 1.0f;
                    } else {
                        noteHue = m_chromaHue + m_huePosition;
                        if (noteHue > 1.0f) noteHue -= 1.0f;
                        if (noteHue < 0.0f) noteHue += 1.0f;
                    }
                    workBuf[i] = hsvToRgbF(noteHue, m_saturation, bright);
                }
            }
        }

        // Save rendered frame to aux for odd-frame replay
        std::memcpy(auxBuf, workBuf, kStripLen * sizeof(CRGB_F));

    } else {
        // Odd frame: replay last rendered frame
        std::memcpy(workBuf, auxBuf, kStripLen * sizeof(CRGB_F));
    }

    // =====================================================================
    // POST-PROCESSING (prism, bulb, incandescent) — identical to base
    // =====================================================================

    // Prism effect
    if (m_prismCount > 0.01f) {
        CRGB_F* prismFx  = m_ps2->fxBuffer;
        CRGB_F* prismTmp = m_ps2->tmpBuffer;

        const uint8_t wholeIter = (uint8_t)m_prismCount;
        const float fractIter = m_prismCount - floorf(m_prismCount);

        for (uint8_t it = 0; it < wholeIter; ++it) {
            std::memcpy(prismFx, workBuf, kStripLen * sizeof(CRGB_F));
            prismTransform(prismFx, prismTmp);

            const float hueShift = it * 0.05f;
#ifndef NATIVE_BUILD
            if (hueShift > 0.001f) {
                const uint8_t hShift8 = (uint8_t)(hueShift * 255.0f);
                for (uint16_t j = 0; j < kStripLen; ++j) {
                    if (prismFx[j].r > 0.002f || prismFx[j].g > 0.002f || prismFx[j].b > 0.002f) {
                        CRGB rgb = prismFx[j].toCRGB();
                        CHSV hsv = rgb2hsv_approximate(rgb);
                        hsv.h += hShift8;
                        hsv2rgb_rainbow(hsv, rgb);
                        prismFx[j] = CRGB_F::fromCRGB(rgb);
                    }
                }
            }
#endif
            for (uint16_t j = 0; j < kStripLen; ++j) {
                workBuf[j].r += prismFx[j].r * 0.25f;
                workBuf[j].g += prismFx[j].g * 0.25f;
                workBuf[j].b += prismFx[j].b * 0.25f;
            }
        }

        if (fractIter > 0.01f) {
            std::memcpy(prismFx, workBuf, kStripLen * sizeof(CRGB_F));
            prismTransform(prismFx, prismTmp);

            const float hueShift = wholeIter * 0.05f;
#ifndef NATIVE_BUILD
            if (hueShift > 0.001f) {
                const uint8_t hShift8 = (uint8_t)(hueShift * 255.0f);
                for (uint16_t j = 0; j < kStripLen; ++j) {
                    if (prismFx[j].r > 0.002f || prismFx[j].g > 0.002f || prismFx[j].b > 0.002f) {
                        CRGB rgb = prismFx[j].toCRGB();
                        CHSV hsv = rgb2hsv_approximate(rgb);
                        hsv.h += hShift8;
                        hsv2rgb_rainbow(hsv, rgb);
                        prismFx[j] = CRGB_F::fromCRGB(rgb);
                    }
                }
            }
#endif
            const float fractOpacity = 0.25f * fractIter;
            for (uint16_t j = 0; j < kStripLen; ++j) {
                workBuf[j].r += prismFx[j].r * fractOpacity;
                workBuf[j].g += prismFx[j].g * fractOpacity;
                workBuf[j].b += prismFx[j].b * fractOpacity;
            }
        }
    }

    // Bulb cover
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

    // Incandescent warm-white filter
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

    // Output: convert to ctx.leds and mirror to strip 2
    const uint16_t ledCount = ctx.ledCount;
    for (uint16_t i = 0; i < kStripLen && i < ledCount; ++i) {
        workBuf[i].clip();
        ctx.leds[i] = workBuf[i].toCRGB();
    }
    for (uint16_t i = 0; i < kStripLen && (kStripLen + i) < ledCount; ++i) {
        ctx.leds[kStripLen + i] = ctx.leds[i];
    }

#endif // FEATURE_AUDIO_SYNC
#endif // NATIVE_BUILD
}

// =========================================================================
// SbK1BloomV2ExponentialEffect — cubed max-bin energy for extreme contrast
// =========================================================================

const plugins::EffectMetadata& SbK1BloomV2ExponentialEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "K1 Bloom Exponential",
        "K1 Bloom V2 — cubed energy (extreme contrast, strobe-like peaks)",
        plugins::EffectCategory::PARTY,
        1,
        "K1.Lightwave"
    };
    return meta;
}

// -------------------------------------------------------------------------
// Render — same scroll/mirror/post-processing as BloomV2 but energy is
// the max bin cubed for extreme dynamic range.
// -------------------------------------------------------------------------

void SbK1BloomV2ExponentialEffect::renderEffect(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps2) return;
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

    CRGB_F* workBuf    = m_ps2->workBuffer;
    CRGB_F* scrollBuf  = m_ps2->scrollState;
    CRGB_F* auxBuf     = m_ps2->auxBuffer;
    CRGB_F* fxBuf      = m_ps2->fxBuffer;

    m_iter++;

    // =====================================================================
    // Alternating frames — only update on EVEN frames (K1 parity)
    // =====================================================================
    if ((m_iter & 1) == 0) {

        // -----------------------------------------------------------------
        // EXPONENTIAL ENERGY: find max bin after contrast, then cube it.
        // This creates extreme dynamic range — quiet passages are nearly
        // invisible while loud peaks produce explosive brightness.
        // -----------------------------------------------------------------
        float maxVal = 0.0f;
        for (int c = 0; c < 12; ++c) {
            float bin = applyContrast(m_chromaSmooth[c], m_contrast);
            if (bin > maxVal) maxVal = bin;
        }
        float totalEnergy = maxVal * maxVal * maxVal; // cube for extreme contrast
        if (totalEnergy > 1.0f) totalEnergy = 1.0f;

        CRGB_F gray = {totalEnergy, totalEnergy, totalEnergy};

        // -----------------------------------------------------------------
        // Integer scroll: shift right half outward by 1 or 2 pixels
        // -----------------------------------------------------------------
        const bool fast = (m_mood > 0.5f);

        if (fast) {
            for (int j = kStripLen - 1; j >= (int)(kHalf + 2); --j) {
                workBuf[j] = scrollBuf[j - 2];
            }
            workBuf[kHalf]     = gray;
            workBuf[kHalf + 1] = gray;
        } else {
            for (int j = kStripLen - 1; j >= (int)(kHalf + 1); --j) {
                workBuf[j] = scrollBuf[j - 1];
            }
            workBuf[kHalf] = gray;
        }

        // -----------------------------------------------------------------
        // Save undistorted right half back to scroll state with decay
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            scrollBuf[kHalf + i] = workBuf[kHalf + i] * 0.995f;
        }

        // -----------------------------------------------------------------
        // Sqrt distortion on right half (display only, not saved to scroll)
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            float prog = (float)i / (float)(kHalf - 1);
            float prog_d = sqrtf(prog);
            float src = (float)kHalf + prog_d * (float)(kHalf - 1);
            if (src > (float)(kStripLen - 2)) src = (float)(kStripLen - 2);

            int srcLow = (int)src;
            float frac = src - (float)srcLow;
            int srcHigh = srcLow + 1;

            fxBuf[kHalf + i].r = workBuf[srcLow].r * (1.0f - frac) + workBuf[srcHigh].r * frac;
            fxBuf[kHalf + i].g = workBuf[srcLow].g * (1.0f - frac) + workBuf[srcHigh].g * frac;
            fxBuf[kHalf + i].b = workBuf[srcLow].b * (1.0f - frac) + workBuf[srcHigh].b * frac;
        }
        std::memcpy(&workBuf[kHalf], &fxBuf[kHalf], kHalf * sizeof(CRGB_F));

        // -----------------------------------------------------------------
        // Linear edge fade
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            float fade = (float)(kHalf - 1 - i) / (float)(kHalf - 1);
            workBuf[kHalf + i] *= fade;
        }

        // -----------------------------------------------------------------
        // Mirror right half to left half
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            workBuf[kHalf - 1 - i] = workBuf[kHalf + i];
        }

        // -----------------------------------------------------------------
        // Apply colour to grayscale transport
        // -----------------------------------------------------------------
        {
            float maxBin = 0.0f;
            uint8_t dominant = 0;
            for (uint8_t c = 0; c < 12; ++c) {
                if (m_chromaSmooth[c] > maxBin) {
                    maxBin = m_chromaSmooth[c];
                    dominant = c;
                }
            }

            const bool chromaticMode = (ctx.saturation >= 128);

            for (uint16_t i = 0; i < kStripLen; ++i) {
                float bright = workBuf[i].r;
                if (bright < 0.001f) continue;

                if (ctx.palette.isValid()) {
                    float distFromCenter = fabsf((float)i - ((float)kHalf - 0.5f)) / (float)kHalf;
                    float palPos = (distFromCenter * 200.0f + bright * 55.0f) / 255.0f;
                    workBuf[i] = paletteColorF(ctx.palette, palPos, bright);
                } else {
                    float noteHue;
                    if (chromaticMode) {
                        noteHue = kNoteHues[dominant] + m_huePosition;
                        if (noteHue > 1.0f) noteHue -= 1.0f;
                    } else {
                        noteHue = m_chromaHue + m_huePosition;
                        if (noteHue > 1.0f) noteHue -= 1.0f;
                        if (noteHue < 0.0f) noteHue += 1.0f;
                    }
                    workBuf[i] = hsvToRgbF(noteHue, m_saturation, bright);
                }
            }
        }

        // Save rendered frame to aux for odd-frame replay
        std::memcpy(auxBuf, workBuf, kStripLen * sizeof(CRGB_F));

    } else {
        // Odd frame: replay last rendered frame
        std::memcpy(workBuf, auxBuf, kStripLen * sizeof(CRGB_F));
    }

    // =====================================================================
    // POST-PROCESSING (prism, bulb, incandescent) — identical to base
    // =====================================================================

    // Prism effect
    if (m_prismCount > 0.01f) {
        CRGB_F* prismFx  = m_ps2->fxBuffer;
        CRGB_F* prismTmp = m_ps2->tmpBuffer;

        const uint8_t wholeIter = (uint8_t)m_prismCount;
        const float fractIter = m_prismCount - floorf(m_prismCount);

        for (uint8_t it = 0; it < wholeIter; ++it) {
            std::memcpy(prismFx, workBuf, kStripLen * sizeof(CRGB_F));
            prismTransform(prismFx, prismTmp);

            const float hueShift = it * 0.05f;
#ifndef NATIVE_BUILD
            if (hueShift > 0.001f) {
                const uint8_t hShift8 = (uint8_t)(hueShift * 255.0f);
                for (uint16_t j = 0; j < kStripLen; ++j) {
                    if (prismFx[j].r > 0.002f || prismFx[j].g > 0.002f || prismFx[j].b > 0.002f) {
                        CRGB rgb = prismFx[j].toCRGB();
                        CHSV hsv = rgb2hsv_approximate(rgb);
                        hsv.h += hShift8;
                        hsv2rgb_rainbow(hsv, rgb);
                        prismFx[j] = CRGB_F::fromCRGB(rgb);
                    }
                }
            }
#endif
            for (uint16_t j = 0; j < kStripLen; ++j) {
                workBuf[j].r += prismFx[j].r * 0.25f;
                workBuf[j].g += prismFx[j].g * 0.25f;
                workBuf[j].b += prismFx[j].b * 0.25f;
            }
        }

        if (fractIter > 0.01f) {
            std::memcpy(prismFx, workBuf, kStripLen * sizeof(CRGB_F));
            prismTransform(prismFx, prismTmp);

            const float hueShift = wholeIter * 0.05f;
#ifndef NATIVE_BUILD
            if (hueShift > 0.001f) {
                const uint8_t hShift8 = (uint8_t)(hueShift * 255.0f);
                for (uint16_t j = 0; j < kStripLen; ++j) {
                    if (prismFx[j].r > 0.002f || prismFx[j].g > 0.002f || prismFx[j].b > 0.002f) {
                        CRGB rgb = prismFx[j].toCRGB();
                        CHSV hsv = rgb2hsv_approximate(rgb);
                        hsv.h += hShift8;
                        hsv2rgb_rainbow(hsv, rgb);
                        prismFx[j] = CRGB_F::fromCRGB(rgb);
                    }
                }
            }
#endif
            const float fractOpacity = 0.25f * fractIter;
            for (uint16_t j = 0; j < kStripLen; ++j) {
                workBuf[j].r += prismFx[j].r * fractOpacity;
                workBuf[j].g += prismFx[j].g * fractOpacity;
                workBuf[j].b += prismFx[j].b * fractOpacity;
            }
        }
    }

    // Bulb cover
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

    // Incandescent warm-white filter
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

    // Output: convert to ctx.leds and mirror to strip 2
    const uint16_t ledCount = ctx.ledCount;
    for (uint16_t i = 0; i < kStripLen && i < ledCount; ++i) {
        workBuf[i].clip();
        ctx.leds[i] = workBuf[i].toCRGB();
    }
    for (uint16_t i = 0; i < kStripLen && (kStripLen + i) < ledCount; ++i) {
        ctx.leds[kStripLen + i] = ctx.leds[i];
    }

#endif // FEATURE_AUDIO_SYNC
#endif // NATIVE_BUILD
}

// =========================================================================
// SbK1BloomV2SpectralSpreadEffect — spectral-flatness-driven bloom variant
// =========================================================================

const plugins::EffectMetadata& SbK1BloomV2SpectralSpreadEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "K1 Bloom Spectral Spread",
        "K1 Bloom V2 — peaked tones bright, flat noise dim (spectral flatness)",
        plugins::EffectCategory::PARTY,
        1,
        "K1.Lightwave"
    };
    return meta;
}

// -------------------------------------------------------------------------
// Render — same scroll/mirror/post-processing as BloomV2 but energy is
// driven by spectral peakedness (1 - flatness) * max bin energy.
// Pure tones = bright, noise/full mixes = dim, silence = black.
// -------------------------------------------------------------------------

void SbK1BloomV2SpectralSpreadEffect::renderEffect(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps2) return;
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

    CRGB_F* workBuf    = m_ps2->workBuffer;
    CRGB_F* scrollBuf  = m_ps2->scrollState;
    CRGB_F* auxBuf     = m_ps2->auxBuffer;
    CRGB_F* fxBuf      = m_ps2->fxBuffer;

    m_iter++;

    // =====================================================================
    // Alternating frames — only update on EVEN frames (K1 parity)
    // =====================================================================
    if ((m_iter & 1) == 0) {

        // -----------------------------------------------------------------
        // SPECTRAL SPREAD: intensity from peakedness * max bin energy.
        //
        // Spectral flatness = geometric_mean / arithmetic_mean of 12 bins.
        // Peakedness = 1.0 - flatness (inverted: peaked spectrum = bright).
        // Final intensity = peakedness * maxBinEnergy.
        //
        // Geometric mean via log-sum-exp to avoid float underflow:
        //   logSum  = sum(log(bin + eps)) / 12
        //   geoMean = exp(logSum)
        // -----------------------------------------------------------------
        static constexpr float kEpsilon = 1e-6f;

        float logSum = 0.0f;
        float arithSum = 0.0f;
        float maxBinEnergy = 0.0f;

        for (int c = 0; c < 12; ++c) {
            float bin = m_chromaSmooth[c];
            logSum += logf(bin + kEpsilon);
            arithSum += bin;
            if (bin > maxBinEnergy) maxBinEnergy = bin;
        }

        float geoMean = expf(logSum / 12.0f);
        float arithMean = arithSum / 12.0f;

        // Compute flatness (0 = perfectly peaked, 1 = perfectly flat)
        float flatness = 0.0f;
        if (arithMean > kEpsilon) {
            flatness = geoMean / arithMean;
        }
        if (flatness > 1.0f) flatness = 1.0f;
        if (flatness < 0.0f) flatness = 0.0f;

        float peakedness = 1.0f - flatness;

        // Final intensity: peakedness gates the max bin energy
        float totalEnergy = peakedness * maxBinEnergy;
        if (totalEnergy > 1.0f) totalEnergy = 1.0f;

        CRGB_F gray = {totalEnergy, totalEnergy, totalEnergy};

        // -----------------------------------------------------------------
        // Integer scroll: shift right half outward by 1 or 2 pixels
        // -----------------------------------------------------------------
        const bool fast = (m_mood > 0.5f);

        if (fast) {
            for (int j = kStripLen - 1; j >= (int)(kHalf + 2); --j) {
                workBuf[j] = scrollBuf[j - 2];
            }
            workBuf[kHalf]     = gray;
            workBuf[kHalf + 1] = gray;
        } else {
            for (int j = kStripLen - 1; j >= (int)(kHalf + 1); --j) {
                workBuf[j] = scrollBuf[j - 1];
            }
            workBuf[kHalf] = gray;
        }

        // -----------------------------------------------------------------
        // Save undistorted right half back to scroll state with decay
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            scrollBuf[kHalf + i] = workBuf[kHalf + i] * 0.995f;
        }

        // -----------------------------------------------------------------
        // Sqrt distortion on right half (display only, not saved to scroll)
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            float prog = (float)i / (float)(kHalf - 1);
            float prog_d = sqrtf(prog);
            float src = (float)kHalf + prog_d * (float)(kHalf - 1);
            if (src > (float)(kStripLen - 2)) src = (float)(kStripLen - 2);

            int srcLow = (int)src;
            float frac = src - (float)srcLow;
            int srcHigh = srcLow + 1;

            fxBuf[kHalf + i].r = workBuf[srcLow].r * (1.0f - frac) + workBuf[srcHigh].r * frac;
            fxBuf[kHalf + i].g = workBuf[srcLow].g * (1.0f - frac) + workBuf[srcHigh].g * frac;
            fxBuf[kHalf + i].b = workBuf[srcLow].b * (1.0f - frac) + workBuf[srcHigh].b * frac;
        }
        std::memcpy(&workBuf[kHalf], &fxBuf[kHalf], kHalf * sizeof(CRGB_F));

        // -----------------------------------------------------------------
        // Linear edge fade
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            float fade = (float)(kHalf - 1 - i) / (float)(kHalf - 1);
            workBuf[kHalf + i] *= fade;
        }

        // -----------------------------------------------------------------
        // Mirror right half to left half
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            workBuf[kHalf - 1 - i] = workBuf[kHalf + i];
        }

        // -----------------------------------------------------------------
        // Apply colour to grayscale transport
        // -----------------------------------------------------------------
        {
            float maxBin = 0.0f;
            uint8_t dominant = 0;
            for (uint8_t c = 0; c < 12; ++c) {
                if (m_chromaSmooth[c] > maxBin) {
                    maxBin = m_chromaSmooth[c];
                    dominant = c;
                }
            }

            const bool chromaticMode = (ctx.saturation >= 128);

            for (uint16_t i = 0; i < kStripLen; ++i) {
                float bright = workBuf[i].r;
                if (bright < 0.001f) continue;

                if (ctx.palette.isValid()) {
                    float distFromCenter = fabsf((float)i - ((float)kHalf - 0.5f)) / (float)kHalf;
                    float palPos = (distFromCenter * 200.0f + bright * 55.0f) / 255.0f;
                    workBuf[i] = paletteColorF(ctx.palette, palPos, bright);
                } else {
                    float noteHue;
                    if (chromaticMode) {
                        noteHue = kNoteHues[dominant] + m_huePosition;
                        if (noteHue > 1.0f) noteHue -= 1.0f;
                    } else {
                        noteHue = m_chromaHue + m_huePosition;
                        if (noteHue > 1.0f) noteHue -= 1.0f;
                        if (noteHue < 0.0f) noteHue += 1.0f;
                    }
                    workBuf[i] = hsvToRgbF(noteHue, m_saturation, bright);
                }
            }
        }

        // Save rendered frame to aux for odd-frame replay
        std::memcpy(auxBuf, workBuf, kStripLen * sizeof(CRGB_F));

    } else {
        // Odd frame: replay last rendered frame
        std::memcpy(workBuf, auxBuf, kStripLen * sizeof(CRGB_F));
    }

    // =====================================================================
    // POST-PROCESSING (prism, bulb, incandescent) — identical to base
    // =====================================================================

    // Prism effect
    if (m_prismCount > 0.01f) {
        CRGB_F* prismFx  = m_ps2->fxBuffer;
        CRGB_F* prismTmp = m_ps2->tmpBuffer;

        const uint8_t wholeIter = (uint8_t)m_prismCount;
        const float fractIter = m_prismCount - floorf(m_prismCount);

        for (uint8_t it = 0; it < wholeIter; ++it) {
            std::memcpy(prismFx, workBuf, kStripLen * sizeof(CRGB_F));
            prismTransform(prismFx, prismTmp);

            const float hueShift = it * 0.05f;
#ifndef NATIVE_BUILD
            if (hueShift > 0.001f) {
                const uint8_t hShift8 = (uint8_t)(hueShift * 255.0f);
                for (uint16_t j = 0; j < kStripLen; ++j) {
                    if (prismFx[j].r > 0.002f || prismFx[j].g > 0.002f || prismFx[j].b > 0.002f) {
                        CRGB rgb = prismFx[j].toCRGB();
                        CHSV hsv = rgb2hsv_approximate(rgb);
                        hsv.h += hShift8;
                        hsv2rgb_rainbow(hsv, rgb);
                        prismFx[j] = CRGB_F::fromCRGB(rgb);
                    }
                }
            }
#endif
            for (uint16_t j = 0; j < kStripLen; ++j) {
                workBuf[j].r += prismFx[j].r * 0.25f;
                workBuf[j].g += prismFx[j].g * 0.25f;
                workBuf[j].b += prismFx[j].b * 0.25f;
            }
        }

        if (fractIter > 0.01f) {
            std::memcpy(prismFx, workBuf, kStripLen * sizeof(CRGB_F));
            prismTransform(prismFx, prismTmp);

            const float hueShift = wholeIter * 0.05f;
#ifndef NATIVE_BUILD
            if (hueShift > 0.001f) {
                const uint8_t hShift8 = (uint8_t)(hueShift * 255.0f);
                for (uint16_t j = 0; j < kStripLen; ++j) {
                    if (prismFx[j].r > 0.002f || prismFx[j].g > 0.002f || prismFx[j].b > 0.002f) {
                        CRGB rgb = prismFx[j].toCRGB();
                        CHSV hsv = rgb2hsv_approximate(rgb);
                        hsv.h += hShift8;
                        hsv2rgb_rainbow(hsv, rgb);
                        prismFx[j] = CRGB_F::fromCRGB(rgb);
                    }
                }
            }
#endif
            const float fractOpacity = 0.25f * fractIter;
            for (uint16_t j = 0; j < kStripLen; ++j) {
                workBuf[j].r += prismFx[j].r * fractOpacity;
                workBuf[j].g += prismFx[j].g * fractOpacity;
                workBuf[j].b += prismFx[j].b * fractOpacity;
            }
        }
    }

    // Bulb cover
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

    // Incandescent warm-white filter
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

    // Output: convert to ctx.leds and mirror to strip 2
    const uint16_t ledCount = ctx.ledCount;
    for (uint16_t i = 0; i < kStripLen && i < ledCount; ++i) {
        workBuf[i].clip();
        ctx.leds[i] = workBuf[i].toCRGB();
    }
    for (uint16_t i = 0; i < kStripLen && (kStripLen + i) < ledCount; ++i) {
        ctx.leds[kStripLen + i] = ctx.leds[i];
    }

#endif // FEATURE_AUDIO_SYNC
#endif // NATIVE_BUILD
}

// =========================================================================
// SbK1BloomV2BassTrebleEffect — bass/treble split bloom variant
// =========================================================================

const plugins::EffectMetadata& SbK1BloomV2BassTrebleEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "K1 Bloom BassTreble",
        "K1 Bloom V2 — bass drives brightness, treble drives scroll speed, warm/cool color shift",
        plugins::EffectCategory::PARTY,
        1,
        "K1.Lightwave"
    };
    return meta;
}

// -------------------------------------------------------------------------
// Render — same scroll/mirror/post-processing as BloomV2 but energy is
// split: bass bins (0-5) drive brightness, treble bins (6-11) drive scroll
// speed, and bass/treble balance shifts hue warm-to-cool.
// -------------------------------------------------------------------------

void SbK1BloomV2BassTrebleEffect::renderEffect(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_ps2) return;
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

    CRGB_F* workBuf    = m_ps2->workBuffer;
    CRGB_F* scrollBuf  = m_ps2->scrollState;
    CRGB_F* auxBuf     = m_ps2->auxBuffer;
    CRGB_F* fxBuf      = m_ps2->fxBuffer;

    m_iter++;

    // =====================================================================
    // Alternating frames — only update on EVEN frames (K1 parity)
    // =====================================================================
    if ((m_iter & 1) == 0) {

        // -----------------------------------------------------------------
        // BASS-TREBLE SPLIT: bins 0-5 = bass, bins 6-11 = treble
        // Bass energy (max of lower 6 bins after contrast) drives brightness.
        // Treble energy (max of upper 6 bins after contrast) drives scroll speed.
        // -----------------------------------------------------------------
        float bassEnergy = 0.0f;
        for (int c = 0; c < 6; ++c) {
            float bin = applyContrast(m_chromaSmooth[c], m_contrast);
            if (bin > bassEnergy) bassEnergy = bin;
        }
        if (bassEnergy > 1.0f) bassEnergy = 1.0f;

        float trebleEnergy = 0.0f;
        for (int c = 6; c < 12; ++c) {
            float bin = applyContrast(m_chromaSmooth[c], m_contrast);
            if (bin > trebleEnergy) trebleEnergy = bin;
        }
        if (trebleEnergy > 1.0f) trebleEnergy = 1.0f;

        // Grayscale pixel intensity driven by bass energy
        CRGB_F gray = {bassEnergy, bassEnergy, bassEnergy};

        // Treble ratio for warm/cool color shift (used during color application)
        const float trebleRatio = trebleEnergy / (bassEnergy + trebleEnergy + 0.001f);

        // -----------------------------------------------------------------
        // Integer scroll: treble energy overrides mood-based speed
        // trebleEnergy > 0.5 => scroll by 2 pixels; otherwise scroll by 1
        // -----------------------------------------------------------------
        const bool fast = (trebleEnergy > 0.5f);

        if (fast) {
            for (int j = kStripLen - 1; j >= (int)(kHalf + 2); --j) {
                workBuf[j] = scrollBuf[j - 2];
            }
            workBuf[kHalf]     = gray;
            workBuf[kHalf + 1] = gray;
        } else {
            for (int j = kStripLen - 1; j >= (int)(kHalf + 1); --j) {
                workBuf[j] = scrollBuf[j - 1];
            }
            workBuf[kHalf] = gray;
        }

        // -----------------------------------------------------------------
        // Save undistorted right half back to scroll state with decay
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            scrollBuf[kHalf + i] = workBuf[kHalf + i] * 0.995f;
        }

        // -----------------------------------------------------------------
        // Sqrt distortion on right half (display only, not saved to scroll)
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            float prog = (float)i / (float)(kHalf - 1);
            float prog_d = sqrtf(prog);
            float src = (float)kHalf + prog_d * (float)(kHalf - 1);
            if (src > (float)(kStripLen - 2)) src = (float)(kStripLen - 2);

            int srcLow = (int)src;
            float frac = src - (float)srcLow;
            int srcHigh = srcLow + 1;

            fxBuf[kHalf + i].r = workBuf[srcLow].r * (1.0f - frac) + workBuf[srcHigh].r * frac;
            fxBuf[kHalf + i].g = workBuf[srcLow].g * (1.0f - frac) + workBuf[srcHigh].g * frac;
            fxBuf[kHalf + i].b = workBuf[srcLow].b * (1.0f - frac) + workBuf[srcHigh].b * frac;
        }
        std::memcpy(&workBuf[kHalf], &fxBuf[kHalf], kHalf * sizeof(CRGB_F));

        // -----------------------------------------------------------------
        // Linear edge fade
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            float fade = (float)(kHalf - 1 - i) / (float)(kHalf - 1);
            workBuf[kHalf + i] *= fade;
        }

        // -----------------------------------------------------------------
        // Mirror right half to left half
        // -----------------------------------------------------------------
        for (uint16_t i = 0; i < kHalf; ++i) {
            workBuf[kHalf - 1 - i] = workBuf[kHalf + i];
        }

        // -----------------------------------------------------------------
        // Apply colour with bass/treble hue shift
        // When bass dominates (trebleRatio near 0): warm colours (lower hue)
        // When treble dominates (trebleRatio near 1): cool colours (higher hue)
        // Shift range: 0 to +0.33 (covers warm-red through cool-blue)
        // -----------------------------------------------------------------
        {
            float maxBin = 0.0f;
            uint8_t dominant = 0;
            for (uint8_t c = 0; c < 12; ++c) {
                if (m_chromaSmooth[c] > maxBin) {
                    maxBin = m_chromaSmooth[c];
                    dominant = c;
                }
            }

            const float btHueOffset = trebleRatio * 0.33f;
            const bool chromaticMode = (ctx.saturation >= 128);

            for (uint16_t i = 0; i < kStripLen; ++i) {
                float bright = workBuf[i].r;
                if (bright < 0.001f) continue;

                if (ctx.palette.isValid()) {
                    float distFromCenter = fabsf((float)i - ((float)kHalf - 0.5f)) / (float)kHalf;
                    float palPos = (distFromCenter * 200.0f + bright * 55.0f) / 255.0f;
                    palPos += btHueOffset;
                    if (palPos > 1.0f) palPos -= 1.0f;
                    workBuf[i] = paletteColorF(ctx.palette, palPos, bright);
                } else {
                    float noteHue;
                    if (chromaticMode) {
                        noteHue = kNoteHues[dominant] + m_huePosition + btHueOffset;
                    } else {
                        noteHue = m_chromaHue + m_huePosition + btHueOffset;
                    }
                    // Wrap hue to [0, 1)
                    while (noteHue >= 1.0f) noteHue -= 1.0f;
                    while (noteHue < 0.0f) noteHue += 1.0f;
                    workBuf[i] = hsvToRgbF(noteHue, m_saturation, bright);
                }
            }
        }

        // Save rendered frame to aux for odd-frame replay
        std::memcpy(auxBuf, workBuf, kStripLen * sizeof(CRGB_F));

    } else {
        // Odd frame: replay last rendered frame
        std::memcpy(workBuf, auxBuf, kStripLen * sizeof(CRGB_F));
    }

    // =====================================================================
    // POST-PROCESSING (prism, bulb, incandescent) — identical to base
    // =====================================================================

    // Prism effect
    if (m_prismCount > 0.01f) {
        CRGB_F* prismFx  = m_ps2->fxBuffer;
        CRGB_F* prismTmp = m_ps2->tmpBuffer;

        const uint8_t wholeIter = (uint8_t)m_prismCount;
        const float fractIter = m_prismCount - floorf(m_prismCount);

        for (uint8_t it = 0; it < wholeIter; ++it) {
            std::memcpy(prismFx, workBuf, kStripLen * sizeof(CRGB_F));
            prismTransform(prismFx, prismTmp);

            const float hueShift = it * 0.05f;
#ifndef NATIVE_BUILD
            if (hueShift > 0.001f) {
                const uint8_t hShift8 = (uint8_t)(hueShift * 255.0f);
                for (uint16_t j = 0; j < kStripLen; ++j) {
                    if (prismFx[j].r > 0.002f || prismFx[j].g > 0.002f || prismFx[j].b > 0.002f) {
                        CRGB rgb = prismFx[j].toCRGB();
                        CHSV hsv = rgb2hsv_approximate(rgb);
                        hsv.h += hShift8;
                        hsv2rgb_rainbow(hsv, rgb);
                        prismFx[j] = CRGB_F::fromCRGB(rgb);
                    }
                }
            }
#endif
            for (uint16_t j = 0; j < kStripLen; ++j) {
                workBuf[j].r += prismFx[j].r * 0.25f;
                workBuf[j].g += prismFx[j].g * 0.25f;
                workBuf[j].b += prismFx[j].b * 0.25f;
            }
        }

        if (fractIter > 0.01f) {
            std::memcpy(prismFx, workBuf, kStripLen * sizeof(CRGB_F));
            prismTransform(prismFx, prismTmp);

            const float hueShift = wholeIter * 0.05f;
#ifndef NATIVE_BUILD
            if (hueShift > 0.001f) {
                const uint8_t hShift8 = (uint8_t)(hueShift * 255.0f);
                for (uint16_t j = 0; j < kStripLen; ++j) {
                    if (prismFx[j].r > 0.002f || prismFx[j].g > 0.002f || prismFx[j].b > 0.002f) {
                        CRGB rgb = prismFx[j].toCRGB();
                        CHSV hsv = rgb2hsv_approximate(rgb);
                        hsv.h += hShift8;
                        hsv2rgb_rainbow(hsv, rgb);
                        prismFx[j] = CRGB_F::fromCRGB(rgb);
                    }
                }
            }
#endif
            const float fractOpacity = 0.25f * fractIter;
            for (uint16_t j = 0; j < kStripLen; ++j) {
                workBuf[j].r += prismFx[j].r * fractOpacity;
                workBuf[j].g += prismFx[j].g * fractOpacity;
                workBuf[j].b += prismFx[j].b * fractOpacity;
            }
        }
    }

    // Bulb cover
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

    // Incandescent warm-white filter
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

    // Output: convert to ctx.leds and mirror to strip 2
    const uint16_t ledCount = ctx.ledCount;
    for (uint16_t i = 0; i < kStripLen && i < ledCount; ++i) {
        workBuf[i].clip();
        ctx.leds[i] = workBuf[i].toCRGB();
    }
    for (uint16_t i = 0; i < kStripLen && (kStripLen + i) < ledCount; ++i) {
        ctx.leds[kStripLen + i] = ctx.leds[i];
    }

#endif // FEATURE_AUDIO_SYNC
#endif // NATIVE_BUILD
}

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
