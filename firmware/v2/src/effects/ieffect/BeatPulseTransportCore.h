/**
 * @file BeatPulseTransportCore.h
 * @brief Bloom-style subpixel advection + temporal feedback core for BeatPulse family
 *
 * This ports the *mechanism* behind Sensory Bridge's `draw_sprite(dest, prev, ..., position, alpha)`
 * into LightwaveOS' centre-origin radial coordinate system:
 *
 *   - Maintain a per-zone HDR-ish history buffer in radial space (distance-from-centre).
 *   - Each frame: advect the whole buffer outward by a fractional offset (subpixel motion).
 *   - Apply persistence (temporal feedback) in a dt-correct way.
 *   - Optionally apply a tiny diffusion pass (bloom-y softening).
 *
 * Why this matters:
 *  - Subpixel advection is the difference between "computed" stepping and "liquid" motion.
 *
 * Design constraints:
 *  - No heap allocations.
 *  - Per-zone state (ZoneComposer uses a shared effect instance).
 *  - Works even if init() is never called (lazy safety).
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include "BeatPulseRenderUtils.h"   // clamp01, floatToByte, etc.
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos::effects::ieffect {

class BeatPulseTransportCore final {
public:
    // LightwaveOS currently uses max 4 zones (see ZoneComposer.cpp)
    static constexpr uint8_t  MAX_ZONES      = 4;

    // We allocate conservatively: enough for a 160-LED strip's radial half (80),
    // but keep 160 to remain safe if centrePoint semantics ever change.
    static constexpr uint16_t MAX_RADIAL_LEN = 160;

    BeatPulseTransportCore() { resetAll(); }

    void resetAll() {
        std::memset(m_hist, 0, sizeof(m_hist));
        std::memset(m_work, 0, sizeof(m_work));
        std::memset(m_lastRenderMs, 0, sizeof(m_lastRenderMs));
    }

    void resetZone(uint8_t zoneId) {
        if (zoneId >= MAX_ZONES) return;
        std::memset(m_hist[zoneId], 0, sizeof(m_hist[zoneId]));
        std::memset(m_work[zoneId], 0, sizeof(m_work[zoneId]));
        m_lastRenderMs[zoneId] = 0;
    }

    /**
     * @brief Advect history outward by a fractional offset (subpixel motion).
     *
     * @param zoneId                  Zone index [0..MAX_ZONES-1]
     * @param radialLen               Active radial length (<= MAX_RADIAL_LEN)
     * @param offsetPerFrameAt60Hz    Offset in "radial pixels" per 1/60s frame (can be fractional)
     * @param persistencePerFrame60Hz Persistence multiplier per 1/60s frame (e.g., 0.99)
     * @param diffusion01             Optional 0..1 diffusion strength (0 disables)
     * @param dtSeconds               Delta seconds (use ctx.getSafeDeltaSeconds())
     */
    void advectOutward(
        uint8_t zoneId,
        uint16_t radialLen,
        float offsetPerFrameAt60Hz,
        float persistencePerFrame60Hz,
        float diffusion01,
        float dtSeconds
    ) {
        if (zoneId >= MAX_ZONES) return;
        radialLen = (radialLen > MAX_RADIAL_LEN) ? MAX_RADIAL_LEN : radialLen;
        if (radialLen < 2) return;

        // If we haven't rendered this zone in a while (zone switched away), clear stale history.
        // This prevents "ghost frames" reappearing when the effect is re-selected.
        // Threshold: ~0.5s.
        if (m_lastRenderMs[zoneId] != 0) {
            const uint32_t gapMs = (m_nowMs > m_lastRenderMs[zoneId]) ? (m_nowMs - m_lastRenderMs[zoneId]) : 0;
            if (gapMs > 500) {
                std::memset(m_hist[zoneId], 0, sizeof(m_hist[zoneId]));
            }
        }
        m_lastRenderMs[zoneId] = m_nowMs;

        // Clamp dt to avoid huge jumps after stalls.
        const float dt = (dtSeconds < 0.0f) ? 0.0f : (dtSeconds > 0.050f ? 0.050f : dtSeconds);

        // dt-correct scaling (reference: 60Hz)
        const float dtScale = dt * 60.0f;

        const float dtOffset = offsetPerFrameAt60Hz * dtScale;
        const float dtPersist = powf(persistencePerFrame60Hz, dtScale);

        // Clear working buffer
        std::memset(m_work[zoneId], 0, sizeof(RGB16) * radialLen);

        // Semi-Lagrangian-like "push" advection using 2-tap linear interpolation.
        // Equivalent to Bloom's draw_sprite shifting prev->dest with fractional weight.
        for (uint16_t i = 0; i < radialLen; i++) {
            const float newPos = static_cast<float>(i) + dtOffset;
            if (newPos < 0.0f || newPos >= static_cast<float>(radialLen - 1)) {
                continue;
            }

            const uint16_t left = static_cast<uint16_t>(newPos);
            const float frac = newPos - static_cast<float>(left);

            const float mixL = (1.0f - frac) * dtPersist;
            const float mixR = frac * dtPersist;

            addScaled(m_work[zoneId][left],     m_hist[zoneId][i], mixL);
            addScaled(m_work[zoneId][left + 1], m_hist[zoneId][i], mixR);
        }

        // Optional diffusion pass (tiny 1D blur) for "bloom" softness.
        // This intentionally happens AFTER advection so it feels like energy spreads as it travels.
        diffusion01 = clamp01(diffusion01);
        if (diffusion01 > 0.0001f) {
            const float k = 0.15f * diffusion01;       // neighbour contribution
            const float c = 1.0f - (2.0f * k);          // centre retention
            // We reuse m_hist as a temp to avoid extra buffers: write into hist, then swap.
            for (uint16_t i = 0; i < radialLen; i++) {
                const RGB16 a = m_work[zoneId][i];
                const RGB16 l = (i > 0) ? m_work[zoneId][i - 1] : RGB16{};
                const RGB16 r = (i + 1 < radialLen) ? m_work[zoneId][i + 1] : RGB16{};

                RGB16 out{};
                out.r = clampU16(static_cast<uint32_t>(a.r * c + l.r * k + r.r * k));
                out.g = clampU16(static_cast<uint32_t>(a.g * c + l.g * k + r.g * k));
                out.b = clampU16(static_cast<uint32_t>(a.b * c + l.b * k + r.b * k));
                m_hist[zoneId][i] = out;
            }
        } else {
            // Swap buffers: hist <- work
            std::memcpy(m_hist[zoneId], m_work[zoneId], sizeof(RGB16) * radialLen);
        }

        // Edge sink: smooth falloff in last 8 bins to prevent hard cutoff.
        // Energy fades quadratically as it approaches the boundary.
        constexpr uint16_t EDGE_SINK_WIDTH = 8;
        if (radialLen > EDGE_SINK_WIDTH) {
            const uint16_t sinkStart = radialLen - EDGE_SINK_WIDTH;
            for (uint16_t i = sinkStart; i < radialLen; i++) {
                const float distFromEdge = static_cast<float>(radialLen - 1 - i);
                const float t = distFromEdge / static_cast<float>(EDGE_SINK_WIDTH);
                const float fade = t * t;  // Quadratic: faster fade near edge
                m_hist[zoneId][i].r = static_cast<uint16_t>(m_hist[zoneId][i].r * fade);
                m_hist[zoneId][i].g = static_cast<uint16_t>(m_hist[zoneId][i].g * fade);
                m_hist[zoneId][i].b = static_cast<uint16_t>(m_hist[zoneId][i].b * fade);
            }
        }
    }

    /**
     * @brief Inject colour energy at the centre of the radial buffer.
     *
     * Uses a 5-bin Gaussian-like kernel for body (not a single-pixel needle).
     * This creates a more organic "bloom" at the injection point.
     *
     * @param zoneId     Zone index
     * @param radialLen  Active radial length
     * @param color      8-bit colour to inject (will be upscaled to 16-bit)
     * @param amount01   0..1 injection amount
     * @param spread01   0..1 spread into the first few radial bins (0=pinpoint, 1=spread)
     */
    void injectAtCentre(
        uint8_t zoneId,
        uint16_t radialLen,
        const CRGB& color,
        float amount01,
        float spread01
    ) {
        if (zoneId >= MAX_ZONES) return;
        radialLen = (radialLen > MAX_RADIAL_LEN) ? MAX_RADIAL_LEN : radialLen;
        if (radialLen < 1) return;

        const float a = clamp01(amount01);
        if (a <= 0.0001f) return;

        spread01 = clamp01(spread01);

        // Convert to 16-bit energy (0..65535). 257 maps 0..255 -> 0..65535 nicely.
        RGB16 e{};
        e.r = clampU16(static_cast<uint32_t>(static_cast<float>(color.r) * 257.0f * a));
        e.g = clampU16(static_cast<uint32_t>(static_cast<float>(color.g) * 257.0f * a));
        e.b = clampU16(static_cast<uint32_t>(static_cast<float>(color.b) * 257.0f * a));

        // 5-bin Gaussian-like kernel for organic body (not a needle).
        // At spread=0: tight core (mostly bin 0)
        // At spread=1: wide spread across 5 bins
        // Weights are normalised to sum to ~1.0
        const float tightness = 1.0f - spread01;
        const float w0 = 0.50f + 0.40f * tightness;  // 0.50..0.90
        const float w1 = 0.20f * spread01 + 0.05f;   // 0.05..0.25
        const float w2 = 0.12f * spread01;           // 0.00..0.12
        const float w3 = 0.06f * spread01;           // 0.00..0.06
        const float w4 = 0.02f * spread01;           // 0.00..0.02

        addScaled(m_hist[zoneId][0], e, w0);
        if (radialLen > 1) addScaled(m_hist[zoneId][1], e, w1);
        if (radialLen > 2) addScaled(m_hist[zoneId][2], e, w2);
        if (radialLen > 3) addScaled(m_hist[zoneId][3], e, w3);
        if (radialLen > 4) addScaled(m_hist[zoneId][4], e, w4);
    }

    /**
     * @brief Convert radial HDR history into a centre-origin dual-strip LED buffer.
     *
     * Writes into ctx.leds (which, in ZoneComposer, points at the per-zone buffer).
     *
     * @param zoneId     Zone index
     * @param ctx        EffectContext
     * @param radialLen  Active radial length
     * @param outGain01  Additional 0..1 scaling applied at output (e.g., silentScale/intensity)
     */
    void readoutToLeds(
        uint8_t zoneId,
        plugins::EffectContext& ctx,
        uint16_t radialLen,
        float outGain01
    ) const {
        readoutToLedsWithPalette(zoneId, ctx, radialLen, outGain01, ctx.gHue, 0.0f);
    }

    /**
     * @brief Convert radial HDR history with palette-based colour enhancement.
     *
     * Adds spatial colour variation by blending transported colour with distance-based
     * palette sampling. This creates richer colours as light flows outward.
     *
     * @param zoneId       Zone index
     * @param ctx          EffectContext
     * @param radialLen    Active radial length
     * @param outGain01    Additional 0..1 scaling applied at output
     * @param baseHue      Base palette index (typically ctx.gHue + shift)
     * @param paletteMix01 How much to blend palette (0=pure transport, 1=full palette)
     */
    void readoutToLedsWithPalette(
        uint8_t zoneId,
        plugins::EffectContext& ctx,
        uint16_t radialLen,
        float outGain01,
        uint8_t baseHue,
        float paletteMix01
    ) const {
        if (zoneId >= MAX_ZONES) return;

        radialLen = (radialLen > MAX_RADIAL_LEN) ? MAX_RADIAL_LEN : radialLen;
        if (radialLen < 1) return;

        const float g = clamp01(outGain01);
        const float pmix = clamp01(paletteMix01);

        // Derive strip length from centrePoint (ZoneComposer sets centrePoint=79 for 160 LEDs).
        // stripLen = (centrePoint+1)*2.
        const uint16_t stripLen = static_cast<uint16_t>((ctx.centerPoint + 1) * 2);
        const bool dualStrip = (ctx.ledCount >= static_cast<uint32_t>(stripLen * 2));

        for (uint16_t dist = 0; dist < radialLen; dist++) {
            // Map radial distance to symmetric pair around centre.
            const int32_t left  = static_cast<int32_t>(ctx.centerPoint) - static_cast<int32_t>(dist);
            const int32_t right = static_cast<int32_t>(ctx.centerPoint) + 1 + static_cast<int32_t>(dist);

            if (left < 0 || right >= static_cast<int32_t>(stripLen)) {
                continue;
            }

            // Get transported HDR colour
            CRGB c = toCRGB8(m_hist[zoneId][dist], g);

            // Blend with palette based on distance for richer colour variation
            if (pmix > 0.001f) {
                // Distance-based palette offset: creates colour shift as light travels
                const float dist01 = static_cast<float>(dist) / static_cast<float>(radialLen);
                const uint8_t paletteIdx = baseHue + static_cast<uint8_t>(dist01 * 64.0f);

                // Use luminance of transported colour to modulate palette brightness
                const uint8_t lum = (c.r > c.g) ? ((c.r > c.b) ? c.r : c.b) : ((c.g > c.b) ? c.g : c.b);
                CRGB palCol = ctx.palette.getColor(paletteIdx, lum);

                // Blend: transported colour + palette tint
                const float t = 1.0f - pmix;
                c.r = static_cast<uint8_t>(c.r * t + palCol.r * pmix);
                c.g = static_cast<uint8_t>(c.g * t + palCol.g * pmix);
                c.b = static_cast<uint8_t>(c.b * t + palCol.b * pmix);
            }

            // Strip 1
            ctx.leds[left]  = c;
            ctx.leds[right] = c;

            // Strip 2 (if present)
            if (dualStrip) {
                const uint16_t o = stripLen;
                ctx.leds[left + o]  = c;
                ctx.leds[right + o] = c;
            }
        }
    }

    void setNowMs(uint32_t nowMs) { m_nowMs = nowMs; }

private:
    struct RGB16 {
        uint16_t r = 0;
        uint16_t g = 0;
        uint16_t b = 0;
    };

    static inline uint16_t clampU16(uint32_t v) {
        return (v > 65535u) ? 65535u : static_cast<uint16_t>(v);
    }

    static inline void addScaled(RGB16& dst, const RGB16& src, float scale) {
        if (scale <= 0.000001f) return;

        const uint32_t r = static_cast<uint32_t>(static_cast<float>(src.r) * scale);
        const uint32_t g = static_cast<uint32_t>(static_cast<float>(src.g) * scale);
        const uint32_t b = static_cast<uint32_t>(static_cast<float>(src.b) * scale);

        dst.r = clampU16(static_cast<uint32_t>(dst.r) + r);
        dst.g = clampU16(static_cast<uint32_t>(dst.g) + g);
        dst.b = clampU16(static_cast<uint32_t>(dst.b) + b);
    }

    /**
     * @brief Knee tone-map: boosts low levels, compresses highlights.
     *
     * Formula: out = in / (in + knee)
     * This keeps low-level energy visible and prevents harsh clipping.
     * Without this, the output looks "computed" and dull.
     */
    static inline float kneeToneMap(float in01, float knee = 0.5f) {
        if (in01 <= 0.0f) return 0.0f;
        // Rescale so that 1.0 maps to ~0.67 (at knee=0.5), not 0.5
        // This keeps peak brightness punchy while compressing highlights
        const float mapped = in01 / (in01 + knee);
        // Boost to restore headroom: at in=1.0, knee=0.5, mapped=0.67 → out=1.0
        return mapped * (1.0f + knee);
    }

    static inline CRGB toCRGB8(const RGB16& v, float gain01) {
        // Convert 16-bit to 8-bit with knee tone-mapping for rich output.
        const float g = clamp01(gain01);

        // Normalise 16-bit to 0..1
        float r01 = static_cast<float>(v.r) / 65535.0f;
        float g01 = static_cast<float>(v.g) / 65535.0f;
        float b01 = static_cast<float>(v.b) / 65535.0f;

        // Apply knee tone-map (boosts lows, compresses highs)
        r01 = kneeToneMap(r01);
        g01 = kneeToneMap(g01);
        b01 = kneeToneMap(b01);

        // Apply gain and convert to 8-bit
        uint32_t r8 = static_cast<uint32_t>(r01 * g * 255.0f + 0.5f);
        uint32_t g8 = static_cast<uint32_t>(g01 * g * 255.0f + 0.5f);
        uint32_t b8 = static_cast<uint32_t>(b01 * g * 255.0f + 0.5f);

        if (r8 > 255u) r8 = 255u;
        if (g8 > 255u) g8 = 255u;
        if (b8 > 255u) b8 = 255u;

        return CRGB(static_cast<uint8_t>(r8), static_cast<uint8_t>(g8), static_cast<uint8_t>(b8));
    }

    RGB16    m_hist[MAX_ZONES][MAX_RADIAL_LEN];
    RGB16    m_work[MAX_ZONES][MAX_RADIAL_LEN];
    uint32_t m_lastRenderMs[MAX_ZONES] = {0,0,0,0};

    uint32_t m_nowMs = 0;
};

} // namespace lightwaveos::effects::ieffect
