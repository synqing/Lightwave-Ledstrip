/**
 * LightwaveOS v2 - State Store Unit Tests
 *
 * Tests for the CQRS state management system including:
 * - Immutable state updates
 * - Command dispatch
 * - State versioning
 * - Thread-safe state transitions
 * - Subscriber notifications
 */

#include <unity.h>

// Mock FreeRTOS for native builds
#ifdef NATIVE_BUILD
#include "mocks/freertos_mock.h"
#endif

#include "../../src/core/state/SystemState.h"
#include "../../src/core/state/StateStore.h"
#include "../../src/core/state/Commands.h"

using namespace lightwaveos::state;

//==============================================================================
// Test Fixtures
//==============================================================================

static bool subscriberCalled = false;
static SystemState lastNotifiedState;

void resetTestState() {
    subscriberCalled = false;
    lastNotifiedState = SystemState();
#ifdef NATIVE_BUILD
    freertos_mock::reset();
#endif
}

void testSubscriber(const SystemState& newState) {
    subscriberCalled = true;
    lastNotifiedState = newState;
}

//==============================================================================
// SystemState Tests - Immutability and Functional Updates
//==============================================================================

void test_initial_state_has_valid_defaults() {
    SystemState state;

    TEST_ASSERT_EQUAL_UINT32(0, state.version);
    TEST_ASSERT_EQUAL_UINT8(0, state.currentEffectId);
    TEST_ASSERT_EQUAL_UINT8(128, state.brightness);  // 50% brightness (safer for LEDs)
    TEST_ASSERT_EQUAL_UINT8(15, state.speed);
    TEST_ASSERT_FALSE(state.zoneModeEnabled);
    TEST_ASSERT_EQUAL_UINT8(1, state.activeZoneCount);
    TEST_ASSERT_FALSE(state.transitionActive);
}

void test_with_effect_creates_new_state() {
    SystemState state;
    SystemState newState = state.withEffect(5);

    // New state should have updated values
    TEST_ASSERT_EQUAL_UINT8(5, newState.currentEffectId);
    TEST_ASSERT_EQUAL_UINT32(1, newState.version);

    // Original state should be unchanged (immutability)
    TEST_ASSERT_EQUAL_UINT8(0, state.currentEffectId);
    TEST_ASSERT_EQUAL_UINT32(0, state.version);
}

void test_with_brightness_creates_new_state() {
    SystemState state;
    SystemState newState = state.withBrightness(200);

    TEST_ASSERT_EQUAL_UINT8(200, newState.brightness);
    TEST_ASSERT_EQUAL_UINT32(1, newState.version);
    TEST_ASSERT_EQUAL_UINT8(128, state.brightness);  // Original unchanged (default is 128)
}

void test_with_speed_creates_new_state() {
    SystemState state;
    SystemState newState = state.withSpeed(25);

    TEST_ASSERT_EQUAL_UINT8(25, newState.speed);
    TEST_ASSERT_EQUAL_UINT32(1, newState.version);
    TEST_ASSERT_EQUAL_UINT8(15, state.speed);  // Original unchanged
}

void test_with_palette_creates_new_state() {
    SystemState state;
    SystemState newState = state.withPalette(3);

    TEST_ASSERT_EQUAL_UINT8(3, newState.currentPaletteId);
    TEST_ASSERT_EQUAL_UINT32(1, newState.version);
}

void test_with_zone_mode_creates_new_state() {
    SystemState state;
    SystemState newState = state.withZoneMode(true, 4);

    TEST_ASSERT_TRUE(newState.zoneModeEnabled);
    TEST_ASSERT_EQUAL_UINT8(4, newState.activeZoneCount);
    TEST_ASSERT_EQUAL_UINT32(1, newState.version);
    TEST_ASSERT_FALSE(state.zoneModeEnabled);  // Original unchanged
}

void test_with_zone_effect_creates_new_state() {
    SystemState state;
    SystemState newState = state.withZoneEffect(0, 7);

    TEST_ASSERT_EQUAL_UINT8(7, newState.zones[0].effectId);
    // Note: withZoneEffect only sets effectId, use withZoneEnabled to enable
    TEST_ASSERT_EQUAL_UINT32(1, newState.version);
}

void test_chained_updates_increment_version() {
    SystemState state;

    SystemState state1 = state.withEffect(1);
    TEST_ASSERT_EQUAL_UINT32(1, state1.version);

    SystemState state2 = state1.withBrightness(200);
    TEST_ASSERT_EQUAL_UINT32(2, state2.version);

    SystemState state3 = state2.withSpeed(30);
    TEST_ASSERT_EQUAL_UINT32(3, state3.version);

    // Check all values are preserved
    TEST_ASSERT_EQUAL_UINT8(1, state3.currentEffectId);
    TEST_ASSERT_EQUAL_UINT8(200, state3.brightness);
    TEST_ASSERT_EQUAL_UINT8(30, state3.speed);
}

//==============================================================================
// StateStore Tests - Command Dispatch
//==============================================================================

void test_state_store_initial_state() {
    resetTestState();
    StateStore store;

    const SystemState& state = store.getState();
    TEST_ASSERT_EQUAL_UINT32(0, state.version);
    TEST_ASSERT_EQUAL_UINT8(0, state.currentEffectId);
}

void test_state_store_dispatch_set_effect() {
    resetTestState();
    StateStore store;

    SetEffectCommand cmd(7);
    bool success = store.dispatch(cmd);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_UINT8(7, store.getCurrentEffect());
    TEST_ASSERT_EQUAL_UINT32(1, store.getVersion());
}

void test_state_store_dispatch_set_brightness() {
    resetTestState();
    StateStore store;

    SetBrightnessCommand cmd(100);
    bool success = store.dispatch(cmd);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_UINT8(100, store.getBrightness());
}

void test_state_store_dispatch_set_speed() {
    resetTestState();
    StateStore store;

    SetSpeedCommand cmd(40);
    bool success = store.dispatch(cmd);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_UINT8(40, store.getSpeed());
}

void test_state_store_dispatch_set_palette() {
    resetTestState();
    StateStore store;

    SetPaletteCommand cmd(5);
    bool success = store.dispatch(cmd);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_UINT8(5, store.getCurrentPalette());
}

void test_state_store_dispatch_invalid_effect_fails() {
    resetTestState();
    StateStore store;

    // Effect ID 99 exceeds MAX_EFFECT_COUNT (64)
    SetEffectCommand cmd(99);
    bool success = store.dispatch(cmd);

    TEST_ASSERT_FALSE(success);
    TEST_ASSERT_EQUAL_UINT8(0, store.getCurrentEffect());  // Unchanged
}

void test_state_store_dispatch_invalid_speed_fails() {
    resetTestState();
    StateStore store;

    // Speed 0 is invalid (must be 1-50)
    SetSpeedCommand cmd(0);
    bool success = store.dispatch(cmd);

    TEST_ASSERT_FALSE(success);
    TEST_ASSERT_EQUAL_UINT8(15, store.getSpeed());  // Unchanged (default)
}

void test_state_store_dispatch_zone_commands() {
    resetTestState();
    StateStore store;

    // Enable zone mode with 2 zones
    SetZoneModeCommand modeCmd(true, 2);
    TEST_ASSERT_TRUE(store.dispatch(modeCmd));
    TEST_ASSERT_TRUE(store.isZoneModeEnabled());
    TEST_ASSERT_EQUAL_UINT8(2, store.getActiveZoneCount());

    // Set effect for zone 0
    ZoneSetEffectCommand effectCmd(0, 5);
    TEST_ASSERT_TRUE(store.dispatch(effectCmd));

    ZoneState zone0 = store.getZoneConfig(0);
    TEST_ASSERT_EQUAL_UINT8(5, zone0.effectId);
    // Note: ZoneSetEffectCommand only sets effectId, zone enabled state is separate
}

//==============================================================================
// StateStore Tests - Batch Dispatch
//==============================================================================

void test_state_store_batch_dispatch_all_succeed() {
    resetTestState();
    StateStore store;

    SetEffectCommand cmd1(3);
    SetBrightnessCommand cmd2(150);
    SetSpeedCommand cmd3(20);

    const ICommand* commands[] = {&cmd1, &cmd2, &cmd3};
    bool success = store.dispatchBatch(commands, 3);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_UINT8(3, store.getCurrentEffect());
    TEST_ASSERT_EQUAL_UINT8(150, store.getBrightness());
    TEST_ASSERT_EQUAL_UINT8(20, store.getSpeed());
}

void test_state_store_batch_dispatch_fails_atomically() {
    resetTestState();
    StateStore store;

    SetEffectCommand cmd1(3);
    SetSpeedCommand cmd2(0);  // Invalid - speed must be 1-50
    SetBrightnessCommand cmd3(100);

    const ICommand* commands[] = {&cmd1, &cmd2, &cmd3};
    bool success = store.dispatchBatch(commands, 3);

    TEST_ASSERT_FALSE(success);
    // State should be unchanged (default brightness is 128)
    TEST_ASSERT_EQUAL_UINT8(0, store.getCurrentEffect());
    TEST_ASSERT_EQUAL_UINT8(128, store.getBrightness());
}

//==============================================================================
// StateStore Tests - Subscribers
//==============================================================================

void test_state_store_subscriber_notification() {
    resetTestState();
    StateStore store;

    TEST_ASSERT_TRUE(store.subscribe(testSubscriber));
    TEST_ASSERT_EQUAL_UINT8(1, store.getSubscriberCount());

    SetEffectCommand cmd(10);
    store.dispatch(cmd);

    TEST_ASSERT_TRUE(subscriberCalled);
    TEST_ASSERT_EQUAL_UINT8(10, lastNotifiedState.currentEffectId);
}

void test_state_store_multiple_subscribers() {
    resetTestState();
    StateStore store;

    static int callCount = 0;
    callCount = 0;

    auto subscriber1 = [](const SystemState&) { callCount++; };
    auto subscriber2 = [](const SystemState&) { callCount++; };

    store.subscribe(subscriber1);
    store.subscribe(subscriber2);
    TEST_ASSERT_EQUAL_UINT8(2, store.getSubscriberCount());

    SetEffectCommand cmd(5);
    store.dispatch(cmd);

    TEST_ASSERT_EQUAL_INT(2, callCount);
}

void test_state_store_unsubscribe() {
    resetTestState();
    StateStore store;

    store.subscribe(testSubscriber);
    TEST_ASSERT_EQUAL_UINT8(1, store.getSubscriberCount());

    bool unsubscribed = store.unsubscribe(testSubscriber);
    TEST_ASSERT_TRUE(unsubscribed);
    TEST_ASSERT_EQUAL_UINT8(0, store.getSubscriberCount());

    SetEffectCommand cmd(5);
    store.dispatch(cmd);

    TEST_ASSERT_FALSE(subscriberCalled);  // Should not be called
}

//==============================================================================
// StateStore Tests - Thread Safety (Double Buffering)
//==============================================================================

void test_state_store_concurrent_reads_during_write() {
    resetTestState();
    StateStore store;

    // Set initial effect
    SetEffectCommand cmd1(1);
    store.dispatch(cmd1);

    // Simulate concurrent read during write
    // In real usage, reads never block on writes due to double buffering
    const SystemState& state1 = store.getState();

    SetEffectCommand cmd2(2);
    store.dispatch(cmd2);

    const SystemState& state2 = store.getState();

    // Both reads should succeed and be valid
    TEST_ASSERT_EQUAL_UINT8(1, state1.currentEffectId);
    TEST_ASSERT_EQUAL_UINT8(2, state2.currentEffectId);
}

//==============================================================================
// Command Tests - Validation
//==============================================================================

void test_command_validation() {
    resetTestState();
    SystemState state;

    // Valid commands
    SetEffectCommand validEffect(10);
    TEST_ASSERT_TRUE(validEffect.validate(state));

    SetSpeedCommand validSpeed(25);
    TEST_ASSERT_TRUE(validSpeed.validate(state));

    // Invalid commands
    SetEffectCommand invalidEffect(100);  // > MAX_EFFECT_COUNT
    TEST_ASSERT_FALSE(invalidEffect.validate(state));

    SetSpeedCommand invalidSpeed(0);  // < 1
    TEST_ASSERT_FALSE(invalidSpeed.validate(state));

    SetSpeedCommand invalidSpeedHigh(51);  // > 50
    TEST_ASSERT_FALSE(invalidSpeedHigh.validate(state));
}

//==============================================================================
// Test Suite Runner
//==============================================================================

void run_state_store_tests() {
    // SystemState immutability tests
    RUN_TEST(test_initial_state_has_valid_defaults);
    RUN_TEST(test_with_effect_creates_new_state);
    RUN_TEST(test_with_brightness_creates_new_state);
    RUN_TEST(test_with_speed_creates_new_state);
    RUN_TEST(test_with_palette_creates_new_state);
    RUN_TEST(test_with_zone_mode_creates_new_state);
    RUN_TEST(test_with_zone_effect_creates_new_state);
    RUN_TEST(test_chained_updates_increment_version);

    // StateStore dispatch tests
    RUN_TEST(test_state_store_initial_state);
    RUN_TEST(test_state_store_dispatch_set_effect);
    RUN_TEST(test_state_store_dispatch_set_brightness);
    RUN_TEST(test_state_store_dispatch_set_speed);
    RUN_TEST(test_state_store_dispatch_set_palette);
    RUN_TEST(test_state_store_dispatch_invalid_effect_fails);
    RUN_TEST(test_state_store_dispatch_invalid_speed_fails);
    RUN_TEST(test_state_store_dispatch_zone_commands);

    // Batch dispatch tests
    RUN_TEST(test_state_store_batch_dispatch_all_succeed);
    RUN_TEST(test_state_store_batch_dispatch_fails_atomically);

    // Subscriber tests
    RUN_TEST(test_state_store_subscriber_notification);
    RUN_TEST(test_state_store_multiple_subscribers);
    RUN_TEST(test_state_store_unsubscribe);

    // Thread safety tests
    RUN_TEST(test_state_store_concurrent_reads_during_write);

    // Command validation tests
    RUN_TEST(test_command_validation);
}
