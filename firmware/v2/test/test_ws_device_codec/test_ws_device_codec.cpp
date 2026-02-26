/**
 * @file test_ws_device_codec.cpp
 * @brief Unit tests for WsDeviceCodec decode contracts
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <ArduinoJson.h>
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

// Decode tests

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

void test_device_decode_request_id_wrong_type_defaults_empty() {
    const char* json = R"({"requestId": 123})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    DeviceDecodeResult result = WsDeviceCodec::decode(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("", result.request.requestId);
}

void setUp(void) {}
void tearDown(void) {}

int main() {
    UNITY_BEGIN();

    // Decode
    RUN_TEST(test_device_decode_with_request_id);
    RUN_TEST(test_device_decode_without_request_id);
    RUN_TEST(test_device_decode_request_id_wrong_type_defaults_empty);

    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD
