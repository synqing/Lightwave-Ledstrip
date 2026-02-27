/**
 * @file KuramotoTransportBuffer.h
 * @brief Visible "light substance" buffer with subpixel transport
 *
 * This is the VISIBLE layer. The Kuramoto engine is invisible.
 *
 * Key operations:
 * - advectWithVelocity(): Subpixel advection using local velocity field
 * - injectAtPos(): Inject energy where events occur
 * - diffuse(): 1D bloom/viscosity
 * - readoutToLeds(): Tone-map and write to centre-origin LEDs
 *
 * All transport is dt-correct (referenced to 60 FPS).
 */

#pragma once

#include <stdint.h>
#include <string.h>
#include <math.h>
#include <algorithm>  // For std::min
#include <esp_heap_caps.h>

// Forward declare CRGB - FastLED included via EffectContext.h
#include "../../plugins/api/EffectContext.h"

// Pull in std::min for portable usage
using std::min;

namespace lightwaveos::effects::ieffect {

/**
 * @brief 16-bit-per-channel RGB used for HDR-ish accumulation.
 */
struct RGB16 {
    uint16_t r = 0;
    uint16_t g = 0;
    uint16_t b = 0;
};

/**
 * @brief KuramotoTransportBuffer
 *
 * A persistent 1D "light substance" buffer with:
 *  - subpixel advection using 2-tap linear interpolation
 *  - dt-correct persistence (trails)
 *  - optional diffusion (cheap 1D bloom/viscosity)
 *  - centre-origin symmetric readout for LightwaveOS dual strips
 *
 * This is intentionally the ONLY visible layer. The Kuramoto engine feeds it via injections.
 */
class KuramotoTransportBuffer {
public:
    static constexpr uint8_t  MAX_ZONES      = 4;
    static constexpr uint16_t MAX_RADIAL_LEN = 80; // centre->edge (for 160 LED strip)

    // PSRAM-ALLOCATED -- large buffers MUST NOT live in DRAM (see MEMORY_ALLOCATION.md)
    struct PsramData {
        RGB16 hist[MAX_ZONES][MAX_RADIAL_LEN];
        RGB16 work[MAX_ZONES][MAX_RADIAL_LEN];
    };

    KuramotoTransportBuffer() = default;

    bool allocatePsram() {
        if (m_ps) return true;
        m_ps = static_cast<PsramData*>(heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
        if (m_ps) {
            memset(m_ps, 0, sizeof(PsramData));
        }
        return m_ps != nullptr;
    }

    void freePsram() {
        if (m_ps) { heap_caps_free(m_ps); m_ps = nullptr; }
    }

    void resetAll() {
        if (!m_ps) return;
        memset(m_ps->hist, 0, sizeof(m_ps->hist));
        memset(m_ps->work, 0, sizeof(m_ps->work));
    }

    void resetZone(uint8_t zoneId) {
        if (!m_ps) return;
        if (zoneId >= MAX_ZONES) return;
        memset(m_ps->hist[zoneId], 0, sizeof(m_ps->hist[zoneId]));
        memset(m_ps->work[zoneId], 0, sizeof(m_ps->work[zoneId]));
    }

    /**
     * @brief Advect the history buffer by a *local* signed velocity field.
     *
     * @param zoneId Zone index [0..MAX_ZONES)
     * @param radialLen Active length (<= MAX_RADIAL_LEN)
     * @param baseOffsetPerFrame60 Distance (in LEDs) moved per frame at 60fps when |v|=1
     * @param persistencePerFrame60 Multiplicative decay per frame at 60fps (e.g. 0.99)
     * @param diffusion01 0..1 cheap 1D diffusion after advection (0 = none)
     * @param dtSeconds Delta time (seconds)
     * @param velocity Per-cell velocity in [-1..+1] (length radialLen)
     */
    void advectWithVelocity(
        uint8_t zoneId,
        uint16_t radialLen,
        float baseOffsetPerFrame60,
        float persistencePerFrame60,
        float diffusion01,
        float dtSeconds,
        const float* velocity
    ) {
        if (!m_ps) return;
        if (zoneId >= MAX_ZONES) return;
        if (radialLen == 0) return;
        if (radialLen > MAX_RADIAL_LEN) radialLen = MAX_RADIAL_LEN;

        // dt-correct (reference = 60fps)
        const float dtScale = dtSeconds * 60.0f;
        const float dtOffset = baseOffsetPerFrame60 * dtScale;
        const float dtPersistence = powf(persistencePerFrame60, dtScale);

        // clear working buffer
        for (uint16_t i = 0; i < radialLen; ++i) {
            m_ps->work[zoneId][i] = RGB16{};
        }

        // push-based semi-Lagrangian style advection:
        // each cell pushes its energy to newPos with 2-tap interpolation.
        //
        // SENSORY BRIDGE REFERENCE: When velocity is nullptr, use UNIFORM OUTWARD motion (+1.0)
        // This creates the characteristic Bloom-style center-to-edge propagation.
        const bool uniformOutward = (velocity == nullptr);

        for (uint16_t i = 0; i < radialLen; ++i) {
            const RGB16 src = m_ps->hist[zoneId][i];
            if ((src.r | src.g | src.b) == 0) continue;

            // +1.0 = outward (toward higher indices = toward edge)
            // -1.0 = inward (toward lower indices = toward center)
            const float v = uniformOutward ? 1.0f : clamp1(velocity[i]);
            const float newPos = (float)i + dtOffset * v;

            if (newPos < 0.0f || newPos > (float)(radialLen - 1)) continue;

            const uint16_t left = (uint16_t)newPos;
            const float frac = newPos - (float)left;
            const float wL = (1.0f - frac) * dtPersistence;
            const float wR = frac * dtPersistence;

            addScaled(m_ps->work[zoneId][left], src, wL);
            if (left + 1 < radialLen) {
                addScaled(m_ps->work[zoneId][left + 1], src, wR);
            }
        }

        // optional cheap diffusion (1D bloom/viscosity)
        if (diffusion01 > 0.0001f) {
            diffuse1D(zoneId, radialLen, diffusion01);
        }

        // swap
        for (uint16_t i = 0; i < radialLen; ++i) {
            m_ps->hist[zoneId][i] = m_ps->work[zoneId][i];
        }
    }

    /**
     * @brief Inject energy at a (possibly fractional) position.
     *
     * @param pos Index in [0, radialLen)
     * @param color 8-bit colour (will be converted to 16-bit HDR-ish)
     * @param amount01 0..1 (scaled by ctx.brightness later)
     * @param spread 0..2 (0 = tight, >0 spreads to neighbours)
     */
    void injectAtPos(
        uint8_t zoneId,
        uint16_t radialLen,
        float pos,
        const CRGB& color,
        float amount01,
        float spread
    ) {
        if (!m_ps) return;
        if (zoneId >= MAX_ZONES) return;
        if (radialLen == 0) return;
        if (radialLen > MAX_RADIAL_LEN) radialLen = MAX_RADIAL_LEN;

        if (pos < 0.0f) return;
        if (pos > (float)(radialLen - 1)) return;

        const uint16_t left = (uint16_t)pos;
        const float frac = pos - (float)left;

        // base weights (2-tap)
        float wL = (1.0f - frac);
        float wR = frac;

        // optional spread to neighbours (simple triangle kernel)
        // spread in [0..2]:
        // 0 -> only 2 taps
        // 1 -> include +/-1 neighbours
        // 2 -> include +/-2 neighbours
        const int maxN = (spread <= 0.01f) ? 0 : (spread < 1.25f ? 1 : 2);
        const float spreadGain = clamp01(spread / 2.0f);

        RGB16 c16 = toRGB16(color, amount01);

        // inject main 2 taps
        addScaled(m_ps->hist[zoneId][left], c16, wL);
        if (left + 1 < radialLen) addScaled(m_ps->hist[zoneId][left + 1], c16, wR);

        // neighbours
        if (maxN >= 1) {
            const float nW = 0.35f * spreadGain;
            if (left > 0) addScaled(m_ps->hist[zoneId][left - 1], c16, nW * wL);
            if (left + 2 < radialLen) addScaled(m_ps->hist[zoneId][left + 2], c16, nW * wR);
        }
        if (maxN >= 2) {
            const float nW2 = 0.20f * spreadGain;
            if (left > 1) addScaled(m_ps->hist[zoneId][left - 2], c16, nW2 * wL);
            if (left + 3 < radialLen) addScaled(m_ps->hist[zoneId][left + 3], c16, nW2 * wR);
        }
    }

    /**
     * @brief Read out to LightwaveOS centre-origin LED layout.
     *
     * Tone-maps HDR energy and blends with ctx.palette so the user's selected
     * palette is respected. Accumulated RGB from injections can drift when
     * blended; the palette blend pulls output toward palette colours.
     */
    void readoutToLeds(
        uint8_t zoneId,
        lightwaveos::plugins::EffectContext& ctx,
        uint16_t radialLen,
        float exposure,              // 0..?
        float saturationBoost        // 0..1
    ) {
        if (!m_ps) return;
        if (zoneId >= MAX_ZONES) return;
        if (radialLen == 0) return;
        if (radialLen > MAX_RADIAL_LEN) radialLen = MAX_RADIAL_LEN;

        // Detect dual-strip layout (two 160 segments).
        const bool dualStrip = (ctx.ledCount >= 320);
        const uint16_t stripLen = (ctx.centerPoint + 1) * 2;
        if (stripLen < 2) return;

        // Clear target (we fully overwrite).
        for (uint16_t i = 0; i < ctx.ledCount; ++i) {
            ctx.leds[i] = CRGB::Black;
        }

        // Palette blend: pull transported colours toward user's palette (centre-origin convention)
        constexpr float kPaletteMix = 0.65f;  // 0=transport only, 1=palette only

        for (uint16_t dist = 0; dist < radialLen; ++dist) {
            CRGB c8 = toneMapToCRGB8(m_ps->hist[zoneId][dist], exposure, saturationBoost);

            // Blend with palette so selected palette is honoured
            if (ctx.palette.isValid() && kPaletteMix > 0.001f) {
                const float dist01 = static_cast<float>(dist) / static_cast<float>(radialLen);
                const uint8_t palIdx = ctx.gHue + static_cast<uint8_t>(dist01 * 64.0f);
                const uint8_t lum = (c8.r > c8.g) ? ((c8.r > c8.b) ? c8.r : c8.b)
                                    : ((c8.g > c8.b) ? c8.g : c8.b);
                const CRGB palCol = ctx.palette.getColor(palIdx, lum);
                const float t = 1.0f - kPaletteMix;
                c8.r = static_cast<uint8_t>(c8.r * t + palCol.r * kPaletteMix);
                c8.g = static_cast<uint8_t>(c8.g * t + palCol.g * kPaletteMix);
                c8.b = static_cast<uint8_t>(c8.b * t + palCol.b * kPaletteMix);
            }

            // strip 1
            writeCentrePair(ctx.leds, 0, stripLen, ctx.centerPoint, dist, c8);

            // strip 2 (same mapping, offset by +160)
            if (dualStrip) {
                writeCentrePair(ctx.leds, stripLen, stripLen, ctx.centerPoint, dist, c8);
            }
        }

        // Respect ctx.brightness as final output gain.
        // (ZoneComposer may apply additional scaling afterwards depending on mode.)
        if (ctx.brightness < 255) {
            for (uint16_t i = 0; i < ctx.ledCount; ++i) {
                ctx.leds[i].nscale8_video(ctx.brightness);
            }
        }
    }

private:
    // PSRAM-allocated large buffers
    PsramData* m_ps = nullptr;

    static float clamp01(float x) {
        if (x < 0.0f) return 0.0f;
        if (x > 1.0f) return 1.0f;
        return x;
    }

    static float clamp1(float x) {
        if (x < -1.0f) return -1.0f;
        if (x >  1.0f) return  1.0f;
        return x;
    }

    static void addScaled(RGB16& dst, const RGB16& src, float w) {
        if (w <= 0.000001f) return;

        // float scaling (cost is tiny: 80 cells)
        const uint32_t addR = (uint32_t)(src.r * w);
        const uint32_t addG = (uint32_t)(src.g * w);
        const uint32_t addB = (uint32_t)(src.b * w);

        dst.r = (uint16_t)min<uint32_t>(65535u, (uint32_t)dst.r + addR);
        dst.g = (uint16_t)min<uint32_t>(65535u, (uint32_t)dst.g + addG);
        dst.b = (uint16_t)min<uint32_t>(65535u, (uint32_t)dst.b + addB);
    }

    static RGB16 toRGB16(const CRGB& c8, float amount01) {
        const float a = clamp01(amount01);
        RGB16 out;
        out.r = (uint16_t)min<int>(65535, (int)lroundf((float)c8.r * 257.0f * a));
        out.g = (uint16_t)min<int>(65535, (int)lroundf((float)c8.g * 257.0f * a));
        out.b = (uint16_t)min<int>(65535, (int)lroundf((float)c8.b * 257.0f * a));
        return out;
    }

    static CRGB toneMapToCRGB8(const RGB16& in, float exposure, float saturationBoost) {
        // Convert to 0..~inf "scene linear"
        const float e = (exposure <= 0.0001f) ? 0.0001f : exposure;

        float r = (float)in.r / 65535.0f;
        float g = (float)in.g / 65535.0f;
        float b = (float)in.b / 65535.0f;

        // exposure
        r *= e; g *= e; b *= e;

        // Luminance-based tone map to preserve hue (colour corruption fix).
        // Per-channel tone map pushes equal R,G,B toward white; tone-map luminance then scale RGB.
        float lum = (r + g + b) * (1.0f / 3.0f);
        if (lum < 1e-6f) {
            return CRGB(0, 0, 0);
        }
        float lumT = lum / (1.0f + lum);  // soft-knee on luminance
        float scale = lumT / lum;
        r *= scale; g *= scale; b *= scale;

        // optional saturation boost (cheap): pull towards chroma away from luma
        const float sat = clamp01(saturationBoost);
        if (sat > 0.0001f) {
            const float luma = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            r = luma + (r - luma) * (1.0f + sat);
            g = luma + (g - luma) * (1.0f + sat);
            b = luma + (b - luma) * (1.0f + sat);

            // clamp after boost
            r = clamp01(r);
            g = clamp01(g);
            b = clamp01(b);
        }

        return CRGB((uint8_t)lroundf(r * 255.0f),
                    (uint8_t)lroundf(g * 255.0f),
                    (uint8_t)lroundf(b * 255.0f));
    }

    void diffuse1D(uint8_t zoneId, uint16_t radialLen, float diffusion01) {
        if (!m_ps) return;
        const float d = clamp01(diffusion01);
        if (d <= 0.0001f) return;

        RGB16 tmp[MAX_RADIAL_LEN];
        for (uint16_t i = 0; i < radialLen; ++i) tmp[i] = m_ps->work[zoneId][i];

        // Kernel weights: keep energy + share with neighbours.
        const float wC = 1.0f - d;
        const float wN = d * 0.5f;

        for (uint16_t i = 0; i < radialLen; ++i) {
            RGB16 out{};

            // centre
            addScaled(out, tmp[i], wC);

            // neighbours
            if (i > 0) addScaled(out, tmp[i - 1], wN);
            if (i + 1 < radialLen) addScaled(out, tmp[i + 1], wN);

            m_ps->work[zoneId][i] = out;
        }
    }

    static void writeCentrePair(
        CRGB* leds,
        uint16_t baseOffset,
        uint16_t stripLen,
        uint16_t centrePoint,
        uint16_t dist,
        const CRGB& c
    ) {
        // left = centrePoint - dist
        // right = centrePoint + 1 + dist
        const int32_t left  = (int32_t)baseOffset + (int32_t)centrePoint - (int32_t)dist;
        const int32_t right = (int32_t)baseOffset + (int32_t)centrePoint + 1 + (int32_t)dist;

        if (left >= (int32_t)baseOffset && left < (int32_t)(baseOffset + stripLen)) {
            leds[left] = c;
        }
        if (right >= (int32_t)baseOffset && right < (int32_t)(baseOffset + stripLen)) {
            leds[right] = c;
        }
    }
};

} // namespace lightwaveos::effects::ieffect
