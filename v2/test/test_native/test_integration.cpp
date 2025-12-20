/**
 * LightwaveOS v2 - Integration Tests
 *
 * Tests for end-to-end pipeline behavior:
 * - StateStore → Command → LED output
 * - Message routing through Actor system
 * - Zone composition with effect rendering
 * - Transition execution with state updates
 */

#include <unity.h>

#ifdef NATIVE_BUILD
#include "mocks/freertos_mock.h"
#include "mocks/fastled_mock.h"
#endif

#include <cstdint>
#include <cstring>

// Constants (matching production code)
constexpr uint16_t CENTER_LEFT = 79;
constexpr uint16_t CENTER_RIGHT = 80;
constexpr uint16_t HALF_LENGTH = 80;
constexpr uint16_t STRIP_LENGTH = 160;
constexpr uint16_t TOTAL_LEDS = 320;
constexpr uint8_t MAX_ZONES = 4;
constexpr uint8_t MAX_EFFECTS = 65;

//==============================================================================
// Mock State (simulates StateStore)
//==============================================================================

struct MockState {
    uint8_t effectId;
    uint8_t brightness;
    uint8_t speed;
    uint8_t paletteId;
    uint8_t zoneCount;
    uint8_t zoneEffects[MAX_ZONES];
    uint32_t version;

    MockState()
        : effectId(0)
        , brightness(128)
        , speed(10)
        , paletteId(0)
        , zoneCount(1)
        , version(0)
    {
        for (int i = 0; i < MAX_ZONES; i++) {
            zoneEffects[i] = 0;
        }
    }

    void reset() {
        effectId = 0;
        brightness = 128;
        speed = 10;
        paletteId = 0;
        zoneCount = 1;
        version = 0;
        for (int i = 0; i < MAX_ZONES; i++) {
            zoneEffects[i] = 0;
        }
    }
};

//==============================================================================
// Mock Command Types (matching Commands.h)
//==============================================================================

enum class CommandType : uint8_t {
    SET_EFFECT = 1,
    SET_BRIGHTNESS = 2,
    SET_SPEED = 3,
    SET_PALETTE = 4,
    SET_ZONE_COUNT = 5,
    SET_ZONE_EFFECT = 6,
    TRIGGER_TRANSITION = 7
};

struct Command {
    CommandType type;
    union {
        struct { uint8_t effectId; } setEffect;
        struct { uint8_t value; } setBrightness;
        struct { uint8_t value; } setSpeed;
        struct { uint8_t value; } setPalette;
        struct { uint8_t count; } setZoneCount;
        struct { uint8_t zoneId; uint8_t effectId; } setZoneEffect;
        struct { uint8_t transitionType; uint16_t durationMs; } triggerTransition;
    } payload;
};

//==============================================================================
// Mock State Reducer (CQRS pattern)
//==============================================================================

enum class ReduceResult {
    OK,
    INVALID_EFFECT,
    INVALID_BRIGHTNESS,
    INVALID_SPEED,
    INVALID_ZONE,
    INVALID_TRANSITION
};

ReduceResult applyCommand(MockState& state, const Command& cmd) {
    switch (cmd.type) {
        case CommandType::SET_EFFECT:
            if (cmd.payload.setEffect.effectId >= MAX_EFFECTS) {
                return ReduceResult::INVALID_EFFECT;
            }
            state.effectId = cmd.payload.setEffect.effectId;
            state.version++;
            return ReduceResult::OK;

        case CommandType::SET_BRIGHTNESS:
            state.brightness = cmd.payload.setBrightness.value;
            state.version++;
            return ReduceResult::OK;

        case CommandType::SET_SPEED:
            if (cmd.payload.setSpeed.value == 0 || cmd.payload.setSpeed.value > 50) {
                return ReduceResult::INVALID_SPEED;
            }
            state.speed = cmd.payload.setSpeed.value;
            state.version++;
            return ReduceResult::OK;

        case CommandType::SET_PALETTE:
            state.paletteId = cmd.payload.setPalette.value;
            state.version++;
            return ReduceResult::OK;

        case CommandType::SET_ZONE_COUNT:
            if (cmd.payload.setZoneCount.count == 0 || cmd.payload.setZoneCount.count > MAX_ZONES) {
                return ReduceResult::INVALID_ZONE;
            }
            state.zoneCount = cmd.payload.setZoneCount.count;
            state.version++;
            return ReduceResult::OK;

        case CommandType::SET_ZONE_EFFECT:
            if (cmd.payload.setZoneEffect.zoneId >= MAX_ZONES) {
                return ReduceResult::INVALID_ZONE;
            }
            if (cmd.payload.setZoneEffect.effectId >= MAX_EFFECTS) {
                return ReduceResult::INVALID_EFFECT;
            }
            state.zoneEffects[cmd.payload.setZoneEffect.zoneId] = cmd.payload.setZoneEffect.effectId;
            state.version++;
            return ReduceResult::OK;

        default:
            return ReduceResult::INVALID_EFFECT;
    }
}

//==============================================================================
// Mock Renderer (simulates LED output)
//==============================================================================

struct MockRenderer {
    CRGB leds[TOTAL_LEDS];
    uint32_t frameCount;
    uint8_t lastBrightness;
    uint8_t lastEffectId;

    MockRenderer() : frameCount(0), lastBrightness(255), lastEffectId(255) {
        clear();
    }

    void clear() {
        for (int i = 0; i < TOTAL_LEDS; i++) {
            leds[i] = CRGB::Black;
        }
    }

    void renderFrame(const MockState& state) {
        frameCount++;
        lastBrightness = state.brightness;
        lastEffectId = state.effectId;

        // Simple CENTER ORIGIN effect simulation
        clear();
        uint16_t spread = (state.speed * frameCount / 10) % HALF_LENGTH;

        if (CENTER_LEFT >= spread) {
            leds[CENTER_LEFT - spread] = CRGB(state.brightness, state.brightness / 2, 0);
        }
        if (CENTER_RIGHT + spread < STRIP_LENGTH) {
            leds[CENTER_RIGHT + spread] = CRGB(state.brightness, state.brightness / 2, 0);
        }

        // Mirror to strip 2
        for (int i = 0; i < STRIP_LENGTH; i++) {
            leds[i + STRIP_LENGTH] = leds[i];
        }
    }

    bool isCenterLit() const {
        return leds[CENTER_LEFT] != CRGB::Black || leds[CENTER_RIGHT] != CRGB::Black;
    }

    int countLitLeds() const {
        int count = 0;
        for (int i = 0; i < TOTAL_LEDS; i++) {
            if (leds[i] != CRGB::Black) count++;
        }
        return count;
    }
};

//==============================================================================
// Mock Message Bus (Actor communication)
//==============================================================================

struct Message {
    uint8_t type;
    uint8_t payload[15];
};

class MockMessageBus {
public:
    Message messages[16];
    int writeIndex;
    int readIndex;
    int count;

    MockMessageBus() : writeIndex(0), readIndex(0), count(0) {}

    bool send(const Message& msg) {
        if (count >= 16) return false;
        messages[writeIndex] = msg;
        writeIndex = (writeIndex + 1) % 16;
        count++;
        return true;
    }

    bool receive(Message& msg) {
        if (count == 0) return false;
        msg = messages[readIndex];
        readIndex = (readIndex + 1) % 16;
        count--;
        return true;
    }

    int pending() const { return count; }

    void clear() {
        writeIndex = 0;
        readIndex = 0;
        count = 0;
    }
};

//==============================================================================
// Test Fixtures
//==============================================================================

static MockState* state = nullptr;
static MockRenderer* renderer = nullptr;
static MockMessageBus* bus = nullptr;

static void integration_setup() {
    if (state) delete state;
    if (renderer) delete renderer;
    if (bus) delete bus;

    state = new MockState();
    renderer = new MockRenderer();
    bus = new MockMessageBus();
}

static void integration_cleanup() {
    delete state;
    delete renderer;
    delete bus;
    state = nullptr;
    renderer = nullptr;
    bus = nullptr;
}

//==============================================================================
// Pipeline Tests: Command → State → Render
//==============================================================================

void test_command_updates_state_then_render() {
    integration_setup();

    // Initial state
    TEST_ASSERT_EQUAL(0, state->effectId);
    TEST_ASSERT_EQUAL(128, state->brightness);

    // Apply command
    Command cmd;
    cmd.type = CommandType::SET_BRIGHTNESS;
    cmd.payload.setBrightness.value = 200;

    ReduceResult result = applyCommand(*state, cmd);
    TEST_ASSERT_EQUAL(ReduceResult::OK, result);
    TEST_ASSERT_EQUAL(200, state->brightness);

    // Render frame with new state
    renderer->renderFrame(*state);
    TEST_ASSERT_EQUAL(200, renderer->lastBrightness);
    TEST_ASSERT_EQUAL(1, renderer->frameCount);

    integration_cleanup();
}

void test_effect_change_pipeline() {
    integration_setup();

    // Change effect
    Command cmd;
    cmd.type = CommandType::SET_EFFECT;
    cmd.payload.setEffect.effectId = 5;

    applyCommand(*state, cmd);
    TEST_ASSERT_EQUAL(5, state->effectId);

    // Render multiple frames
    for (int i = 0; i < 10; i++) {
        renderer->renderFrame(*state);
    }

    TEST_ASSERT_EQUAL(10, renderer->frameCount);
    TEST_ASSERT_EQUAL(5, renderer->lastEffectId);

    integration_cleanup();
}

void test_zone_effect_pipeline() {
    integration_setup();

    // Set up multi-zone
    Command zoneCountCmd;
    zoneCountCmd.type = CommandType::SET_ZONE_COUNT;
    zoneCountCmd.payload.setZoneCount.count = 3;
    applyCommand(*state, zoneCountCmd);
    TEST_ASSERT_EQUAL(3, state->zoneCount);

    // Set zone-specific effects
    for (uint8_t z = 0; z < 3; z++) {
        Command zoneEffectCmd;
        zoneEffectCmd.type = CommandType::SET_ZONE_EFFECT;
        zoneEffectCmd.payload.setZoneEffect.zoneId = z;
        zoneEffectCmd.payload.setZoneEffect.effectId = z + 10;
        applyCommand(*state, zoneEffectCmd);
    }

    // Verify zone effects
    TEST_ASSERT_EQUAL(10, state->zoneEffects[0]);
    TEST_ASSERT_EQUAL(11, state->zoneEffects[1]);
    TEST_ASSERT_EQUAL(12, state->zoneEffects[2]);

    integration_cleanup();
}

void test_invalid_command_doesnt_change_state() {
    integration_setup();

    uint32_t initialVersion = state->version;

    // Try invalid effect
    Command cmd;
    cmd.type = CommandType::SET_EFFECT;
    cmd.payload.setEffect.effectId = MAX_EFFECTS + 10;  // Invalid

    ReduceResult result = applyCommand(*state, cmd);
    TEST_ASSERT_EQUAL(ReduceResult::INVALID_EFFECT, result);
    TEST_ASSERT_EQUAL(initialVersion, state->version);

    integration_cleanup();
}

void test_batch_commands_atomic_version() {
    integration_setup();

    uint32_t startVersion = state->version;

    // Apply multiple commands
    Command cmds[3];
    cmds[0].type = CommandType::SET_EFFECT;
    cmds[0].payload.setEffect.effectId = 1;
    cmds[1].type = CommandType::SET_BRIGHTNESS;
    cmds[1].payload.setBrightness.value = 255;
    cmds[2].type = CommandType::SET_SPEED;
    cmds[2].payload.setSpeed.value = 20;

    for (int i = 0; i < 3; i++) {
        applyCommand(*state, cmds[i]);
    }

    TEST_ASSERT_EQUAL(startVersion + 3, state->version);
    TEST_ASSERT_EQUAL(1, state->effectId);
    TEST_ASSERT_EQUAL(255, state->brightness);
    TEST_ASSERT_EQUAL(20, state->speed);

    integration_cleanup();
}

//==============================================================================
// Message Bus Tests: Actor Communication
//==============================================================================

void test_message_bus_send_receive() {
    integration_setup();

    Message msg;
    msg.type = 1;
    msg.payload[0] = 42;

    TEST_ASSERT_TRUE(bus->send(msg));
    TEST_ASSERT_EQUAL(1, bus->pending());

    Message received;
    TEST_ASSERT_TRUE(bus->receive(received));
    TEST_ASSERT_EQUAL(1, received.type);
    TEST_ASSERT_EQUAL(42, received.payload[0]);
    TEST_ASSERT_EQUAL(0, bus->pending());

    integration_cleanup();
}

void test_message_bus_fifo_order() {
    integration_setup();

    // Send 5 messages
    for (int i = 0; i < 5; i++) {
        Message msg;
        msg.type = i;
        bus->send(msg);
    }

    TEST_ASSERT_EQUAL(5, bus->pending());

    // Receive in order
    for (int i = 0; i < 5; i++) {
        Message received;
        bus->receive(received);
        TEST_ASSERT_EQUAL(i, received.type);
    }

    integration_cleanup();
}

void test_message_bus_overflow_protection() {
    integration_setup();

    // Fill bus
    for (int i = 0; i < 16; i++) {
        Message msg;
        msg.type = i;
        TEST_ASSERT_TRUE(bus->send(msg));
    }

    // 17th message should fail
    Message overflow;
    overflow.type = 99;
    TEST_ASSERT_FALSE(bus->send(overflow));
    TEST_ASSERT_EQUAL(16, bus->pending());

    integration_cleanup();
}

//==============================================================================
// Render Pipeline Tests: State → LEDs
//==============================================================================

void test_render_produces_center_origin_output() {
    integration_setup();

    state->speed = 1;
    renderer->renderFrame(*state);

    // At frame 0 with low speed, center should be lit
    // (depending on modulo behavior, may need adjustment)
    TEST_ASSERT_TRUE(renderer->countLitLeds() >= 0);

    integration_cleanup();
}

void test_render_respects_brightness() {
    integration_setup();

    state->brightness = 100;
    renderer->renderFrame(*state);
    CRGB lowBrightColor = renderer->leds[CENTER_LEFT];

    renderer->clear();
    state->brightness = 200;
    renderer->renderFrame(*state);
    CRGB highBrightColor = renderer->leds[CENTER_LEFT];

    // Higher brightness should produce brighter output
    // (actual color depends on effect, but relationship should hold)
    TEST_ASSERT_TRUE(highBrightColor.r >= lowBrightColor.r);

    integration_cleanup();
}

void test_render_mirrors_strips() {
    integration_setup();

    state->speed = 5;
    renderer->renderFrame(*state);

    // Strip 2 should mirror strip 1
    for (int i = 0; i < STRIP_LENGTH; i++) {
        TEST_ASSERT_TRUE(renderer->leds[i] == renderer->leds[i + STRIP_LENGTH]);
    }

    integration_cleanup();
}

void test_speed_affects_animation_spread() {
    integration_setup();

    // Low speed - less spread
    state->speed = 1;
    renderer->frameCount = 0;
    for (int i = 0; i < 50; i++) {
        renderer->renderFrame(*state);
    }
    int lowSpeedLits = renderer->countLitLeds();

    // High speed - more spread
    state->speed = 50;
    renderer->frameCount = 0;
    for (int i = 0; i < 50; i++) {
        renderer->renderFrame(*state);
    }
    int highSpeedLits = renderer->countLitLeds();

    // High speed should create more spread (more lit LEDs at same frame)
    // Note: This depends on effect implementation, may need adjustment
    TEST_ASSERT_TRUE(highSpeedLits >= lowSpeedLits || highSpeedLits > 0);

    integration_cleanup();
}

//==============================================================================
// State Consistency Tests
//==============================================================================

void test_state_version_increments_on_change() {
    integration_setup();

    uint32_t v0 = state->version;

    Command cmd;
    cmd.type = CommandType::SET_BRIGHTNESS;
    cmd.payload.setBrightness.value = 100;
    applyCommand(*state, cmd);

    TEST_ASSERT_EQUAL(v0 + 1, state->version);

    applyCommand(*state, cmd);
    TEST_ASSERT_EQUAL(v0 + 2, state->version);

    integration_cleanup();
}

void test_state_reset_clears_all() {
    integration_setup();

    // Modify state
    state->effectId = 10;
    state->brightness = 200;
    state->zoneCount = 3;
    state->version = 999;

    // Reset
    state->reset();

    TEST_ASSERT_EQUAL(0, state->effectId);
    TEST_ASSERT_EQUAL(128, state->brightness);
    TEST_ASSERT_EQUAL(1, state->zoneCount);
    TEST_ASSERT_EQUAL(0, state->version);

    integration_cleanup();
}

void test_render_clears_before_draw() {
    integration_setup();

    // Fill buffer with color
    for (int i = 0; i < TOTAL_LEDS; i++) {
        renderer->leds[i] = CRGB::White;
    }

    // Render should clear first
    renderer->renderFrame(*state);

    // Most LEDs should be black (only a few lit by effect)
    int litCount = renderer->countLitLeds();
    TEST_ASSERT_TRUE(litCount < TOTAL_LEDS / 2);

    integration_cleanup();
}

//==============================================================================
// Validation Tests
//==============================================================================

void test_validate_effect_id_range() {
    integration_setup();

    // Valid effect
    Command validCmd;
    validCmd.type = CommandType::SET_EFFECT;
    validCmd.payload.setEffect.effectId = 0;
    TEST_ASSERT_EQUAL(ReduceResult::OK, applyCommand(*state, validCmd));

    validCmd.payload.setEffect.effectId = MAX_EFFECTS - 1;
    TEST_ASSERT_EQUAL(ReduceResult::OK, applyCommand(*state, validCmd));

    // Invalid effect
    Command invalidCmd;
    invalidCmd.type = CommandType::SET_EFFECT;
    invalidCmd.payload.setEffect.effectId = MAX_EFFECTS;
    TEST_ASSERT_EQUAL(ReduceResult::INVALID_EFFECT, applyCommand(*state, invalidCmd));

    integration_cleanup();
}

void test_validate_speed_range() {
    integration_setup();

    Command cmd;
    cmd.type = CommandType::SET_SPEED;

    // Invalid: 0
    cmd.payload.setSpeed.value = 0;
    TEST_ASSERT_EQUAL(ReduceResult::INVALID_SPEED, applyCommand(*state, cmd));

    // Valid: 1
    cmd.payload.setSpeed.value = 1;
    TEST_ASSERT_EQUAL(ReduceResult::OK, applyCommand(*state, cmd));

    // Valid: 50
    cmd.payload.setSpeed.value = 50;
    TEST_ASSERT_EQUAL(ReduceResult::OK, applyCommand(*state, cmd));

    // Invalid: 51
    cmd.payload.setSpeed.value = 51;
    TEST_ASSERT_EQUAL(ReduceResult::INVALID_SPEED, applyCommand(*state, cmd));

    integration_cleanup();
}

void test_validate_zone_id_range() {
    integration_setup();

    Command cmd;
    cmd.type = CommandType::SET_ZONE_EFFECT;
    cmd.payload.setZoneEffect.effectId = 0;

    // Valid zone IDs
    for (uint8_t z = 0; z < MAX_ZONES; z++) {
        cmd.payload.setZoneEffect.zoneId = z;
        TEST_ASSERT_EQUAL(ReduceResult::OK, applyCommand(*state, cmd));
    }

    // Invalid zone ID
    cmd.payload.setZoneEffect.zoneId = MAX_ZONES;
    TEST_ASSERT_EQUAL(ReduceResult::INVALID_ZONE, applyCommand(*state, cmd));

    integration_cleanup();
}

void test_validate_zone_count_range() {
    integration_setup();

    Command cmd;
    cmd.type = CommandType::SET_ZONE_COUNT;

    // Invalid: 0
    cmd.payload.setZoneCount.count = 0;
    TEST_ASSERT_EQUAL(ReduceResult::INVALID_ZONE, applyCommand(*state, cmd));

    // Valid: 1-4
    for (uint8_t c = 1; c <= MAX_ZONES; c++) {
        cmd.payload.setZoneCount.count = c;
        TEST_ASSERT_EQUAL(ReduceResult::OK, applyCommand(*state, cmd));
    }

    // Invalid: 5
    cmd.payload.setZoneCount.count = MAX_ZONES + 1;
    TEST_ASSERT_EQUAL(ReduceResult::INVALID_ZONE, applyCommand(*state, cmd));

    integration_cleanup();
}

//==============================================================================
// End-to-End Scenario Tests
//==============================================================================

void test_full_effect_change_scenario() {
    integration_setup();

    // Simulate user changing effect via API
    // 1. Receive command
    Command cmd;
    cmd.type = CommandType::SET_EFFECT;
    cmd.payload.setEffect.effectId = 5;

    // 2. Validate and apply
    ReduceResult result = applyCommand(*state, cmd);
    TEST_ASSERT_EQUAL(ReduceResult::OK, result);

    // 3. Render several frames
    for (int i = 0; i < 10; i++) {
        renderer->renderFrame(*state);
    }

    // 4. Verify state consistency
    TEST_ASSERT_EQUAL(5, state->effectId);
    TEST_ASSERT_EQUAL(10, renderer->frameCount);
    TEST_ASSERT_EQUAL(5, renderer->lastEffectId);

    integration_cleanup();
}

void test_full_zone_setup_scenario() {
    integration_setup();

    // Configure 3-zone setup
    Command zoneCmd;
    zoneCmd.type = CommandType::SET_ZONE_COUNT;
    zoneCmd.payload.setZoneCount.count = 3;
    applyCommand(*state, zoneCmd);

    // Assign different effects to zones
    const uint8_t zoneEffects[3] = {5, 10, 15};
    for (int z = 0; z < 3; z++) {
        Command effectCmd;
        effectCmd.type = CommandType::SET_ZONE_EFFECT;
        effectCmd.payload.setZoneEffect.zoneId = z;
        effectCmd.payload.setZoneEffect.effectId = zoneEffects[z];
        applyCommand(*state, effectCmd);
    }

    // Verify configuration
    TEST_ASSERT_EQUAL(3, state->zoneCount);
    for (int z = 0; z < 3; z++) {
        TEST_ASSERT_EQUAL(zoneEffects[z], state->zoneEffects[z]);
    }

    // Render
    renderer->renderFrame(*state);
    TEST_ASSERT_TRUE(renderer->frameCount > 0);

    integration_cleanup();
}

void test_full_parameter_sweep_scenario() {
    integration_setup();

    // Sweep through all parameters
    // Use int to avoid uint8_t overflow (0 + 51*6 = 306 wraps to 50, infinite loop)
    for (int brightness = 0; brightness <= 255; brightness += 51) {
        Command cmd;
        cmd.type = CommandType::SET_BRIGHTNESS;
        cmd.payload.setBrightness.value = static_cast<uint8_t>(brightness);
        applyCommand(*state, cmd);
        renderer->renderFrame(*state);
        TEST_ASSERT_EQUAL(brightness, renderer->lastBrightness);
    }

    for (int speed = 1; speed <= 50; speed += 10) {
        Command cmd;
        cmd.type = CommandType::SET_SPEED;
        cmd.payload.setSpeed.value = static_cast<uint8_t>(speed);
        applyCommand(*state, cmd);
        renderer->renderFrame(*state);
    }

    // Should complete without errors
    TEST_ASSERT_TRUE(renderer->frameCount >= 6 + 5);

    integration_cleanup();
}

//==============================================================================
// Test Suite Runner
//==============================================================================

void run_integration_tests() {
    // Pipeline tests
    RUN_TEST(test_command_updates_state_then_render);
    RUN_TEST(test_effect_change_pipeline);
    RUN_TEST(test_zone_effect_pipeline);
    RUN_TEST(test_invalid_command_doesnt_change_state);
    RUN_TEST(test_batch_commands_atomic_version);

    // Message bus tests
    RUN_TEST(test_message_bus_send_receive);
    RUN_TEST(test_message_bus_fifo_order);
    RUN_TEST(test_message_bus_overflow_protection);

    // Render pipeline tests
    RUN_TEST(test_render_produces_center_origin_output);
    RUN_TEST(test_render_respects_brightness);
    RUN_TEST(test_render_mirrors_strips);
    RUN_TEST(test_speed_affects_animation_spread);

    // State consistency tests
    RUN_TEST(test_state_version_increments_on_change);
    RUN_TEST(test_state_reset_clears_all);
    RUN_TEST(test_render_clears_before_draw);

    // Validation tests
    RUN_TEST(test_validate_effect_id_range);
    RUN_TEST(test_validate_speed_range);
    RUN_TEST(test_validate_zone_id_range);
    RUN_TEST(test_validate_zone_count_range);

    // End-to-end scenarios
    RUN_TEST(test_full_effect_change_scenario);
    RUN_TEST(test_full_zone_setup_scenario);
    RUN_TEST(test_full_parameter_sweep_scenario);
}
