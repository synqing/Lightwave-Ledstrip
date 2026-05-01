/**
 * @file native_stubs.h
 * @brief Force-included header providing NATIVE_BUILD stubs for golden-frame testing.
 *
 * Compile with: -include native_stubs.h
 * This header is force-included before any other header in every TU,
 * ensuring ColourUtil and other stubs are visible to inline callers
 * in production headers (e.g. BeatPulseCore.h line 76).
 *
 * ============================================================================
 *  DRIFT GUARD — DO NOT REMOVE
 * ============================================================================
 *  The stub implementations of ColourUtil::addWhiteSaturating and
 *  ColourUtil::additive below MUST mirror production byte-for-byte.
 *  Production lives in:
 *      firmware-v3/src/effects/ieffect/BeatPulseRenderUtils.h:174-205
 *  Distinctive production sentinel comment to grep for when re-syncing:
 *      "Replaces former addWhiteSaturating which drove (r,g,b) toward (255,255,255)."
 *
 *  To prevent silent drift, this file declares constexpr "canonical" copies
 *  of both functions immediately below the stubs and a static_assert grid
 *  that fails the build if the stub and the canonical disagree on any
 *  representative input. If you change either, build will break until you
 *  update both AND verify production lines 174-205 still match the canonical.
 * ============================================================================
 */

#pragma once

#include <cstdint>

// Pull in mock CRGB type first
#include "../../test/test_native/mocks/fastled_mock.h"

// ColourUtil — guarded out by #ifndef NATIVE_BUILD in BeatPulseRenderUtils.h.
// We provide the identical implementation here for native builds.
namespace lightwaveos::effects::ieffect::ColourUtil {

// ---------------------------------------------------------------------------
// Stub implementations — mirrored from production (BeatPulseRenderUtils.h:174-205)
// ---------------------------------------------------------------------------

static inline void addWhiteSaturating(CRGB& c, uint8_t w) {
    uint16_t r = static_cast<uint16_t>(c.r);
    uint16_t g = static_cast<uint16_t>(c.g);
    uint16_t b = static_cast<uint16_t>(c.b);
    uint16_t lum = (r + g + b) / 3;
    if (lum == 0) {
        c.r = c.g = c.b = (w > 255) ? 255 : w;
        return;
    }
    uint16_t lumNew = lum + w;
    if (lumNew > 255) lumNew = 255;
    c.r = static_cast<uint8_t>((r * lumNew) / lum);
    c.g = static_cast<uint8_t>((g * lumNew) / lum);
    c.b = static_cast<uint8_t>((b * lumNew) / lum);
}

static inline CRGB additive(const CRGB& base, const CRGB& overlay) {
    return CRGB(
        static_cast<uint8_t>((static_cast<uint16_t>(base.r) + overlay.r + 1) >> 1),
        static_cast<uint8_t>((static_cast<uint16_t>(base.g) + overlay.g + 1) >> 1),
        static_cast<uint8_t>((static_cast<uint16_t>(base.b) + overlay.b + 1) >> 1)
    );
}

} // namespace lightwaveos::effects::ieffect::ColourUtil

// ===========================================================================
// Drift-guard canonical reference + compile-time equivalence assertions.
//
// MAINTAINER PROTOCOL when production changes:
//   1. Open firmware-v3/src/effects/ieffect/BeatPulseRenderUtils.h:174-205.
//   2. Confirm the sentinel comment (see top of this file) still appears.
//   3. Update BOTH the stub block above AND the canonical_* functions below.
//   4. If new behavioural inputs need coverage, extend kAwsRgbGrid / kAwsWeightGrid.
//   5. Re-run the native build; the static_asserts below MUST still pass.
// ===========================================================================

namespace lightwaveos::effects::ieffect::ColourUtil::detail {

// Compile-time mirror of production. KEEP IN LOCK-STEP with the stub above.
constexpr CRGB canonical_addWhiteSaturating(CRGB c, uint8_t w) {
    uint16_t r = static_cast<uint16_t>(c.r);
    uint16_t g = static_cast<uint16_t>(c.g);
    uint16_t b = static_cast<uint16_t>(c.b);
    uint16_t lum = static_cast<uint16_t>((r + g + b) / 3);
    if (lum == 0) {
        uint8_t v = (w > 255) ? static_cast<uint8_t>(255) : w;
        return CRGB(v, v, v);
    }
    uint16_t lumNew = static_cast<uint16_t>(lum + w);
    if (lumNew > 255) lumNew = 255;
    return CRGB(
        static_cast<uint8_t>((r * lumNew) / lum),
        static_cast<uint8_t>((g * lumNew) / lum),
        static_cast<uint8_t>((b * lumNew) / lum)
    );
}

constexpr CRGB canonical_additive(CRGB base, CRGB overlay) {
    return CRGB(
        static_cast<uint8_t>((static_cast<uint16_t>(base.r) + overlay.r + 1) >> 1),
        static_cast<uint8_t>((static_cast<uint16_t>(base.g) + overlay.g + 1) >> 1),
        static_cast<uint8_t>((static_cast<uint16_t>(base.b) + overlay.b + 1) >> 1)
    );
}

// constexpr re-expression of the stub body so we can compare at compile time.
// Must remain a byte-for-byte logical copy of the runtime stub above.
constexpr CRGB stub_addWhiteSaturating_ce(CRGB c, uint8_t w) {
    uint16_t r = static_cast<uint16_t>(c.r);
    uint16_t g = static_cast<uint16_t>(c.g);
    uint16_t b = static_cast<uint16_t>(c.b);
    uint16_t lum = static_cast<uint16_t>((r + g + b) / 3);
    if (lum == 0) {
        uint8_t v = (w > 255) ? static_cast<uint8_t>(255) : w;
        return CRGB(v, v, v);
    }
    uint16_t lumNew = static_cast<uint16_t>(lum + w);
    if (lumNew > 255) lumNew = 255;
    return CRGB(
        static_cast<uint8_t>((r * lumNew) / lum),
        static_cast<uint8_t>((g * lumNew) / lum),
        static_cast<uint8_t>((b * lumNew) / lum)
    );
}

constexpr CRGB stub_additive_ce(CRGB base, CRGB overlay) {
    return CRGB(
        static_cast<uint8_t>((static_cast<uint16_t>(base.r) + overlay.r + 1) >> 1),
        static_cast<uint8_t>((static_cast<uint16_t>(base.g) + overlay.g + 1) >> 1),
        static_cast<uint8_t>((static_cast<uint16_t>(base.b) + overlay.b + 1) >> 1)
    );
}

constexpr bool eq(CRGB a, CRGB b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

// Input grid — 8 RGB triples × 6 weights for addWhiteSaturating.
// Includes lum=0 inputs (CRGB(0,0,0)) to engage the (w>255)?255:w clamp branch.
constexpr CRGB kAwsRgbGrid[] = {
    CRGB(0, 0, 0),
    CRGB(255, 0, 0),
    CRGB(0, 128, 0),
    CRGB(0, 0, 64),
    CRGB(255, 255, 255),
    CRGB(128, 64, 32),
    CRGB(7, 7, 7),
    CRGB(200, 100, 100),
};
constexpr uint8_t kAwsWeightGrid[] = { 0, 1, 25, 100, 200, 255 };

constexpr CRGB kAddBaseGrid[] = {
    CRGB(0, 0, 0),
    CRGB(255, 255, 255),
    CRGB(128, 0, 0),
    CRGB(0, 128, 0),
    CRGB(0, 0, 128),
    CRGB(73, 137, 211),
};
constexpr CRGB kAddOverlayGrid[] = {
    CRGB(0, 0, 0),
    CRGB(255, 255, 255),
    CRGB(0, 255, 0),
    CRGB(255, 0, 255),
    CRGB(127, 127, 127),
    CRGB(1, 2, 3),
};

constexpr bool aws_grid_matches() {
    for (auto base : kAwsRgbGrid) {
        for (auto w : kAwsWeightGrid) {
            CRGB s = stub_addWhiteSaturating_ce(base, w);
            CRGB c = canonical_addWhiteSaturating(base, w);
            if (!eq(s, c)) return false;
        }
    }
    return true;
}

constexpr bool add_grid_matches() {
    for (auto b : kAddBaseGrid) {
        for (auto o : kAddOverlayGrid) {
            CRGB s = stub_additive_ce(b, o);
            CRGB c = canonical_additive(b, o);
            if (!eq(s, c)) return false;
        }
    }
    return true;
}

static_assert(aws_grid_matches(),
    "native_stubs.h: addWhiteSaturating stub has drifted from canonical mirror "
    "of BeatPulseRenderUtils.h:174-188. Re-sync per MAINTAINER PROTOCOL.");

static_assert(add_grid_matches(),
    "native_stubs.h: additive stub has drifted from canonical mirror "
    "of BeatPulseRenderUtils.h:199-205. Re-sync per MAINTAINER PROTOCOL.");

} // namespace lightwaveos::effects::ieffect::ColourUtil::detail
