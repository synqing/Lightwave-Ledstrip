// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * LightwaveOS v2 - Unity Test Runner
 *
 * Main entry point for native unit tests. Runs all test suites and
 * reports results.
 *
 * Build: pio run -e native_test
 * Run: .pio/build/native_test/program
 */

#include <unity.h>
#include <cstdio>

// Test suite declarations
extern void run_state_store_tests();
extern void run_hal_led_tests();
extern void run_actor_tests();
extern void run_effect_tests();
extern void run_zone_composer_tests();
extern void run_transition_tests();
extern void run_integration_tests();
extern void run_sync_tests();
extern void run_subscription_manager_tests();

// Unity setUp/tearDown (required but can be empty)
void setUp(void) {
    // Called before each test
}

void tearDown(void) {
    // Called after each test
}

/**
 * Main test runner
 *
 * Executes all test suites in sequence and reports aggregate results.
 */
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  LightwaveOS v2 - Native Unit Test Suite\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");

    UNITY_BEGIN();

    printf("───────────────────────────────────────────────────────────────\n");
    printf("  State Store Tests (CQRS Pattern)\n");
    printf("───────────────────────────────────────────────────────────────\n");
    run_state_store_tests();

    printf("\n───────────────────────────────────────────────────────────────\n");
    printf("  HAL LED Driver Tests\n");
    printf("───────────────────────────────────────────────────────────────\n");
    run_hal_led_tests();

    printf("\n───────────────────────────────────────────────────────────────\n");
    printf("  Actor System Tests\n");
    printf("───────────────────────────────────────────────────────────────\n");
    run_actor_tests();

    printf("\n───────────────────────────────────────────────────────────────\n");
    printf("  Effect Rendering Tests (CENTER ORIGIN)\n");
    printf("───────────────────────────────────────────────────────────────\n");
    run_effect_tests();

    printf("\n───────────────────────────────────────────────────────────────\n");
    printf("  Zone Composer Tests (Buffer Isolation + Blend Modes)\n");
    printf("───────────────────────────────────────────────────────────────\n");
    run_zone_composer_tests();

    printf("\n───────────────────────────────────────────────────────────────\n");
    printf("  Transition Tests (12 Types + 15 Easing Curves)\n");
    printf("───────────────────────────────────────────────────────────────\n");
    run_transition_tests();

    printf("\n───────────────────────────────────────────────────────────────\n");
    printf("  Integration Tests (End-to-End Pipeline)\n");
    printf("───────────────────────────────────────────────────────────────\n");
    run_integration_tests();

    printf("\n───────────────────────────────────────────────────────────────\n");
    printf("  Multi-Device Sync Tests (CQRS Replication)\n");
    printf("───────────────────────────────────────────────────────────────\n");
    run_sync_tests();

    printf("\n───────────────────────────────────────────────────────────────\n");
    printf("  Subscription Manager Tests (WebSocket)\n");
    printf("───────────────────────────────────────────────────────────────\n");
    run_subscription_manager_tests();

    printf("\n═══════════════════════════════════════════════════════════════\n");
    int result = UNITY_END();
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");

    if (result == 0) {
        printf("✓ All tests passed!\n\n");
    } else {
        printf("✗ Some tests failed. See details above.\n\n");
    }

    return result;
}
