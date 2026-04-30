/**
 * @file RenderPrimitives.cpp
 * @brief Implementation of Layer 4 render primitives.
 *
 * See `RenderPrimitives.h` for the design contract. This translation unit owns
 * the single file-scope scratch buffer used by `drawSpriteScrolled` so that
 * the function can perform an in-place sub-pixel scatter without per-call heap
 * allocation. The buffer is sized for K1's largest expected frame (320 LEDs);
 * `drawSpriteScrolled` clamps its working count to that ceiling. The renderer
 * runs single-threaded on Core 1 so no concurrency guard is required.
 */

#include "RenderPrimitives.h"

#include <cmath>

namespace lightwaveos {
namespace effects {
namespace render {

// ─── File-scope scratch buffer ────────────────────────────────────────────────

/**
 * Maximum LED count any single primitive call may operate on. K1 v2 currently
 * uses 160 LEDs per strip; the unified buffer in RendererActor reaches 320.
 * Sized once at static-init; ~960 bytes of DRAM. Never resized at run time.
 */
static constexpr uint16_t kMaxScratchLeds = 320;
static CRGB g_scratchBuf[kMaxScratchLeds];

// ─── Local helpers ────────────────────────────────────────────────────────────

/**
 * Saturating per-channel additive blend. Mirrors FastLED's `CRGB::operator+=`
 * but takes a float weight in [0, 1] applied to `colour` before the add. Used
 * by every primitive's scatter step.
 */
static inline void addScaled(CRGB& dst, const CRGB& src, float weight) {
    if (weight <= 0.0f) return;
    if (weight > 1.0f) weight = 1.0f;
    int r = static_cast<int>(dst.r) + static_cast<int>(static_cast<float>(src.r) * weight);
    int g = static_cast<int>(dst.g) + static_cast<int>(static_cast<float>(src.g) * weight);
    int b = static_cast<int>(dst.b) + static_cast<int>(static_cast<float>(src.b) * weight);
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    dst.r = static_cast<uint8_t>(r);
    dst.g = static_cast<uint8_t>(g);
    dst.b = static_cast<uint8_t>(b);
}

/// Clamp a float to [0, 1].
static inline float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

/**
 * Plot a single sub-pixel dot at fractional `floatIdx` with `weight × colour`,
 * scattering across the floor and ceil neighbours via linear interpolation.
 * Bounds-checked; out-of-range writes silently dropped.
 */
static inline void plotSubPixel(CRGB* leds,
                                uint16_t ledCount,
                                float floatIdx,
                                CRGB colour,
                                float weight) {
    if (weight <= 0.0f) return;
    const int floorI = static_cast<int>(::floorf(floatIdx));
    const float frac = floatIdx - static_cast<float>(floorI);
    const int ceilI = floorI + 1;
    const float wFloor = (1.0f - frac) * weight;
    const float wCeil = frac * weight;
    if (floorI >= 0 && floorI < static_cast<int>(ledCount)) {
        addScaled(leds[floorI], colour, wFloor);
    }
    if (ceilI >= 0 && ceilI < static_cast<int>(ledCount)) {
        addScaled(leds[ceilI], colour, wCeil);
    }
}

// ─── drawDot ──────────────────────────────────────────────────────────────────

void drawDot(CRGB* leds,
             uint16_t ledCount,
             uint16_t centrePoint,
             float position,
             CRGB colour,
             float opacity,
             float prevPosition,
             bool mirror) {
    if (leds == nullptr || ledCount == 0 || centrePoint == 0 ||
        centrePoint >= ledCount) {
        return;
    }
    position = clamp01(position);
    opacity = clamp01(opacity);
    if (opacity <= 0.0f) return;

    const float leftSpan = static_cast<float>(centrePoint - 1);
    const float rightSpan = static_cast<float>(ledCount - 1 - centrePoint);

    // Convert a normalised position to fractional LED indices for the right
    // half (centrePoint outward to ledCount-1) and the left half
    // (centrePoint-1 outward to 0). At pos=0 the indices are the centre pair;
    // at pos=1 they are the edges.
    auto rightIdxOf = [&](float pos) {
        return static_cast<float>(centrePoint) + pos * rightSpan;
    };
    auto leftIdxOf = [&](float pos) {
        return static_cast<float>(centrePoint - 1) - pos * leftSpan;
    };

    if (prevPosition < 0.0f) {
        // No trail — single sub-pixel dot at full opacity.
        plotSubPixel(leds, ledCount, rightIdxOf(position), colour, opacity);
        if (mirror) {
            plotSubPixel(leds, ledCount, leftIdxOf(position), colour, opacity);
        }
        return;
    }

    // Trail line from prev to current. Brightness is 1/spread so a stationary
    // dot is one pixel at full opacity and a fast dot is dim across many.
    prevPosition = clamp01(prevPosition);
    const float dist01 = ::fabsf(position - prevPosition);
    // Use the larger of the two half-spans for the spread metric so the
    // brightness law is symmetric across asymmetric strips.
    const float spreadRef = (leftSpan > rightSpan) ? leftSpan : rightSpan;
    float spread = dist01 * spreadRef;
    if (spread < 1.0f) spread = 1.0f;
    const float lineWeight = (1.0f / spread) * opacity;
    const int steps = static_cast<int>(::ceilf(spread));
    for (int s = 0; s <= steps; ++s) {
        const float t = (steps > 0)
                            ? static_cast<float>(s) / static_cast<float>(steps)
                            : 0.0f;
        const float p = prevPosition + t * (position - prevPosition);
        plotSubPixel(leds, ledCount, rightIdxOf(p), colour, lineWeight);
        if (mirror) {
            plotSubPixel(leds, ledCount, leftIdxOf(p), colour, lineWeight);
        }
    }
}

// ─── drawSpriteScrolled ───────────────────────────────────────────────────────

void drawSpriteScrolled(CRGB* leds,
                        uint16_t ledCount,
                        uint16_t centrePoint,
                        float scrollAmount,
                        float alpha,
                        float dt) {
    if (leds == nullptr || ledCount == 0 || centrePoint == 0 ||
        centrePoint >= ledCount) {
        return;
    }
    if (ledCount > kMaxScratchLeds) {
        // Hard ceiling — silently truncate. K1 currently uses 320 max so this
        // path is not reachable on production hardware; defending against
        // future strip-length growth without a heap allocation.
        ledCount = kMaxScratchLeds;
    }

    // dt-correct alpha. At dt = 1/120 s, effAlpha == alpha. At dt = 1/60 s,
    // effAlpha == alpha² (one 60 FPS frame == two 120 FPS frames). Constant
    // per call — avoids per-pixel pow.
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    const float effAlpha = (dt > 0.0f) ? ::powf(alpha, dt * 120.0f) : alpha;

    // Snapshot current buffer into scratch, then zero the destination.
    for (uint16_t i = 0; i < ledCount; ++i) {
        g_scratchBuf[i] = leds[i];
        leds[i] = CRGB::Black;
    }

    // Scatter scratch back into leds with sub-pixel scroll. Pixels at
    // index >= centrePoint scroll toward the right edge (positive direction
    // if scrollAmount is positive); pixels < centrePoint scroll toward the
    // left edge (negative direction).
    for (uint16_t i = 0; i < ledCount; ++i) {
        const CRGB& src = g_scratchBuf[i];
        if (src.r == 0 && src.g == 0 && src.b == 0) {
            continue;  // skip black pixels — common case after first frame.
        }
        const float signedScroll =
            (i >= centrePoint) ? scrollAmount : -scrollAmount;
        const float destF = static_cast<float>(i) + signedScroll;
        plotSubPixel(leds, ledCount, destF, src, effAlpha);
    }
}

// ─── fillFromBins ─────────────────────────────────────────────────────────────

void fillFromBins(CRGB* leds,
                  uint16_t ledCount,
                  uint16_t centrePoint,
                  const float* bins,
                  uint8_t binCount,
                  const CRGBPalette16& palette,
                  uint8_t brightness,
                  bool additive) {
    if (leds == nullptr || bins == nullptr || ledCount == 0 ||
        centrePoint == 0 || centrePoint >= ledCount || binCount == 0) {
        return;
    }
    const float leftSpan = static_cast<float>(centrePoint - 1);
    const float rightSpan = static_cast<float>(ledCount - 1 - centrePoint);
    const float denom = (binCount > 1) ? static_cast<float>(binCount - 1) : 1.0f;

    if (!additive) {
        // Hard reset — match SB pattern of memset before re-render.
        for (uint16_t i = 0; i < ledCount; ++i) leds[i] = CRGB::Black;
    }

    for (uint8_t b = 0; b < binCount; ++b) {
        const float ratio = static_cast<float>(b) / denom;  // 0..1
        float mag = bins[b];
        if (mag < 0.0f) mag = 0.0f;
        if (mag > 1.0f) mag = 1.0f;

        const uint8_t paletteIdx =
            static_cast<uint8_t>(ratio * 255.0f + 0.5f);
        const uint8_t valBri =
            static_cast<uint8_t>(static_cast<float>(brightness) * mag + 0.5f);
        if (valBri == 0) continue;
        const CRGB c =
            ColorFromPalette(palette, paletteIdx, valBri, LINEARBLEND);

        // Right index: centre + ratio × rightSpan; rounded to nearest LED.
        const int rIdx =
            static_cast<int>(static_cast<float>(centrePoint) + ratio * rightSpan + 0.5f);
        // Left index: (centre-1) - ratio × leftSpan.
        const int lIdx =
            static_cast<int>(static_cast<float>(centrePoint - 1) - ratio * leftSpan + 0.5f);

        if (rIdx >= 0 && rIdx < static_cast<int>(ledCount)) {
            if (additive) {
                leds[rIdx] += c;
            } else {
                leds[rIdx] = c;
            }
        }
        if (lIdx >= 0 && lIdx < static_cast<int>(ledCount) && lIdx != rIdx) {
            if (additive) {
                leds[lIdx] += c;
            } else {
                leds[lIdx] = c;
            }
        }
    }
}

}  // namespace render
}  // namespace effects
}  // namespace lightwaveos
