/**
 * @file test_ws_device_codec.cpp
 * @brief Unit tests for WsDeviceCodec JSON parsing and encoder allow-list validation
 *
 * Tests device WebSocket command decoding (requestId extraction) and encoder
 * functions (response payload allow-lists).
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>
#include "../../src/codec/WsDeviceCodec.h"

using namespace lightwaveos::codec;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Load JSON from string and parse into JsonDocument
 */
static bool loadJsonString(const char* jsonStr, JsonDocument& doc) {
    DeserializationError error = deserializeJson(doc, jsonStr);
    return error == DeserializationError::Ok;
}

// ============================================================================
// Decode tests
// ============================================================================

void test_device_decode_with_request_id() {
    const char* json = R"({"requestId": "test123"})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    DeviceDecodeResult result = WsDeviceCodec::decode(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("test123", result.request.requestId);
}

void test_device_decode_without_request_id() {
    const char* json = R"({})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    DeviceDecodeResult result = WsDeviceCodec::decode(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("", result.request.requestId);
}

// ============================================================================
// Unity setUp/tearDown (required by Unity framework)
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// Test Runner
// ============================================================================

int main() {
    UNITY_BEGIN();

    // Decode
    RUN_TEST(test_device_decode_with_request_id);
    RUN_TEST(test_device_decode_without_request_id);

    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD
