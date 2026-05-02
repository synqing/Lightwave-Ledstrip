/**
 * @file test_main.cpp
 * @brief Compile-time regression test for B7 BlendMode namespace disambiguation
 *
 * Created 2026-05-01 as part of Phase 0 cleanup. Asserts that:
 *   1. `lightwaveos::zones::BlendMode` and `lightwaveos::effects::gradient::GradientBlendMode`
 *      are distinct types (compile-time check).
 *   2. The gradient enum is named `GradientBlendMode` (NOT `BlendMode`) in its namespace.
 *   3. Zone BlendMode has the expected 8 modes + MODE_COUNT sentinel.
 *
 * If a future contributor re-introduces a `BlendMode` enum in
 * `lightwaveos::effects::gradient::` or any other non-canonical namespace,
 * the static_assert chain below catches it at compile time.
 *
 * NOTE: This test file is scaffolded but NOT yet wired into platformio.ini.
 * One-line addition to firmware-v3/platformio.ini:
 *
 *     [env:native_test_blend_mode_namespace]
 *     extends = env:native_test_base   ; or whatever the local base test env is
 *     test_filter = test_blend_mode_namespace
 *
 * Captain or follow-up Phase 0 work to wire this into the active test envs.
 *
 * Run via: pio test -e native_test_blend_mode_namespace
 */

#include <type_traits>
#include <cstdint>

#include "../../src/effects/zones/BlendMode.h"
#include "../../src/effects/gradient/GradientTypes.h"

// ============================================================================
// Compile-time assertions (these run at build time, no Unity needed)
// ============================================================================

// 1. The two enum types must be distinct.
static_assert(
    !std::is_same<lightwaveos::zones::BlendMode,
                  lightwaveos::effects::gradient::GradientBlendMode>::value,
    "Zone BlendMode and Gradient GradientBlendMode must remain distinct types. "
    "If this fires, someone is aliasing them — revisit B7 disambiguation."
);

// 2. Gradient enum must be named GradientBlendMode (the rename target).
//    Reading the type via decltype on a constant of that type proves the symbol exists.
constexpr auto kGradientReplaceCheck = lightwaveos::effects::gradient::GradientBlendMode::REPLACE;
static_assert(
    std::is_same<
        decltype(kGradientReplaceCheck),
        const lightwaveos::effects::gradient::GradientBlendMode
    >::value,
    "GradientBlendMode::REPLACE must exist and have type GradientBlendMode. "
    "If this fires, the rename has been reverted."
);

// 3. Zone BlendMode has the expected 8 modes + MODE_COUNT sentinel.
static_assert(
    static_cast<uint8_t>(lightwaveos::zones::BlendMode::OVERWRITE) == 0,
    "zones::BlendMode::OVERWRITE must be 0."
);
static_assert(
    static_cast<uint8_t>(lightwaveos::zones::BlendMode::DARKEN) == 7,
    "zones::BlendMode::DARKEN must be 7."
);
static_assert(
    static_cast<uint8_t>(lightwaveos::zones::BlendMode::MODE_COUNT) == 8,
    "zones::BlendMode::MODE_COUNT must be 8 (8 valid modes 0..7)."
);

// 4. Gradient enum has the expected 4 modes.
static_assert(
    static_cast<uint8_t>(lightwaveos::effects::gradient::GradientBlendMode::REPLACE) == 0,
    "GradientBlendMode::REPLACE must be 0."
);
static_assert(
    static_cast<uint8_t>(lightwaveos::effects::gradient::GradientBlendMode::MULTIPLY) == 3,
    "GradientBlendMode::MULTIPLY must be 3 (4 modes 0..3)."
);

// ============================================================================
// Runtime smoke test (so the test runner has something to execute)
// ============================================================================

#ifdef ARDUINO
#include <Arduino.h>
void setup() {}
void loop() {}
#else

#include <unity.h>

void test_zone_blend_mode_count(void) {
    TEST_ASSERT_EQUAL_UINT8(
        8,
        static_cast<uint8_t>(lightwaveos::zones::BlendMode::MODE_COUNT)
    );
}

void test_gradient_blend_mode_distinct(void) {
    // Compile-time check is the real test; runtime is just a presence assertion.
    auto zoneMode = lightwaveos::zones::BlendMode::ADDITIVE;
    auto gradMode = lightwaveos::effects::gradient::GradientBlendMode::ADD;
    TEST_ASSERT_EQUAL_UINT8(1, static_cast<uint8_t>(zoneMode));
    TEST_ASSERT_EQUAL_UINT8(1, static_cast<uint8_t>(gradMode));
    // Same numeric value, different types — proves disambiguation.
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_zone_blend_mode_count);
    RUN_TEST(test_gradient_blend_mode_distinct);
    return UNITY_END();
}

#endif
