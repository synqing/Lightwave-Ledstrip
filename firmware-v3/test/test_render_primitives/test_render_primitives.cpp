// Pipeline-reform Phase 1+2 substrate tests.
//
// Verifies the three Layer 4 render primitives (drawDot, drawSpriteScrolled,
// fillFromBins) and the Layer 5 frame post-process (applyFrameBlending).
// Pure additive — no existing effects are touched. The whole point of this
// substrate is to behave correctly under unit-test conditions BEFORE Phase 5
// integrates it into RendererActor and the four spazz effects.
//
// All primitives must:
//   - perform NO heap allocation (called transitively from render path)
//   - be dt-correct (alpha/blend exponentiated against 120 FPS reference)
//   - be cheap (well under the 2.0 ms per-frame ceiling for 320 LEDs)
//   - work native + ESP32-S3 (no platform-specific intrinsics)
//   - render centre-origin (LED 79+80 outward for K1 standard config)
//
// British English in comments and identifiers.

#include <unity.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

#include "effects/render/RenderPrimitives.h"
#include "effects/render/FrameBlend.h"

using lightwaveos::effects::render::drawDot;
using lightwaveos::effects::render::drawSpriteScrolled;
using lightwaveos::effects::render::fillFromBins;
using lightwaveos::effects::render::applyFrameBlending;

namespace {

// ─── Heap-allocation tracking ────────────────────────────────────────────────
//
// Global new/delete are overridden at the bottom of this TU. The counter is
// kept inside the anonymous namespace so it is unique to this test module. A
// helper resets the counter and exposes it for the no-heap regression test.
int g_heapAllocations = 0;

inline void resetHeapCounter() { g_heapAllocations = 0; }
inline int heapCount() { return g_heapAllocations; }

// ─── Test fixtures ───────────────────────────────────────────────────────────

constexpr uint16_t kLedCount = 160;     // single K1 strip
constexpr uint16_t kCentre = 80;        // K1 standard centrePoint

CRGB g_buf[kLedCount];
CRGB g_prevFrame[kLedCount];

void clearBuf(CRGB* buf, uint16_t n) {
    for (uint16_t i = 0; i < n; ++i) buf[i] = CRGB::Black;
}

bool isBlack(const CRGB& c) {
    return c.r == 0 && c.g == 0 && c.b == 0;
}

bool nonBlack(const CRGB& c) { return !isBlack(c); }

// Sum total brightness in a buffer (rough magnitude metric).
uint32_t totalBrightness(const CRGB* buf, uint16_t n) {
    uint32_t sum = 0;
    for (uint16_t i = 0; i < n; ++i) {
        sum += buf[i].r;
        sum += buf[i].g;
        sum += buf[i].b;
    }
    return sum;
}

// ────────────────────────────────────────────────────────────────────────────
// drawDot tests
// ────────────────────────────────────────────────────────────────────────────

// 1 — drawDot centre placement: pos=0 lights LED 79+80, all others black.
void test_drawDot_centre_placement() {
    clearBuf(g_buf, kLedCount);
    drawDot(g_buf, kLedCount, kCentre, /*pos=*/0.0f, CRGB::White,
            /*opacity=*/1.0f, /*prevPos=*/-1.0f, /*mirror=*/true);
    TEST_ASSERT_TRUE(nonBlack(g_buf[79]));
    TEST_ASSERT_TRUE(nonBlack(g_buf[80]));
    for (uint16_t i = 0; i < kLedCount; ++i) {
        if (i == 79 || i == 80) continue;
        TEST_ASSERT_TRUE_MESSAGE(isBlack(g_buf[i]),
                                 "drawDot centre placement leaked outside the centre pair");
    }
}

// 2 — drawDot edge placement: pos=1 lights LEDs 0 and 159, centre dark.
void test_drawDot_edge_placement() {
    clearBuf(g_buf, kLedCount);
    drawDot(g_buf, kLedCount, kCentre, /*pos=*/1.0f, CRGB::White,
            /*opacity=*/1.0f, /*prevPos=*/-1.0f, /*mirror=*/true);
    TEST_ASSERT_TRUE(nonBlack(g_buf[0]));
    TEST_ASSERT_TRUE(nonBlack(g_buf[kLedCount - 1]));
    TEST_ASSERT_TRUE(isBlack(g_buf[79]));
    TEST_ASSERT_TRUE(isBlack(g_buf[80]));
}

// 3 — drawDot sub-pixel: pos=0.5 distributes brightness across adjacent LEDs.
//     Right side lands at index 80 + 0.5*79 = 119.5 → LEDs 119 and 120 split.
void test_drawDot_sub_pixel_interpolation() {
    clearBuf(g_buf, kLedCount);
    drawDot(g_buf, kLedCount, kCentre, /*pos=*/0.5f, CRGB::White,
            /*opacity=*/1.0f, /*prevPos=*/-1.0f, /*mirror=*/true);
    // Both LED 119 and 120 should be lit (sub-pixel split).
    TEST_ASSERT_TRUE(nonBlack(g_buf[119]));
    TEST_ASSERT_TRUE(nonBlack(g_buf[120]));
    // Mirrored: 79 - 39.5 = 39.5 → LED 39 and 40 split.
    TEST_ASSERT_TRUE(nonBlack(g_buf[39]));
    TEST_ASSERT_TRUE(nonBlack(g_buf[40]));
    // Neither pixel should be brighter than the other by more than 1 raw count
    // (rounding). Means the split is roughly equal.
    const int diff = static_cast<int>(g_buf[119].r) - static_cast<int>(g_buf[120].r);
    TEST_ASSERT_INT_WITHIN(2, 0, diff);
}

// 4 — drawDot motion trail: prev=0.2, pos=0.4 fills LEDs between the two
//     positions with non-zero brightness. Expected sweep on the right side:
//     80+0.2*79=95.8 → 80+0.4*79=111.6, so LEDs 96..111 all should be lit.
void test_drawDot_motion_trail_fills_between() {
    clearBuf(g_buf, kLedCount);
    drawDot(g_buf, kLedCount, kCentre, /*pos=*/0.4f, CRGB::White,
            /*opacity=*/1.0f, /*prevPos=*/0.2f, /*mirror=*/true);
    int litCount = 0;
    for (uint16_t i = 96; i <= 111; ++i) {
        if (nonBlack(g_buf[i])) ++litCount;
    }
    // Expect at least 12 of the 16 inter-position LEDs to be lit (the trail
    // line plus rounding may leave 1-2 dark inside; the contract is "between
    // positions has non-zero brightness", not "every inter-position LED").
    TEST_ASSERT_GREATER_OR_EQUAL_INT(12, litCount);
}

// 5 — drawDot mirror=false: only the right half (centrePoint outward) is
//     touched; the left half stays black.
void test_drawDot_mirror_false() {
    clearBuf(g_buf, kLedCount);
    drawDot(g_buf, kLedCount, kCentre, /*pos=*/0.5f, CRGB::White,
            /*opacity=*/1.0f, /*prevPos=*/-1.0f, /*mirror=*/false);
    // Right half should have at least one lit LED.
    bool rightLit = false;
    for (uint16_t i = kCentre; i < kLedCount; ++i) {
        if (nonBlack(g_buf[i])) { rightLit = true; break; }
    }
    TEST_ASSERT_TRUE(rightLit);
    // Left half (0..79) must be entirely black.
    for (uint16_t i = 0; i < kCentre; ++i) {
        TEST_ASSERT_TRUE_MESSAGE(isBlack(g_buf[i]),
                                 "drawDot mirror=false should not touch the left half");
    }
}

// ────────────────────────────────────────────────────────────────────────────
// drawSpriteScrolled tests
// ────────────────────────────────────────────────────────────────────────────

// 6 — drawSpriteScrolled outward: seed centre pair, scrollAmount=1, alpha=1,
//     verify centre pair has moved outward by exactly 1 LED.
void test_drawSpriteScrolled_outward_one_pixel() {
    clearBuf(g_buf, kLedCount);
    g_buf[79] = CRGB::White;
    g_buf[80] = CRGB::White;
    drawSpriteScrolled(g_buf, kLedCount, kCentre,
                       /*scrollAmount=*/1.0f, /*alpha=*/1.0f,
                       /*dt=*/1.0f / 120.0f);
    // After the scroll: LEDs 78 and 81 should be lit; 79 and 80 should be black.
    TEST_ASSERT_TRUE(nonBlack(g_buf[78]));
    TEST_ASSERT_TRUE(nonBlack(g_buf[81]));
    TEST_ASSERT_TRUE(isBlack(g_buf[79]));
    TEST_ASSERT_TRUE(isBlack(g_buf[80]));
}

// 7 — drawSpriteScrolled alpha decay: stationary scroll (scrollAmount=0) with
//     alpha=0.9 reduces brightness each call.
void test_drawSpriteScrolled_alpha_decay() {
    clearBuf(g_buf, kLedCount);
    g_buf[79] = CRGB::White;
    g_buf[80] = CRGB::White;
    const uint32_t b0 = totalBrightness(g_buf, kLedCount);
    drawSpriteScrolled(g_buf, kLedCount, kCentre,
                       /*scrollAmount=*/0.0f, /*alpha=*/0.9f,
                       /*dt=*/1.0f / 120.0f);
    const uint32_t b1 = totalBrightness(g_buf, kLedCount);
    drawSpriteScrolled(g_buf, kLedCount, kCentre,
                       0.0f, 0.9f, 1.0f / 120.0f);
    const uint32_t b2 = totalBrightness(g_buf, kLedCount);
    TEST_ASSERT_LESS_THAN_UINT32(b0, b1);
    TEST_ASSERT_LESS_THAN_UINT32(b1, b2);
}

// 8 — drawSpriteScrolled dt-correction: two calls at dt=1/240 should produce
//     the same total brightness as one call at dt=1/120 (with caller scaling
//     scrollAmount linearly by dt).
void test_drawSpriteScrolled_dt_independence() {
    // Path A: one call at dt = 1/120 s.
    clearBuf(g_buf, kLedCount);
    g_buf[79] = CRGB::White;
    g_buf[80] = CRGB::White;
    drawSpriteScrolled(g_buf, kLedCount, kCentre,
                       /*scrollAmount=*/2.0f / 120.0f,  // 2 px/s × dt
                       /*alpha=*/0.9f,
                       /*dt=*/1.0f / 120.0f);
    const uint32_t bA = totalBrightness(g_buf, kLedCount);

    // Path B: two calls at dt = 1/240 s each.
    CRGB bufB[kLedCount];
    for (uint16_t i = 0; i < kLedCount; ++i) bufB[i] = CRGB::Black;
    bufB[79] = CRGB::White;
    bufB[80] = CRGB::White;
    drawSpriteScrolled(bufB, kLedCount, kCentre,
                       2.0f / 240.0f, 0.9f, 1.0f / 240.0f);
    drawSpriteScrolled(bufB, kLedCount, kCentre,
                       2.0f / 240.0f, 0.9f, 1.0f / 240.0f);
    const uint32_t bB = totalBrightness(bufB, kLedCount);

    // Total brightness should be within 5% — sub-pixel rounding error
    // accumulates differently across one vs two scrolls.
    const uint32_t diff = (bA > bB) ? (bA - bB) : (bB - bA);
    const uint32_t mag = (bA > bB) ? bA : bB;
    const uint32_t five_percent = mag / 20;
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(five_percent + 16, diff);
}

// ────────────────────────────────────────────────────────────────────────────
// fillFromBins tests
// ────────────────────────────────────────────────────────────────────────────

// 9 — fillFromBins centre-origin: 8 bins. bin[0] lights centre pair (79+80);
//     bin[7] lights edges (0 and 159); intermediate bins land in between.
void test_fillFromBins_centre_origin_mapping() {
    clearBuf(g_buf, kLedCount);
    CRGBPalette16 palette;
    for (int i = 0; i < 16; ++i) palette[i] = CRGB::White;
    float bins[8] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    fillFromBins(g_buf, kLedCount, kCentre,
                 bins, /*binCount=*/8, palette, /*brightness=*/255,
                 /*additive=*/false);
    // bin[0] → centre pair LIT.
    TEST_ASSERT_TRUE(nonBlack(g_buf[79]));
    TEST_ASSERT_TRUE(nonBlack(g_buf[80]));
    // bin[7] → edges LIT.
    TEST_ASSERT_TRUE(nonBlack(g_buf[0]));
    TEST_ASSERT_TRUE(nonBlack(g_buf[kLedCount - 1]));
}

// 10 — fillFromBins additive: pre-fill red, call with white palette and
//      additive=true, verify the previously-lit centre LEDs gained brightness
//      (saturating add via CRGB::operator+=).
void test_fillFromBins_additive_blends() {
    // Pre-fill the entire buffer with a dim red.
    for (uint16_t i = 0; i < kLedCount; ++i) g_buf[i] = CRGB(80, 0, 0);
    CRGBPalette16 palette;
    for (int i = 0; i < 16; ++i) palette[i] = CRGB(0, 0, 100);  // dim blue
    float bins[1] = {1.0f};
    fillFromBins(g_buf, kLedCount, kCentre,
                 bins, /*binCount=*/1, palette, /*brightness=*/255,
                 /*additive=*/true);
    // Centre pair: pre-existing red 80 stays; blue contribution adds.
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(80, g_buf[79].r);
    TEST_ASSERT_GREATER_THAN_UINT8(0, g_buf[79].b);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(80, g_buf[80].r);
    TEST_ASSERT_GREATER_THAN_UINT8(0, g_buf[80].b);
    // Outside the centre pair (bin 0 only writes to centre): red preserved,
    // blue still 0.
    TEST_ASSERT_EQUAL_UINT8(80, g_buf[0].r);
    TEST_ASSERT_EQUAL_UINT8(0, g_buf[0].b);
}

// ────────────────────────────────────────────────────────────────────────────
// applyFrameBlending tests
// ────────────────────────────────────────────────────────────────────────────

// 11 — applyFrameBlending mood=0: output equals input verbatim, prevFrame
//      synchronised to current.
void test_applyFrameBlending_mood_zero_passthrough() {
    clearBuf(g_buf, kLedCount);
    clearBuf(g_prevFrame, kLedCount);
    // Seed prevFrame with red, leds with green.
    for (uint16_t i = 0; i < kLedCount; ++i) {
        g_prevFrame[i] = CRGB(255, 0, 0);
        g_buf[i] = CRGB(0, 255, 0);
    }
    applyFrameBlending(g_buf, g_prevFrame, kLedCount, /*mood=*/0,
                       /*dt=*/1.0f / 120.0f);
    // Output equals input (green).
    for (uint16_t i = 0; i < kLedCount; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, g_buf[i].r);
        TEST_ASSERT_EQUAL_UINT8(255, g_buf[i].g);
        TEST_ASSERT_EQUAL_UINT8(0, g_buf[i].b);
        // prevFrame synchronised to current.
        TEST_ASSERT_EQUAL_UINT8(0, g_prevFrame[i].r);
        TEST_ASSERT_EQUAL_UINT8(255, g_prevFrame[i].g);
    }
}

// 12 — applyFrameBlending mood=255: output heavily favours prevFrame at one
//      120-FPS frame interval. With prev=red and curr=green, the output
//      should be dominated by red (≥ 0.92 weight per the formula).
void test_applyFrameBlending_mood_max_persistence() {
    for (uint16_t i = 0; i < kLedCount; ++i) {
        g_prevFrame[i] = CRGB(255, 0, 0);
        g_buf[i] = CRGB(0, 255, 0);
    }
    applyFrameBlending(g_buf, g_prevFrame, kLedCount, /*mood=*/255,
                       /*dt=*/1.0f / 120.0f);
    // Expected: r ≈ 255 × 0.92 = 234; g ≈ 255 × 0.08 = 20.4 → 20.
    // Use ±3 tolerance for rounding.
    TEST_ASSERT_INT_WITHIN(3, 234, g_buf[0].r);
    TEST_ASSERT_INT_WITHIN(3, 20, g_buf[0].g);
    // prevFrame is the new blended value, ready for next call.
    TEST_ASSERT_EQUAL_UINT8(g_buf[0].r, g_prevFrame[0].r);
    TEST_ASSERT_EQUAL_UINT8(g_buf[0].g, g_prevFrame[0].g);
}

// 13 — applyFrameBlending dt-independence: two calls at dt=1/240 should
//      converge to the same result as one call at dt=1/120.
void test_applyFrameBlending_dt_independence() {
    // Path A: one call at dt = 1/120.
    CRGB bufA[kLedCount];
    CRGB prevA[kLedCount];
    for (uint16_t i = 0; i < kLedCount; ++i) {
        prevA[i] = CRGB(200, 0, 0);
        bufA[i] = CRGB(0, 200, 0);
    }
    applyFrameBlending(bufA, prevA, kLedCount, 200, 1.0f / 120.0f);

    // Path B: two calls at dt = 1/240, current frame held constant.
    CRGB bufB[kLedCount];
    CRGB prevB[kLedCount];
    for (uint16_t i = 0; i < kLedCount; ++i) {
        prevB[i] = CRGB(200, 0, 0);
        bufB[i] = CRGB(0, 200, 0);
    }
    applyFrameBlending(bufB, prevB, kLedCount, 200, 1.0f / 240.0f);
    // Hold the current frame steady for the second iteration (the blend has
    // already absorbed the previous prev).
    for (uint16_t i = 0; i < kLedCount; ++i) bufB[i] = CRGB(0, 200, 0);
    applyFrameBlending(bufB, prevB, kLedCount, 200, 1.0f / 240.0f);

    // Output should match within 3 raw counts per channel (rounding tolerance).
    TEST_ASSERT_INT_WITHIN(3, bufA[0].r, bufB[0].r);
    TEST_ASSERT_INT_WITHIN(3, bufA[0].g, bufB[0].g);
}

// ────────────────────────────────────────────────────────────────────────────
// No-heap test
// ────────────────────────────────────────────────────────────────────────────

// 14 — No primitive performs heap allocation. Override of global new/delete
//      counts allocations across the call. Must be 0.
void test_no_heap_allocation_across_primitives() {
    CRGBPalette16 palette;
    for (int i = 0; i < 16; ++i) palette[i] = CRGB::White;
    float bins[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    clearBuf(g_buf, kLedCount);
    clearBuf(g_prevFrame, kLedCount);

    resetHeapCounter();

    drawDot(g_buf, kLedCount, kCentre, 0.3f, CRGB::White, 0.8f, 0.1f, true);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, heapCount(),
                                  "drawDot allocated on the heap");

    drawSpriteScrolled(g_buf, kLedCount, kCentre, 0.5f, 0.95f, 1.0f / 120.0f);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, heapCount(),
                                  "drawSpriteScrolled allocated on the heap");

    fillFromBins(g_buf, kLedCount, kCentre, bins, 8, palette, 200, false);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, heapCount(),
                                  "fillFromBins allocated on the heap");

    applyFrameBlending(g_buf, g_prevFrame, kLedCount, 128, 1.0f / 120.0f);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, heapCount(),
                                  "applyFrameBlending allocated on the heap");
}

}  // namespace

// ─── Global new/delete override (counts allocations) ─────────────────────────
//
// Defined at namespace scope so they replace the global ::operator new/delete
// across the entire test executable. The counter lives in the anonymous
// namespace above; the operators reach into it via the bare names because the
// namespace is the same TU.

void* operator new(std::size_t s) {
    g_heapAllocations++;
    return std::malloc(s);
}
void* operator new[](std::size_t s) {
    g_heapAllocations++;
    return std::malloc(s);
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

// ─── Test runner ─────────────────────────────────────────────────────────────

void run_render_primitives_tests() {
    RUN_TEST(test_drawDot_centre_placement);
    RUN_TEST(test_drawDot_edge_placement);
    RUN_TEST(test_drawDot_sub_pixel_interpolation);
    RUN_TEST(test_drawDot_motion_trail_fills_between);
    RUN_TEST(test_drawDot_mirror_false);
    RUN_TEST(test_drawSpriteScrolled_outward_one_pixel);
    RUN_TEST(test_drawSpriteScrolled_alpha_decay);
    RUN_TEST(test_drawSpriteScrolled_dt_independence);
    RUN_TEST(test_fillFromBins_centre_origin_mapping);
    RUN_TEST(test_fillFromBins_additive_blends);
    RUN_TEST(test_applyFrameBlending_mood_zero_passthrough);
    RUN_TEST(test_applyFrameBlending_mood_max_persistence);
    RUN_TEST(test_applyFrameBlending_dt_independence);
    RUN_TEST(test_no_heap_allocation_across_primitives);
}
