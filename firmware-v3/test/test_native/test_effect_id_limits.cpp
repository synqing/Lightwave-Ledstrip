// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * LightwaveOS v2 - Effect ID limit regression tests
 *
 * These tests protect against accidental reintroduction of hard caps that make
 * higher effect IDs (e.g. 77+) impossible to register or select.
 *
 * NOTE: This does not validate visual correctness on hardware; it validates
 * the control-plane constraints so effects can actually be reached.
 */

#include <unity.h>

#include "../../src/core/actors/RendererActor.h"
#include "../../src/core/state/SystemState.h"
#include "../../src/core/state/Commands.h"

using namespace lightwaveos;

static constexpr uint8_t kExpectedImplementedEffects = 85;  // Update alongside PatternRegistry/CoreEffects parity
static constexpr uint8_t kHighestPerlinEffectId = 84;        // Last Perlin ambient ID

void test_effect_id_caps_allow_perlin_suite() {
    TEST_ASSERT_TRUE_MESSAGE(
        actors::RendererActor::MAX_EFFECTS >= kExpectedImplementedEffects,
        "RendererActor::MAX_EFFECTS is too low; effects cannot register"
    );

    TEST_ASSERT_TRUE_MESSAGE(
        state::MAX_EFFECT_COUNT >= kExpectedImplementedEffects,
        "state::MAX_EFFECT_COUNT is too low; commands cannot select effects"
    );
}

void test_set_effect_command_accepts_high_effect_ids() {
    state::SystemState s;
    state::SetEffectCommand cmd(kHighestPerlinEffectId);
    TEST_ASSERT_TRUE_MESSAGE(
        cmd.validate(s),
        "SetEffectCommand rejected a valid high effect ID"
    );
}


