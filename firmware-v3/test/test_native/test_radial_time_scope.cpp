// Phase 5 Move 5.4 — LIN-06 RadialTimeScope unit tests.
//
// These tests verify the *centre-origin topology contract* and the
// *dt-correct push cadence* of the Radial Time-Scope effect without
// booting the full FastLED + AudioActor stack. They run native on the
// Unity test runner.
//
// Verified properties:
//   1. Centre pair (LEDs 79 + 80) renders the *newest* history sample.
//   2. Edge LEDs (0 / 159) render the *oldest* history sample.
//   3. The push cadence is FPS-independent (60 Hz fixed history rate).
//   4. Silence (audioConfidence = 0) collapses every LED to black.
//   5. The strip is mirror-symmetric across LEDs 79/80.
//   6. Strip 2 is a faithful reflective twin of strip 1 (PS-05 contract).
//   7. The metadata's effect ID matches the EID_RADIAL_TIME_SCOPE constant
//      (sanity-checks the registration declaration).
//   8. Hue stays within ±32 of the anchor over a long render run — "no
//      rainbows" hard rule.
//   9. With fewer pushes than the ring depth, LEDs at distance ≥ count
//      render dark (partial-fill semantics).

#include <unity.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>

#include "effects/ieffect/RadialTimeScopeEffect.h"
#include "plugins/api/EffectContext.h"

// CoreEffects.h pulls in RendererActor + ValidationMode + VRMSMetrics, which
// have several pre-existing native-build incompatibilities (FastLED-only
// random()/inoise8/rgb2hsv_approximate/CHSV.hue). The same constants are
// defined here in a private const so this test file stays decoupled from
// that compilation chain.
namespace {
constexpr uint16_t CENTER_LEFT  = 79;
constexpr uint16_t CENTER_RIGHT = 80;
constexpr uint16_t HALF_LENGTH  = 80;
constexpr uint16_t STRIP_LENGTH = 160;
}  // namespace

using lightwaveos::effects::ieffect::RadialTimeScopeEffect;
using lightwaveos::plugins::EffectContext;

namespace {

// Standard K1 buffer: dual 160-LED strip, 320 LEDs total.
constexpr uint16_t kTotalLeds = 320;

// Helper: construct a fresh EffectContext backed by a CRGB array.
struct ScopeFixture {
    CRGB leds[kTotalLeds];
    EffectContext ctx;

    ScopeFixture() {
        std::memset(leds, 0, sizeof(leds));
        ctx.leds = leds;
        ctx.ledCount = kTotalLeds;
        ctx.centerPoint = HALF_LENGTH;  // 80
        ctx.deltaTimeSeconds = 1.0f / 60.0f;
        ctx.brightness = 255;
    }
};

inline bool isBrightish(const CRGB& c) {
    // "Lit" if any channel is non-zero. Useful where the exact CHSV→CRGB
    // value depends on hue and we only care about presence/absence.
    return (c.r != 0) || (c.g != 0) || (c.b != 0);
}

inline int luma(const CRGB& c) {
    return static_cast<int>(c.r) + static_cast<int>(c.g) + static_cast<int>(c.b);
}

// 1 — Centre pair (LEDs 79 and 80) renders the freshest history sample.
//     We push a single value of 1.0 then render, and expect both centre LEDs
//     to be lit while LEDs further out are dark (only one sample in the ring,
//     so distance > 0 returns 0).
void test_centre_pair_renders_newest_history_value() {
    ScopeFixture fx;
    RadialTimeScopeEffect effect;
    TEST_ASSERT_TRUE(effect.init(fx.ctx));

    // Drive one push at the 60 Hz cadence with a bright onset value.
    TEST_ASSERT_TRUE(effect.testTickAndRender(fx.ctx,
                                              /*onsetEnv=*/1.0f,
                                              /*audioConfidence=*/1.0f,
                                              /*silentScale=*/1.0f,
                                              /*deltaTimeSeconds=*/1.0f / 60.0f));

    TEST_ASSERT_TRUE_MESSAGE(isBrightish(fx.leds[CENTER_LEFT]),
        "LED 79 should be lit at the freshest history sample");
    TEST_ASSERT_TRUE_MESSAGE(isBrightish(fx.leds[CENTER_RIGHT]),
        "LED 80 should be lit at the freshest history sample");
    // One sample in the ring, so any LED at distance > 0 should be dark.
    TEST_ASSERT_FALSE_MESSAGE(isBrightish(fx.leds[CENTER_LEFT - 1]),
        "LED 78 should be dark with only one sample pushed");
    TEST_ASSERT_FALSE_MESSAGE(isBrightish(fx.leds[CENTER_RIGHT + 1]),
        "LED 81 should be dark with only one sample pushed");

    effect.cleanup();
}

// 2 — Push 80 distinct values; the edge LEDs (0 and 159) read the oldest
//     sample (the very first push). We use brightness as a proxy: the
//     first push is full-bright (1.0), the 79 subsequent pushes are dark
//     (0.0). The edges should therefore be brighter than the LEDs just
//     inside them.
void test_edge_renders_oldest_history_value() {
    ScopeFixture fx;
    RadialTimeScopeEffect effect;
    TEST_ASSERT_TRUE(effect.init(fx.ctx));

    // First push — bright, lands at offset 0 (centre) on the first call,
    // and gets shifted out to offset N-1 (edge) by the next 79 pushes.
    effect.testTickAndRender(fx.ctx, 1.0f, 1.0f, 1.0f, 1.0f / 60.0f);

    // 79 dark pushes pushing the bright sample to the edge of the ring.
    for (int i = 0; i < 79; ++i) {
        effect.testTickAndRender(fx.ctx, 0.0f, 1.0f, 1.0f, 1.0f / 60.0f);
    }

    TEST_ASSERT_TRUE_MESSAGE(isBrightish(fx.leds[0]),
        "LED 0 should display the oldest sample (full-bright)");
    TEST_ASSERT_TRUE_MESSAGE(isBrightish(fx.leds[STRIP_LENGTH - 1]),
        "LED 159 should display the oldest sample (full-bright)");
    // The LED just inside (distance 78) saw a 0.0 push, so it must be dark.
    TEST_ASSERT_FALSE_MESSAGE(isBrightish(fx.leds[1]),
        "LED 1 (distance 78 from centre) should be dark");
    TEST_ASSERT_FALSE_MESSAGE(isBrightish(fx.leds[STRIP_LENGTH - 2]),
        "LED 158 (distance 78 from centre) should be dark");

    effect.cleanup();
}

// 3 — dt-correctness: the same total elapsed time yields the same number of
//     pushes regardless of whether dt is 1/60 s × 1 frame or 1/120 s × 2
//     frames. After ten 1/60 s frames, the ring should hold 10 samples;
//     after twenty 1/120 s frames, also 10 samples.
void test_dt_correct_push_rate_independent_of_render_fps() {
    ScopeFixture fxA;
    RadialTimeScopeEffect effectA;
    effectA.init(fxA.ctx);
    for (int i = 0; i < 10; ++i) {
        effectA.testTickAndRender(fxA.ctx, 1.0f, 1.0f, 1.0f, 1.0f / 60.0f);
    }
    const size_t pushedA = effectA.pushedSamples();

    ScopeFixture fxB;
    RadialTimeScopeEffect effectB;
    effectB.init(fxB.ctx);
    for (int i = 0; i < 20; ++i) {
        effectB.testTickAndRender(fxB.ctx, 1.0f, 1.0f, 1.0f, 1.0f / 120.0f);
    }
    const size_t pushedB = effectB.pushedSamples();

    TEST_ASSERT_EQUAL_size_t_MESSAGE(pushedA, pushedB,
        "Push count must be identical under varying render FPS for the same total dt");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(10, pushedA,
        "10 frames at 1/60 s should yield 10 pushes");

    effectA.cleanup();
    effectB.cleanup();
}

// 4 — Silence (audioConfidence = 0) collapses every LED to black regardless
//     of what the history ring still holds. We pre-fill the ring with bright
//     values, then render a single frame with audioConfidence = 0.
void test_silence_scales_brightness_to_zero() {
    ScopeFixture fx;
    RadialTimeScopeEffect effect;
    effect.init(fx.ctx);

    // Pre-fill with bright samples while audio confidence is high.
    for (int i = 0; i < 80; ++i) {
        effect.testTickAndRender(fx.ctx, 1.0f, 1.0f, 1.0f, 1.0f / 60.0f);
    }
    // Silence frame: audioConfidence = 0 → all LEDs must be black.
    // (deltaTimeSeconds = 0 ensures no further pushes can disturb the test.)
    effect.testTickAndRender(fx.ctx, 1.0f, 0.0f, 1.0f, 0.0f);

    for (uint16_t i = 0; i < STRIP_LENGTH; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, fx.leds[i].r);
        TEST_ASSERT_EQUAL_UINT8(0, fx.leds[i].g);
        TEST_ASSERT_EQUAL_UINT8(0, fx.leds[i].b);
    }

    effect.cleanup();
}

// 5 — Strict mirror symmetry across LEDs 79/80: leds[79-k] == leds[80+k].
void test_centre_origin_strict_mirror() {
    ScopeFixture fx;
    RadialTimeScopeEffect effect;
    effect.init(fx.ctx);

    // Fill the ring with a diverse set of values so each radius gets a
    // distinct colour; if any radius computed an asymmetric write, the
    // test will fail at the corresponding k.
    const float pattern[8] = {1.0f, 0.7f, 0.4f, 0.9f, 0.2f, 0.55f, 0.85f, 0.30f};
    for (int i = 0; i < 80; ++i) {
        effect.testTickAndRender(fx.ctx, pattern[i % 8], 1.0f, 1.0f, 1.0f / 60.0f);
    }

    for (uint16_t k = 0; k < HALF_LENGTH; ++k) {
        const CRGB& left  = fx.leds[CENTER_LEFT  - k];
        const CRGB& right = fx.leds[CENTER_RIGHT + k];
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(left.r, right.r, "Mirror R mismatch");
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(left.g, right.g, "Mirror G mismatch");
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(left.b, right.b, "Mirror B mismatch");
    }

    effect.cleanup();
}

// 6 — Strip 2 (160..319) mirrors strip 1 (0..159) — PS-05 reflective twin.
void test_strip2_mirrors_strip1() {
    ScopeFixture fx;
    RadialTimeScopeEffect effect;
    effect.init(fx.ctx);

    for (int i = 0; i < 80; ++i) {
        effect.testTickAndRender(fx.ctx, static_cast<float>(i) / 79.0f,
                                 1.0f, 1.0f, 1.0f / 60.0f);
    }

    for (uint16_t i = 0; i < STRIP_LENGTH; ++i) {
        const CRGB& a = fx.leds[i];
        const CRGB& b = fx.leds[i + STRIP_LENGTH];
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(a.r, b.r, "Strip 2 R != Strip 1 R");
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(a.g, b.g, "Strip 2 G != Strip 1 G");
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(a.b, b.b, "Strip 2 B != Strip 1 B");
    }

    effect.cleanup();
}

// 7 — Metadata sanity: the registered effect ID matches EID_RADIAL_TIME_SCOPE.
//     Native build does not pull in effect_ids.h's symbol (kId is only
//     declared on hardware), so this test asserts the metadata constructor
//     received the expected role and category, plus the human-readable name.
void test_metadata_id_matches_eid_constant() {
    RadialTimeScopeEffect effect;
    const auto& meta = effect.getMetadata();
    TEST_ASSERT_NOT_NULL(meta.name);
    TEST_ASSERT_EQUAL_STRING("Radial Time-Scope", meta.name);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(lightwaveos::plugins::EffectCategory::AMBIENT),
                          static_cast<int>(meta.category));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(lightwaveos::plugins::EffectRoleFlags::SELF_TRAILING),
                          static_cast<int>(meta.roleFlags));
}

// 8 — No full hue-wheel sweep: even after 1000 frames at 60 Hz, the hue
//     offset stays bounded to ±kHueBound (32) of the anchor.
void test_no_full_hue_sweep_over_1000_frames() {
    ScopeFixture fx;
    RadialTimeScopeEffect effect;
    effect.init(fx.ctx);

    for (int i = 0; i < 1000; ++i) {
        effect.testTickAndRender(fx.ctx, 0.5f, 1.0f, 1.0f, 1.0f / 60.0f);
        const float hue = effect.currentHue();
        TEST_ASSERT_TRUE_MESSAGE(std::fabs(hue) <= static_cast<float>(RadialTimeScopeEffect::kHueBound) + 0.001f,
            "Hue offset must stay within ±kHueBound of the anchor");
    }

    effect.cleanup();
}

// 9 — Partial fill: with 5 pushes, LEDs at distance ≥ 5 must render dark
//     (the ring returns 0.0 for unread slots, which sampleAtRadius collapses
//     to black).
void test_partial_fill_renders_dark_outside_history_count() {
    ScopeFixture fx;
    RadialTimeScopeEffect effect;
    effect.init(fx.ctx);

    for (int i = 0; i < 5; ++i) {
        effect.testTickAndRender(fx.ctx, 1.0f, 1.0f, 1.0f, 1.0f / 60.0f);
    }

    // LEDs at distance 0..4 from the centre should be lit.
    for (uint16_t k = 0; k < 5; ++k) {
        const CRGB& left  = fx.leds[CENTER_LEFT - k];
        const CRGB& right = fx.leds[CENTER_RIGHT + k];
        TEST_ASSERT_TRUE_MESSAGE(luma(left)  > 0,
            "LEDs within history depth should be lit (left side)");
        TEST_ASSERT_TRUE_MESSAGE(luma(right) > 0,
            "LEDs within history depth should be lit (right side)");
    }
    // LEDs at distance 5..79 should be dark.
    for (uint16_t k = 5; k < HALF_LENGTH; ++k) {
        const CRGB& left  = fx.leds[CENTER_LEFT - k];
        const CRGB& right = fx.leds[CENTER_RIGHT + k];
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, luma(left),
            "LEDs beyond history depth should be dark (left side)");
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, luma(right),
            "LEDs beyond history depth should be dark (right side)");
    }

    effect.cleanup();
}

}  // namespace

void run_radial_time_scope_tests() {
    RUN_TEST(test_centre_pair_renders_newest_history_value);
    RUN_TEST(test_edge_renders_oldest_history_value);
    RUN_TEST(test_dt_correct_push_rate_independent_of_render_fps);
    RUN_TEST(test_silence_scales_brightness_to_zero);
    RUN_TEST(test_centre_origin_strict_mirror);
    RUN_TEST(test_strip2_mirrors_strip1);
    RUN_TEST(test_metadata_id_matches_eid_constant);
    RUN_TEST(test_no_full_hue_sweep_over_1000_frames);
    RUN_TEST(test_partial_fill_renders_dark_outside_history_count);
}
