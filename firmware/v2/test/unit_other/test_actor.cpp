/**
 * LightwaveOS v2 - Actor System Unit Tests
 *
 * Tests for the Actor model including:
 * - Message queue operations
 * - Message type classification
 * - Actor lifecycle (simplified - no actual FreeRTOS tasks in native)
 * - Message structure validation
 */

#include <unity.h>

#ifdef NATIVE_BUILD
#include "mocks/freertos_mock.h"
#endif

#include "../../src/core/actors/Actor.h"
#include <cstring>

using namespace lightwaveos::actors;

//==============================================================================
// Test Fixtures
//==============================================================================

void setUp() {
#ifdef NATIVE_BUILD
    freertos_mock::reset();
#endif
}

void tearDown() {
}

//==============================================================================
// Message Structure Tests
//==============================================================================

void test_message_size_is_16_bytes() {
    TEST_ASSERT_EQUAL_size_t(16, sizeof(Message));
}

void test_message_default_constructor() {
    Message msg;

    TEST_ASSERT_EQUAL_INT(MessageType::HEALTH_CHECK, msg.type);
    TEST_ASSERT_EQUAL_UINT8(0, msg.param1);
    TEST_ASSERT_EQUAL_UINT8(0, msg.param2);
    TEST_ASSERT_EQUAL_UINT8(0, msg.param3);
    TEST_ASSERT_EQUAL_UINT32(0, msg.param4);
}

void test_message_parameterized_constructor() {
    Message msg(MessageType::SET_EFFECT, 5, 10, 15, 1000);

    TEST_ASSERT_EQUAL_INT(MessageType::SET_EFFECT, msg.type);
    TEST_ASSERT_EQUAL_UINT8(5, msg.param1);
    TEST_ASSERT_EQUAL_UINT8(10, msg.param2);
    TEST_ASSERT_EQUAL_UINT8(15, msg.param3);
    TEST_ASSERT_EQUAL_UINT32(1000, msg.param4);
}

void test_message_is_command() {
    Message effectCmd(MessageType::SET_EFFECT, 5);
    Message zoneCmd(MessageType::ZONE_ENABLE, 0);
    Message systemCmd(MessageType::SHUTDOWN);

    TEST_ASSERT_TRUE(effectCmd.isCommand());
    TEST_ASSERT_TRUE(zoneCmd.isCommand());
    TEST_ASSERT_TRUE(systemCmd.isCommand());
}

void test_message_is_event() {
    Message effectChanged(MessageType::EFFECT_CHANGED, 3);
    Message frameRendered(MessageType::FRAME_RENDERED);
    Message stateUpdated(MessageType::STATE_UPDATED);

    TEST_ASSERT_TRUE(effectChanged.isEvent());
    TEST_ASSERT_TRUE(frameRendered.isEvent());
    TEST_ASSERT_TRUE(stateUpdated.isEvent());
}

void test_message_command_vs_event() {
    Message command(MessageType::SET_BRIGHTNESS, 128);
    Message event(MessageType::STATE_UPDATED);

    TEST_ASSERT_TRUE(command.isCommand());
    TEST_ASSERT_FALSE(command.isEvent());

    TEST_ASSERT_FALSE(event.isCommand());
    TEST_ASSERT_TRUE(event.isEvent());
}

//==============================================================================
// Message Type Tests
//==============================================================================

void test_message_type_ranges() {
    // Effect commands (0x00-0x1F)
    TEST_ASSERT_EQUAL_UINT8(0x00, static_cast<uint8_t>(MessageType::SET_EFFECT));
    TEST_ASSERT_LESS_THAN_UINT8(0x20, static_cast<uint8_t>(MessageType::SET_EFFECT));

    // Zone commands (0x20-0x3F)
    TEST_ASSERT_EQUAL_UINT8(0x20, static_cast<uint8_t>(MessageType::ZONE_ENABLE));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(0x20, static_cast<uint8_t>(MessageType::ZONE_SET_EFFECT));
    TEST_ASSERT_LESS_THAN_UINT8(0x40, static_cast<uint8_t>(MessageType::ZONE_SET_EFFECT));

    // Transition commands (0x40-0x5F)
    TEST_ASSERT_EQUAL_UINT8(0x40, static_cast<uint8_t>(MessageType::TRIGGER_TRANSITION));
    TEST_ASSERT_LESS_THAN_UINT8(0x60, static_cast<uint8_t>(MessageType::TRIGGER_TRANSITION));

    // System commands (0x60-0x7F)
    TEST_ASSERT_EQUAL_UINT8(0x60, static_cast<uint8_t>(MessageType::SHUTDOWN));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(0x60, static_cast<uint8_t>(MessageType::PING));
    TEST_ASSERT_LESS_THAN_UINT8(0x80, static_cast<uint8_t>(MessageType::PING));

    // Events (0x80+)
    TEST_ASSERT_EQUAL_UINT8(0x80, static_cast<uint8_t>(MessageType::EFFECT_CHANGED));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(0x80, static_cast<uint8_t>(MessageType::FRAME_RENDERED));
}

//==============================================================================
// ActorConfig Tests
//==============================================================================

void test_actor_config_default_constructor() {
    ActorConfig config;

    TEST_ASSERT_EQUAL_STRING("Actor", config.name);
    TEST_ASSERT_EQUAL_UINT16(2048, config.stackSize);
    TEST_ASSERT_EQUAL_UINT8(2, config.priority);
    TEST_ASSERT_EQUAL_INT(0, config.coreId);
    TEST_ASSERT_EQUAL_UINT8(16, config.queueSize);
    TEST_ASSERT_EQUAL_UINT32(0, config.tickInterval);
}

void test_actor_config_parameterized_constructor() {
    ActorConfig config("TestActor", 4096, 5, 1, 32, 100);

    TEST_ASSERT_EQUAL_STRING("TestActor", config.name);
    TEST_ASSERT_EQUAL_UINT16(4096, config.stackSize);
    TEST_ASSERT_EQUAL_UINT8(5, config.priority);
    TEST_ASSERT_EQUAL_INT(1, config.coreId);
    TEST_ASSERT_EQUAL_UINT8(32, config.queueSize);
    TEST_ASSERT_EQUAL_UINT32(100, config.tickInterval);
}

void test_actor_config_predefined_renderer() {
    ActorConfig config = ActorConfigs::Renderer();

    TEST_ASSERT_EQUAL_STRING("Renderer", config.name);
    TEST_ASSERT_EQUAL_UINT16(4096, config.stackSize);
    TEST_ASSERT_EQUAL_UINT8(5, config.priority);
    TEST_ASSERT_EQUAL_INT(1, config.coreId);  // Core 1
    TEST_ASSERT_EQUAL_UINT8(32, config.queueSize);
    TEST_ASSERT_GREATER_THAN_UINT32(0, config.tickInterval);  // Has tick interval
}

void test_actor_config_predefined_network() {
    ActorConfig config = ActorConfigs::Network();

    TEST_ASSERT_EQUAL_STRING("Network", config.name);
    TEST_ASSERT_EQUAL_UINT16(3072, config.stackSize);
    TEST_ASSERT_EQUAL_UINT8(3, config.priority);
    TEST_ASSERT_EQUAL_INT(0, config.coreId);  // Core 0
    TEST_ASSERT_EQUAL_UINT8(16, config.queueSize);
}

//==============================================================================
// Message Encoding Tests (Parameter Usage)
//==============================================================================

void test_set_effect_message_encoding() {
    // SET_EFFECT: param1=effectId, param4=transitionMs
    Message msg(MessageType::SET_EFFECT, 7, 0, 0, 500);

    TEST_ASSERT_EQUAL_UINT8(7, msg.param1);    // Effect ID
    TEST_ASSERT_EQUAL_UINT32(500, msg.param4);  // Transition duration
}

void test_set_brightness_message_encoding() {
    // SET_BRIGHTNESS: param1=brightness (0-255)
    Message msg(MessageType::SET_BRIGHTNESS, 128);

    TEST_ASSERT_EQUAL_UINT8(128, msg.param1);  // Brightness value
}

void test_zone_set_effect_message_encoding() {
    // ZONE_SET_EFFECT: param1=zoneId, param2=effectId
    Message msg(MessageType::ZONE_SET_EFFECT, 2, 5);

    TEST_ASSERT_EQUAL_UINT8(2, msg.param1);  // Zone ID
    TEST_ASSERT_EQUAL_UINT8(5, msg.param2);  // Effect ID
}

void test_trigger_transition_message_encoding() {
    // TRIGGER_TRANSITION: param1=transitionType, param4=durationMs
    Message msg(MessageType::TRIGGER_TRANSITION, 3, 0, 0, 1000);

    TEST_ASSERT_EQUAL_UINT8(3, msg.param1);     // Transition type
    TEST_ASSERT_EQUAL_UINT32(1000, msg.param4);  // Duration
}

//==============================================================================
// FreeRTOS Queue Mock Tests
//==============================================================================

void test_queue_create() {
    QueueHandle_t queue = xQueueCreate(16, sizeof(Message));

    TEST_ASSERT_NOT_NULL(queue);

    vQueueDelete(queue);
}

void test_queue_send_receive() {
    QueueHandle_t queue = xQueueCreate(16, sizeof(Message));
    TEST_ASSERT_NOT_NULL(queue);

    Message sendMsg(MessageType::SET_EFFECT, 7);
    BaseType_t sendResult = xQueueSend(queue, &sendMsg, 0);
    TEST_ASSERT_EQUAL_INT(pdPASS, sendResult);

    Message recvMsg;
    BaseType_t recvResult = xQueueReceive(queue, &recvMsg, 0);
    TEST_ASSERT_EQUAL_INT(pdPASS, recvResult);

    TEST_ASSERT_EQUAL_INT(MessageType::SET_EFFECT, recvMsg.type);
    TEST_ASSERT_EQUAL_UINT8(7, recvMsg.param1);

    vQueueDelete(queue);
}

void test_queue_messages_waiting() {
    QueueHandle_t queue = xQueueCreate(16, sizeof(Message));
    TEST_ASSERT_NOT_NULL(queue);

    TEST_ASSERT_EQUAL_UINT(0, uxQueueMessagesWaiting(queue));

    Message msg1(MessageType::SET_BRIGHTNESS, 100);
    xQueueSend(queue, &msg1, 0);
    TEST_ASSERT_EQUAL_UINT(1, uxQueueMessagesWaiting(queue));

    Message msg2(MessageType::SET_SPEED, 20);
    xQueueSend(queue, &msg2, 0);
    TEST_ASSERT_EQUAL_UINT(2, uxQueueMessagesWaiting(queue));

    Message recvMsg;
    xQueueReceive(queue, &recvMsg, 0);
    TEST_ASSERT_EQUAL_UINT(1, uxQueueMessagesWaiting(queue));

    vQueueDelete(queue);
}

void test_queue_fifo_order() {
    QueueHandle_t queue = xQueueCreate(16, sizeof(Message));

    Message msg1(MessageType::SET_EFFECT, 1);
    Message msg2(MessageType::SET_EFFECT, 2);
    Message msg3(MessageType::SET_EFFECT, 3);

    xQueueSend(queue, &msg1, 0);
    xQueueSend(queue, &msg2, 0);
    xQueueSend(queue, &msg3, 0);

    Message recv1, recv2, recv3;
    xQueueReceive(queue, &recv1, 0);
    xQueueReceive(queue, &recv2, 0);
    xQueueReceive(queue, &recv3, 0);

    TEST_ASSERT_EQUAL_UINT8(1, recv1.param1);
    TEST_ASSERT_EQUAL_UINT8(2, recv2.param1);
    TEST_ASSERT_EQUAL_UINT8(3, recv3.param1);

    vQueueDelete(queue);
}

//==============================================================================
// Message Categorization Tests
//==============================================================================

void test_all_effect_commands_are_commands() {
    Message setBrightness(MessageType::SET_BRIGHTNESS, 100);
    Message setSpeed(MessageType::SET_SPEED, 20);
    Message setPalette(MessageType::SET_PALETTE, 3);
    Message setIntensity(MessageType::SET_INTENSITY, 200);

    TEST_ASSERT_TRUE(setBrightness.isCommand());
    TEST_ASSERT_TRUE(setSpeed.isCommand());
    TEST_ASSERT_TRUE(setPalette.isCommand());
    TEST_ASSERT_TRUE(setIntensity.isCommand());
}

void test_all_zone_commands_are_commands() {
    Message zoneEnable(MessageType::ZONE_ENABLE, 0);
    Message zoneDisable(MessageType::ZONE_DISABLE, 1);
    Message zoneSetEffect(MessageType::ZONE_SET_EFFECT, 2, 5);
    Message zoneSetBrightness(MessageType::ZONE_SET_BRIGHTNESS, 3, 128);

    TEST_ASSERT_TRUE(zoneEnable.isCommand());
    TEST_ASSERT_TRUE(zoneDisable.isCommand());
    TEST_ASSERT_TRUE(zoneSetEffect.isCommand());
    TEST_ASSERT_TRUE(zoneSetBrightness.isCommand());
}

void test_all_events_are_events() {
    Message effectChanged(MessageType::EFFECT_CHANGED, 5);
    Message frameRendered(MessageType::FRAME_RENDERED);
    Message stateUpdated(MessageType::STATE_UPDATED);
    Message paletteChanged(MessageType::PALETTE_CHANGED, 3);
    Message transitionComplete(MessageType::TRANSITION_COMPLETE);

    TEST_ASSERT_TRUE(effectChanged.isEvent());
    TEST_ASSERT_TRUE(frameRendered.isEvent());
    TEST_ASSERT_TRUE(stateUpdated.isEvent());
    TEST_ASSERT_TRUE(paletteChanged.isEvent());
    TEST_ASSERT_TRUE(transitionComplete.isEvent());
}

//==============================================================================
// Semaphore Mock Tests (for StateStore and MessageBus)
//==============================================================================

void test_semaphore_create() {
    SemaphoreHandle_t sem = xSemaphoreCreateMutex();
    TEST_ASSERT_NOT_NULL(sem);
    vSemaphoreDelete(sem);
}

void test_semaphore_take_give() {
    SemaphoreHandle_t sem = xSemaphoreCreateMutex();
    TEST_ASSERT_NOT_NULL(sem);

    BaseType_t takeResult = xSemaphoreTake(sem, 0);
    TEST_ASSERT_EQUAL_INT(pdPASS, takeResult);

    BaseType_t giveResult = xSemaphoreGive(sem);
    TEST_ASSERT_EQUAL_INT(pdPASS, giveResult);

    vSemaphoreDelete(sem);
}

//==============================================================================
// Time Mock Tests
//==============================================================================

void test_millis_tracking() {
#ifdef NATIVE_BUILD
    freertos_mock::setMillis(0);
    TEST_ASSERT_EQUAL_UINT32(0, millis());

    freertos_mock::advanceTime(1000);
    TEST_ASSERT_EQUAL_UINT32(1000, millis());

    freertos_mock::advanceTime(500);
    TEST_ASSERT_EQUAL_UINT32(1500, millis());
#endif
}

void test_delay_advances_time() {
#ifdef NATIVE_BUILD
    freertos_mock::setMillis(0);

    delay(100);
    TEST_ASSERT_EQUAL_UINT32(100, millis());

    delay(50);
    TEST_ASSERT_EQUAL_UINT32(150, millis());
#endif
}

//==============================================================================
// Test Suite Runner
//==============================================================================

void run_actor_tests() {
    // Message structure tests
    RUN_TEST(test_message_size_is_16_bytes);
    RUN_TEST(test_message_default_constructor);
    RUN_TEST(test_message_parameterized_constructor);
    RUN_TEST(test_message_is_command);
    RUN_TEST(test_message_is_event);
    RUN_TEST(test_message_command_vs_event);

    // Message type tests
    RUN_TEST(test_message_type_ranges);

    // ActorConfig tests
    RUN_TEST(test_actor_config_default_constructor);
    RUN_TEST(test_actor_config_parameterized_constructor);
    RUN_TEST(test_actor_config_predefined_renderer);
    RUN_TEST(test_actor_config_predefined_network);

    // Message encoding tests
    RUN_TEST(test_set_effect_message_encoding);
    RUN_TEST(test_set_brightness_message_encoding);
    RUN_TEST(test_zone_set_effect_message_encoding);
    RUN_TEST(test_trigger_transition_message_encoding);

    // FreeRTOS queue mock tests
    RUN_TEST(test_queue_create);
    RUN_TEST(test_queue_send_receive);
    RUN_TEST(test_queue_messages_waiting);
    RUN_TEST(test_queue_fifo_order);

    // Message categorization tests
    RUN_TEST(test_all_effect_commands_are_commands);
    RUN_TEST(test_all_zone_commands_are_commands);
    RUN_TEST(test_all_events_are_events);

    // Semaphore mock tests
    RUN_TEST(test_semaphore_create);
    RUN_TEST(test_semaphore_take_give);

    // Time mock tests
    RUN_TEST(test_millis_tracking);
    RUN_TEST(test_delay_advances_time);
}
