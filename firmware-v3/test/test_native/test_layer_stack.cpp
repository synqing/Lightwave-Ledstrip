// Phase 1 Move 1.4 — INF-01 LayerStack composer test
//
// Per Topology_Reconciliation §5: LayerStack is the overlap-permitted
// sibling-class to ZoneComposer. Where ZoneComposer partitions the strip
// into 1..3 disjoint zones, LayerStack stacks N back-to-front layers
// across the FULL strip with per-layer blend mode + per-layer alpha
// (the τ exposure required by the INF-02 mandatory pass).
//
// The composer is geometry-agnostic — these tests use a tiny 4-pixel
// strip (kStripLen=4) and N=3 layers to keep arithmetic transparent.
// Centre-origin and audio-reactivity rules are not directly applicable
// to a pure pixel-stacking utility.
//
// All entry points must:
//   - perform NO heap allocation (called transitively from render path)
//   - be cheap (bounded by N × kStripLen pixel ops)
//   - be FastLED-mock-friendly (no qadd8/scale8 dependency)

#include <unity.h>
#include <cstddef>
#include <cstdint>

#include "effects/composers/LayerStack.h"

using lightwaveos::effects::composers::LayerStack;
using lightwaveos::effects::composers::LayerBlendMode;

namespace {

constexpr size_t kN   = 3;
constexpr size_t kLen = 4;
using TinyStack = LayerStack<kN, kLen>;

// Helper — fill a 4-pixel scratch buffer with a single colour.
void fillSolid(CRGB* buf, uint8_t r, uint8_t g, uint8_t b) {
    for (size_t i = 0; i < kLen; ++i) {
        buf[i] = CRGB(r, g, b);
    }
}

// ─── 1: Default state composites to all-black ─────────────────────────────
void test_default_state_yields_black() {
    TinyStack stack;
    CRGB dest[kLen];
    // Pre-poison dest so we can confirm compositeInto cleared it.
    fillSolid(dest, 99, 88, 77);
    stack.compositeInto(dest);
    TEST_ASSERT_EQUAL_size_t(0u, stack.layerCount());
    for (size_t i = 0; i < kLen; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, dest[i].r);
        TEST_ASSERT_EQUAL_UINT8(0, dest[i].g);
        TEST_ASSERT_EQUAL_UINT8(0, dest[i].b);
    }
}

// ─── 2: Single layer OVERWRITE at alpha=1 echoes the source ──────────────
void test_single_layer_overwrite_matches_source() {
    TinyStack stack;
    CRGB src[kLen];
    fillSolid(src, 200, 100, 50);
    stack.setLayer(0, LayerBlendMode::OVERWRITE, 1.0f);
    stack.writeLayer(0, src);

    CRGB dest[kLen];
    stack.compositeInto(dest);
    TEST_ASSERT_EQUAL_size_t(1u, stack.layerCount());
    for (size_t i = 0; i < kLen; ++i) {
        TEST_ASSERT_EQUAL_UINT8(200, dest[i].r);
        TEST_ASSERT_EQUAL_UINT8(100, dest[i].g);
        TEST_ASSERT_EQUAL_UINT8(50,  dest[i].b);
    }
}

// ─── 3: Two-layer ADDITIVE saturates within uint8_t range ────────────────
//
// layer 0 (BASE, OVERWRITE @ alpha=1.0) = (100, 50, 25)
// layer 1 (ADDITIVE, alpha=1.0)         = (200, 100, 250)
// expected: clamp(layer0 + layer1) = (255, 150, 255)
void test_two_layer_additive_clamps() {
    TinyStack stack;
    CRGB base[kLen], add[kLen];
    fillSolid(base, 100, 50, 25);
    fillSolid(add,  200, 100, 250);
    stack.setLayer(0, LayerBlendMode::OVERWRITE, 1.0f);
    stack.writeLayer(0, base);
    stack.setLayer(1, LayerBlendMode::ADDITIVE, 1.0f);
    stack.writeLayer(1, add);

    CRGB dest[kLen];
    stack.compositeInto(dest);
    TEST_ASSERT_EQUAL_size_t(2u, stack.layerCount());
    for (size_t i = 0; i < kLen; ++i) {
        TEST_ASSERT_EQUAL_UINT8(255, dest[i].r);  // 100+200=300 → clamp 255
        TEST_ASSERT_EQUAL_UINT8(150, dest[i].g);  // 50+100=150
        TEST_ASSERT_EQUAL_UINT8(255, dest[i].b);  // 25+250=275 → clamp 255
    }
}

// ─── 4: Two-layer ALPHA lerps base toward layer ──────────────────────────
//
// layer 0 = (0, 0, 0); layer 1 = (100, 200, 50); alpha=0.5
// expected: lerp = (50, 100, 25)
void test_two_layer_alpha_lerps() {
    TinyStack stack;
    CRGB base[kLen], top[kLen];
    fillSolid(base, 0, 0, 0);
    fillSolid(top,  100, 200, 50);
    stack.setLayer(0, LayerBlendMode::OVERWRITE, 1.0f);
    stack.writeLayer(0, base);
    stack.setLayer(1, LayerBlendMode::ALPHA, 0.5f);
    stack.writeLayer(1, top);

    CRGB dest[kLen];
    stack.compositeInto(dest);
    for (size_t i = 0; i < kLen; ++i) {
        TEST_ASSERT_EQUAL_UINT8(50,  dest[i].r);
        TEST_ASSERT_EQUAL_UINT8(100, dest[i].g);
        TEST_ASSERT_EQUAL_UINT8(25,  dest[i].b);
    }
}

// ─── 5: Three-layer MULTIPLY chain at alpha=1 ─────────────────────────────
//
// layer 0 = (255, 200, 128); layer 1 multiplies by (200, 200, 200);
// layer 2 multiplies by (128, 128, 128).
//
// step1 = (255*200/255, 200*200/255, 128*200/255) = (200, 156, 100)
// step2 = (200*128/255, 156*128/255, 100*128/255) = (100, 78,  50)
//
// (Integer division truncates per the canonical (a*b)/255 contract;
// allow ±1 LSB tolerance to absorb that floor.)
void test_three_layer_multiply() {
    TinyStack stack;
    CRGB l0[kLen], l1[kLen], l2[kLen];
    fillSolid(l0, 255, 200, 128);
    fillSolid(l1, 200, 200, 200);
    fillSolid(l2, 128, 128, 128);
    stack.setLayer(0, LayerBlendMode::OVERWRITE, 1.0f);
    stack.writeLayer(0, l0);
    stack.setLayer(1, LayerBlendMode::MULTIPLY, 1.0f);
    stack.writeLayer(1, l1);
    stack.setLayer(2, LayerBlendMode::MULTIPLY, 1.0f);
    stack.writeLayer(2, l2);

    CRGB dest[kLen];
    stack.compositeInto(dest);
    TEST_ASSERT_EQUAL_size_t(3u, stack.layerCount());
    for (size_t i = 0; i < kLen; ++i) {
        TEST_ASSERT_INT_WITHIN(1, 100, dest[i].r);
        TEST_ASSERT_INT_WITHIN(1, 78,  dest[i].g);
        TEST_ASSERT_INT_WITHIN(1, 50,  dest[i].b);
    }
}

// ─── 6: Per-layer alpha=0 erases that layer's contribution ───────────────
//
// layer 0 = (10, 20, 30) at alpha=1 (BASE) → dest = (10, 20, 30).
// layer 1 ADDITIVE alpha=0 must NOT modify dest.
// layer 2 ADDITIVE alpha=1 of (5, 5, 5) → dest = (15, 25, 35).
void test_per_layer_alpha_zero_erases() {
    TinyStack stack;
    CRGB l0[kLen], l1[kLen], l2[kLen];
    fillSolid(l0, 10, 20, 30);
    fillSolid(l1, 100, 100, 100);  // would saturate channels if alpha mattered
    fillSolid(l2, 5, 5, 5);
    stack.setLayer(0, LayerBlendMode::OVERWRITE, 1.0f);
    stack.writeLayer(0, l0);
    stack.setLayer(1, LayerBlendMode::ADDITIVE, 0.0f);
    stack.writeLayer(1, l1);
    stack.setLayer(2, LayerBlendMode::ADDITIVE, 1.0f);
    stack.writeLayer(2, l2);

    CRGB dest[kLen];
    stack.compositeInto(dest);
    for (size_t i = 0; i < kLen; ++i) {
        TEST_ASSERT_EQUAL_UINT8(15, dest[i].r);
        TEST_ASSERT_EQUAL_UINT8(25, dest[i].g);
        TEST_ASSERT_EQUAL_UINT8(35, dest[i].b);
    }
}

// ─── 7: setLayer(idx >= kMaxLayers) is a defensive no-op ─────────────────
void test_set_layer_out_of_range_is_noop() {
    TinyStack stack;
    // kN==3 → idx 3 and 99 are both out of range.
    stack.setLayer(kN,      LayerBlendMode::ADDITIVE, 0.5f);
    stack.setLayer(kN + 96, LayerBlendMode::MULTIPLY, 1.0f);
    TEST_ASSERT_EQUAL_size_t(0u, stack.layerCount());

    // writeLayer on the same out-of-range index must also be benign.
    CRGB src[kLen];
    fillSolid(src, 1, 2, 3);
    stack.writeLayer(kN, src);

    // Default-state composite still yields black.
    CRGB dest[kLen];
    fillSolid(dest, 99, 99, 99);
    stack.compositeInto(dest);
    for (size_t i = 0; i < kLen; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, dest[i].r);
        TEST_ASSERT_EQUAL_UINT8(0, dest[i].g);
        TEST_ASSERT_EQUAL_UINT8(0, dest[i].b);
    }
}

}  // namespace

void run_layer_stack_tests() {
    RUN_TEST(test_default_state_yields_black);
    RUN_TEST(test_single_layer_overwrite_matches_source);
    RUN_TEST(test_two_layer_additive_clamps);
    RUN_TEST(test_two_layer_alpha_lerps);
    RUN_TEST(test_three_layer_multiply);
    RUN_TEST(test_per_layer_alpha_zero_erases);
    RUN_TEST(test_set_layer_out_of_range_is_noop);
}
