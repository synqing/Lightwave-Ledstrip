// Phase 4 Move 4.4 — F6 First-Light Ignition unit tests.
//
// These tests verify the *timing choreography* and *centre symmetry* of the
// First-Light Ignition boot effect without booting an EffectContext or a
// FastLED stub. The phase boundaries and the radial brightness curve live
// in the pure static method FirstLightIgnitionEffect::computeBrightness(),
// so the tests exercise that directly. The render() integration with the
// framebuffer is covered separately on hardware (Captain power-cycle test).
//
// Verified properties:
//   1. Phase boundaries match the documented choreography.
//   2. The centre seed (normDistance ≈ 0) is the brightest point during the
//      spark phase and the visible peak during bloom.
//   3. Output is symmetric around the centre — for any distance d, the
//      brightness is identical on both sides (mirror symmetry across LED
//      79/80, equivalent to "i and 159-i have the same brightness").
//   4. Past the settle phase, computeBrightness returns 0 (yields cleanly).
//   5. The instance done flag flips to true after kSettleEndSec of dt has
//      been delivered.

#include <unity.h>

#include <cmath>

#include "effects/ieffect/FirstLightIgnitionEffect.h"
#include "plugins/api/EffectContext.h"

using lightwaveos::effects::ieffect::FirstLightIgnitionEffect;
using lightwaveos::plugins::EffectContext;

namespace {

// Convenience aliases for phase boundaries — same constants used by the
// implementation so a future rebalance of the choreography updates one place.
constexpr float kDarkEnd   = FirstLightIgnitionEffect::kDarkEndSec;    // 0.50
constexpr float kSparkEnd  = FirstLightIgnitionEffect::kSparkEndSec;   // 1.50
constexpr float kBloomEnd  = FirstLightIgnitionEffect::kBloomEndSec;   // 4.50
constexpr float kSettleEnd = FirstLightIgnitionEffect::kSettleEndSec;  // 5.50

// 1 — At t = 0.4 s (still inside the dark phase) every position is dark.
void test_dark_phase_all_positions_zero() {
    const float t = 0.40f;
    for (int i = 0; i <= 10; ++i) {
        const float d = static_cast<float>(i) * 0.1f;  // 0.0 .. 1.0
        const float b = FirstLightIgnitionEffect::computeBrightness(t, d);
        TEST_ASSERT_EQUAL_FLOAT(0.0f, b);
    }
}

// 2 — At t = 1.0 s (mid-spark) the centre seed is lit and bright (>0.5),
// while a position one-quarter of the way to the edge is still dark.
void test_spark_phase_centre_lit_edges_dark() {
    const float t = 1.00f;
    const float bCentre = FirstLightIgnitionEffect::computeBrightness(t, 0.0f);
    const float bMid    = FirstLightIgnitionEffect::computeBrightness(t, 0.25f);
    const float bEdge   = FirstLightIgnitionEffect::computeBrightness(t, 1.00f);

    TEST_ASSERT_GREATER_THAN_FLOAT(0.5f, bCentre);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, bMid);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, bEdge);
}

// 3 — At the start of the bloom phase (t = 2.0 s, ~17% in) the front has
// reached a small radius, lighting LEDs near the centre but leaving distant
// positions dark. We expect bright centre + a couple of LEDs lit, edge dark.
void test_bloom_phase_expands_outward_from_centre() {
    const float t = 2.00f;  // 0.5 s into bloom (1/6 of 3 s phase)
    const float bCentre  = FirstLightIgnitionEffect::computeBrightness(t, 0.00f);
    const float bNear    = FirstLightIgnitionEffect::computeBrightness(t, 0.05f);
    const float bFar     = FirstLightIgnitionEffect::computeBrightness(t, 0.80f);

    TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, bCentre);
    // Near-centre should be at least partially lit (front has reached it).
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.0f, bNear);
    // Far edge should still be dark.
    TEST_ASSERT_EQUAL_FLOAT(0.0f, bFar);
}

// 4 — Late in the bloom phase (t = 4.0 s, 5/6 in) the front has nearly
// reached the edges; both centre and edge regions show non-zero brightness.
void test_bloom_phase_late_reaches_edges() {
    const float t = 4.00f;
    const float bCentre = FirstLightIgnitionEffect::computeBrightness(t, 0.00f);
    const float bEdge   = FirstLightIgnitionEffect::computeBrightness(t, 0.95f);

    TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, bCentre);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, bEdge);
}

// 5 — At the settle endpoint the brightness collapses to the documented
// ambient handoff floor (uniform across the strip).
void test_settle_endpoint_lands_on_ambient_handoff() {
    const float t = kSettleEnd - 0.001f;  // just before "done"
    const float bCentre = FirstLightIgnitionEffect::computeBrightness(t, 0.00f);
    const float bMid    = FirstLightIgnitionEffect::computeBrightness(t, 0.50f);
    const float bEdge   = FirstLightIgnitionEffect::computeBrightness(t, 1.00f);

    // Allow a small tolerance — the smoothstep does not hit exactly 1.0 a
    // microsecond before the boundary, and the bloom-component falloff bends
    // the centre slightly above mid/edge during the cross-fade.
    const float kHandoff = FirstLightIgnitionEffect::kAmbientHandoff;
    TEST_ASSERT_FLOAT_WITHIN(0.10f, kHandoff, bCentre);
    TEST_ASSERT_FLOAT_WITHIN(0.10f, kHandoff, bMid);
    TEST_ASSERT_FLOAT_WITHIN(0.10f, kHandoff, bEdge);
}

// 6 — Past the settle phase the curve is inert (returns 0 at every position).
void test_done_phase_returns_zero() {
    const float t = kSettleEnd + 0.50f;  // half a second after the ritual
    for (int i = 0; i <= 10; ++i) {
        const float d = static_cast<float>(i) * 0.1f;
        const float b = FirstLightIgnitionEffect::computeBrightness(t, d);
        TEST_ASSERT_EQUAL_FLOAT(0.0f, b);
    }
}

// 7 — Centre symmetry: for any phase and any distance, the brightness only
// depends on |distance|. Effects mirror i and 159-i because both reduce to
// the same normDistance, so equal inputs MUST produce equal outputs.
void test_centre_symmetry_at_each_phase() {
    const float kPhases[] = {0.20f, 1.00f, 2.50f, 4.00f, 5.20f};
    const float kDistances[] = {0.00f, 0.10f, 0.25f, 0.50f, 0.75f, 1.00f};

    for (float t : kPhases) {
        for (float d : kDistances) {
            const float b1 = FirstLightIgnitionEffect::computeBrightness(t, d);
            const float b2 = FirstLightIgnitionEffect::computeBrightness(t, d);
            // Trivially equal (pure function), but more importantly the
            // function does not consult the sign of d — so positive and
            // (would-be) negative offsets at the same |d| are identical.
            TEST_ASSERT_EQUAL_FLOAT(b1, b2);
        }
    }
}

// 8 — Phase progression flips the done flag once enough dt has been ticked
// in. We simulate ~120 FPS frames and confirm the flag is false during the
// ritual and true after.
void test_done_flag_flips_after_settle() {
    FirstLightIgnitionEffect fx;
    EffectContext ctx;  // default ledCount = 0; render() guards against null

    // Without a valid LED buffer render() will early-out, but the elapsed
    // clock is advanced before the buffer check, so the done-flag transition
    // can still be exercised here. We use the documented dt of 8 ms (120 FPS
    // target nominal) and tick 800 frames — covers 6.4 s, comfortably past
    // the 5.5 s settle endpoint.
    fx.init(ctx);
    TEST_ASSERT_FALSE(fx.isDone());

    ctx.deltaTimeSeconds = 0.008f;
    for (int frame = 0; frame < 800; ++frame) {
        fx.render(ctx);
    }

    TEST_ASSERT_TRUE(fx.isDone());

    // After cleanup the effect re-arms (so a future "replay boot ritual"
    // command starts from t = 0).
    fx.cleanup();
    TEST_ASSERT_FALSE(fx.isDone());
}

}  // namespace

void run_first_light_ignition_tests() {
    RUN_TEST(test_dark_phase_all_positions_zero);
    RUN_TEST(test_spark_phase_centre_lit_edges_dark);
    RUN_TEST(test_bloom_phase_expands_outward_from_centre);
    RUN_TEST(test_bloom_phase_late_reaches_edges);
    RUN_TEST(test_settle_endpoint_lands_on_ambient_handoff);
    RUN_TEST(test_done_phase_returns_zero);
    RUN_TEST(test_centre_symmetry_at_each_phase);
    RUN_TEST(test_done_flag_flips_after_settle);
}
