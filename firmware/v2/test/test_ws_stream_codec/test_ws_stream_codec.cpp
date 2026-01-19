/**
 * @file test_ws_stream_codec.cpp
 * @brief Unit tests for WsStreamCodec JSON parsing and validation
 *
 * Tests stream WebSocket command decoding with type checking, unknown-key rejection,
 * and encoder allow-list validation.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <ArduinoJson.h>
#include <fstream>
#include <string>
#include <cstring>
#include "../../src/codec/WsStreamCodec.h"

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

/**
 * @brief Validate keys against allow-list (matches decode test pattern)
 * 
 * Verifies:
 * - All keys in object are in the allow-list (no unknown keys)
 * - All required keys from allow-list are present
 * 
 * @param obj JSON object to validate
 * @param allowedKeys Array of allowed key names
 * @param allowedCount Number of allowed keys
 * @return true if all keys are allowed and all required keys are present
 */
static bool validateKeysAgainstAllowList(const JsonObject& obj, const char* allowedKeys[], size_t allowedCount) {
    size_t foundCount = 0;
    // Check that all keys in object are in allow-list
    for (JsonPair kv : obj) {
        const char* key = kv.key().c_str();
        bool isAllowed = false;
        for (size_t i = 0; i < allowedCount; i++) {
            if (strcmp(key, allowedKeys[i]) == 0) {
                isAllowed = true;
                foundCount++;
                break;
            }
        }
        if (!isAllowed) {
            return false;  // Unknown key found
        }
    }
    // Verify all required keys are present
    for (size_t i = 0; i < allowedCount; i++) {
        if (!obj.containsKey(allowedKeys[i])) {
            return false;  // Required key missing
        }
    }
    return foundCount == allowedCount;  // No extra, no missing
}

// ============================================================================
// Test: Valid Simple Request (requestId only)
// ============================================================================

void test_stream_simple_valid() {
    const char* json = R"({"requestId": "test123"})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    StreamSimpleDecodeResult result = WsStreamCodec::decodeSimple(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("test123", result.request.requestId);
}

void test_stream_simple_valid_no_requestId() {
    const char* json = R"({})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    StreamSimpleDecodeResult result = WsStreamCodec::decodeSimple(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("", result.request.requestId);
}

// ============================================================================
// Test: Encoder Functions (Response Encoding)
// ============================================================================

void test_encode_led_stream_subscribed() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsStreamCodec::encodeLedStreamSubscribed(12345, 966, 1, 2, 160, 20, 0xFE, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(12345, data["clientId"].as<uint32_t>(), "clientId should be 12345");
    TEST_ASSERT_EQUAL_INT_MESSAGE(966, data["frameSize"].as<uint16_t>(), "frameSize should be 966");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, data["frameVersion"].as<uint8_t>(), "frameVersion should be 1");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, data["numStrips"].as<uint8_t>(), "numStrips should be 2");
    TEST_ASSERT_EQUAL_INT_MESSAGE(160, data["ledsPerStrip"].as<uint16_t>(), "ledsPerStrip should be 160");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, data["targetFps"].as<uint8_t>(), "targetFps should be 20");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0xFE, data["magicByte"].as<uint8_t>(), "magicByte should be 0xFE");
    TEST_ASSERT_TRUE_MESSAGE(data["accepted"].as<bool>(), "accepted should be true");
    
    const char* allowedKeys[] = {"clientId", "frameSize", "frameVersion", "numStrips", "ledsPerStrip", "targetFps", "magicByte", "accepted"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 8),
        "Should only have required keys, no extras allowed");
}

void test_encode_led_stream_unsubscribed() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsStreamCodec::encodeLedStreamUnsubscribed(12345, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(12345, data["clientId"].as<uint32_t>(), "clientId should be 12345");
    
    const char* allowedKeys[] = {"clientId"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 1),
        "Should only have clientId key, no extras allowed");
}

void test_encode_validation_subscribed() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsStreamCodec::encodeValidationSubscribed(12345, 128, 16, 10, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(12345, data["clientId"].as<uint32_t>(), "clientId should be 12345");
    TEST_ASSERT_EQUAL_INT_MESSAGE(128, data["sampleSize"].as<uint32_t>(), "sampleSize should be 128");
    TEST_ASSERT_EQUAL_INT_MESSAGE(16, data["maxSamplesPerFrame"].as<uint32_t>(), "maxSamplesPerFrame should be 16");
    TEST_ASSERT_EQUAL_INT_MESSAGE(10, data["targetFps"].as<uint8_t>(), "targetFps should be 10");
    TEST_ASSERT_TRUE_MESSAGE(data["accepted"].as<bool>(), "accepted should be true");
    
    const char* allowedKeys[] = {"clientId", "sampleSize", "maxSamplesPerFrame", "targetFps", "accepted"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 5),
        "Should only have required keys, no extras allowed");
}

void test_encode_validation_unsubscribed() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsStreamCodec::encodeValidationUnsubscribed(12345, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(12345, data["clientId"].as<uint32_t>(), "clientId should be 12345");
    
    const char* allowedKeys[] = {"clientId"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 1),
        "Should only have clientId key, no extras allowed");
}

void test_encode_benchmark_subscribed() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsStreamCodec::encodeBenchmarkSubscribed(12345, 32, 10, 0x41, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(12345, data["clientId"].as<uint32_t>(), "clientId should be 12345");
    TEST_ASSERT_EQUAL_INT_MESSAGE(32, data["frameSize"].as<uint32_t>(), "frameSize should be 32");
    TEST_ASSERT_EQUAL_INT_MESSAGE(10, data["targetFps"].as<uint8_t>(), "targetFps should be 10");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0x41, data["magicByte"].as<uint8_t>(), "magicByte should be 0x41");
    TEST_ASSERT_TRUE_MESSAGE(data["accepted"].as<bool>(), "accepted should be true");
    
    const char* allowedKeys[] = {"clientId", "frameSize", "targetFps", "magicByte", "accepted"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 5),
        "Should only have required keys, no extras allowed");
}

void test_encode_benchmark_unsubscribed() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsStreamCodec::encodeBenchmarkUnsubscribed(12345, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(12345, data["clientId"].as<uint32_t>(), "clientId should be 12345");
    
    const char* allowedKeys[] = {"clientId"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 1),
        "Should only have clientId key, no extras allowed");
}

void test_encode_benchmark_started() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsStreamCodec::encodeBenchmarkStarted(data);

    TEST_ASSERT_TRUE_MESSAGE(data["active"].as<bool>(), "active should be true");
    
    const char* allowedKeys[] = {"active"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 1),
        "Should only have active key, no extras allowed");
}

void test_encode_benchmark_stopped() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsStreamCodec::encodeBenchmarkStopped(100.5f, 50.2f, 75.0f, 1000U, 200U, data);

    TEST_ASSERT_FALSE_MESSAGE(data["active"].as<bool>(), "active should be false");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("results"), "results object should be present");
    
    JsonObject results = data["results"].as<JsonObject>();
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.1f, 100.5f, results["avgTotalUs"].as<float>(), "avgTotalUs should be 100.5");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.1f, 50.2f, results["avgGoertzelUs"].as<float>(), "avgGoertzelUs should be 50.2");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.1f, 75.0f, results["cpuLoadPercent"].as<float>(), "cpuLoadPercent should be 75.0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1000, results["hopCount"].as<uint32_t>(), "hopCount should be 1000");
    TEST_ASSERT_EQUAL_INT_MESSAGE(200, results["peakTotalUs"].as<uint16_t>(), "peakTotalUs should be 200");
    
    const char* allowedKeys[] = {"active", "results"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 2),
        "Should only have active and results keys, no extras allowed");
    
    const char* resultsKeys[] = {"avgTotalUs", "avgGoertzelUs", "cpuLoadPercent", "hopCount", "peakTotalUs"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(results, resultsKeys, 5),
        "Results object should only have required keys, no extras allowed");
}

void test_encode_benchmark_stats() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsStreamCodec::encodeBenchmarkStats(true, 100.5f, 50.2f, 10.1f, 5.5f, 200U, 75.0f, 1000U, data);

    TEST_ASSERT_TRUE_MESSAGE(data["streaming"].as<bool>(), "streaming should be true");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("timing"), "timing object should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("load"), "load object should be present");
    
    JsonObject timing = data["timing"].as<JsonObject>();
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.1f, 100.5f, timing["avgTotalUs"].as<float>(), "avgTotalUs should be 100.5");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.1f, 50.2f, timing["avgGoertzelUs"].as<float>(), "avgGoertzelUs should be 50.2");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.1f, 10.1f, timing["avgDcAgcUs"].as<float>(), "avgDcAgcUs should be 10.1");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.1f, 5.5f, timing["avgChromaUs"].as<float>(), "avgChromaUs should be 5.5");
    TEST_ASSERT_EQUAL_INT_MESSAGE(200, timing["peakTotalUs"].as<uint16_t>(), "peakTotalUs should be 200");
    
    JsonObject load = data["load"].as<JsonObject>();
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.1f, 75.0f, load["cpuPercent"].as<float>(), "cpuPercent should be 75.0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1000, load["hopCount"].as<uint32_t>(), "hopCount should be 1000");
    
    const char* allowedKeys[] = {"streaming", "timing", "load"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 3),
        "Should only have streaming, timing, and load keys, no extras allowed");
    
    const char* timingKeys[] = {"avgTotalUs", "avgGoertzelUs", "avgDcAgcUs", "avgChromaUs", "peakTotalUs"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(timing, timingKeys, 5),
        "Timing object should only have required keys, no extras allowed");
    
    const char* loadKeys[] = {"cpuPercent", "hopCount"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(load, loadKeys, 2),
        "Load object should only have required keys, no extras allowed");
}

// ============================================================================
// Unity setUp/tearDown (required by Unity framework)
// ============================================================================

void setUp(void) {
    // Called before each test
}

void tearDown(void) {
    // Called after each test
}

// ============================================================================
// Test Runner
// ============================================================================

int main() {
    UNITY_BEGIN();

    // Valid payloads
    RUN_TEST(test_stream_simple_valid);
    RUN_TEST(test_stream_simple_valid_no_requestId);

    // Encoder tests (response payloads)
    RUN_TEST(test_encode_led_stream_subscribed);
    RUN_TEST(test_encode_led_stream_unsubscribed);
    RUN_TEST(test_encode_validation_subscribed);
    RUN_TEST(test_encode_validation_unsubscribed);
    RUN_TEST(test_encode_benchmark_subscribed);
    RUN_TEST(test_encode_benchmark_unsubscribed);
    RUN_TEST(test_encode_benchmark_started);
    RUN_TEST(test_encode_benchmark_stopped);
    RUN_TEST(test_encode_benchmark_stats);

    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD
