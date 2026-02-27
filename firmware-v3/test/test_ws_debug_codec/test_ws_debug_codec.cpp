/**
 * @file test_ws_debug_codec.cpp
 * @brief Unit tests for WsDebugCodec JSON parsing and validation
 *
 * Tests debug WebSocket command decoding with type checking, unknown-key rejection,
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
#include "../../src/codec/WsDebugCodec.h"

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

void test_debug_simple_valid() {
    const char* json = R"({"requestId": "test123"})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    DebugSimpleDecodeResult result = WsDebugCodec::decodeSimple(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("test123", result.request.requestId);
}

void test_debug_simple_valid_no_requestId() {
    const char* json = R"({})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    DebugSimpleDecodeResult result = WsDebugCodec::decodeSimple(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("", result.request.requestId);
}

// ============================================================================
// Test: Decode Functions
// ============================================================================

void test_decode_debug_audio_set_valid_verbosity_only() {
    const char* json = R"({"verbosity": 3})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    DebugAudioSetDecodeResult result = WsDebugCodec::decodeDebugAudioSet(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasVerbosity, "hasVerbosity should be true");
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, result.request.verbosity, "verbosity should be 3");
    TEST_ASSERT_FALSE_MESSAGE(result.request.hasBaseInterval, "hasBaseInterval should be false");
}

void test_decode_debug_audio_set_valid_baseInterval_only() {
    const char* json = R"({"baseInterval": 500})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    DebugAudioSetDecodeResult result = WsDebugCodec::decodeDebugAudioSet(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_FALSE_MESSAGE(result.request.hasVerbosity, "hasVerbosity should be false");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasBaseInterval, "hasBaseInterval should be true");
    TEST_ASSERT_EQUAL_INT_MESSAGE(500, result.request.baseInterval, "baseInterval should be 500");
}

void test_decode_debug_audio_set_valid_both() {
    const char* json = R"({"verbosity": 2, "baseInterval": 250, "requestId": "test"})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    DebugAudioSetDecodeResult result = WsDebugCodec::decodeDebugAudioSet(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasVerbosity, "hasVerbosity should be true");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, result.request.verbosity, "verbosity should be 2");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasBaseInterval, "hasBaseInterval should be true");
    TEST_ASSERT_EQUAL_INT_MESSAGE(250, result.request.baseInterval, "baseInterval should be 250");
    TEST_ASSERT_EQUAL_STRING("test", result.request.requestId);
}

void test_decode_debug_audio_set_invalid_missing_both() {
    const char* json = R"({})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    DebugAudioSetDecodeResult result = WsDebugCodec::decodeDebugAudioSet(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result.errorMsg, "At least one") != nullptr, "Error should mention at least one field required");
}

void test_decode_debug_audio_set_invalid_verbosity_range() {
    const char* json = R"({"verbosity": 6})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    DebugAudioSetDecodeResult result = WsDebugCodec::decodeDebugAudioSet(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result.errorMsg, "out of range") != nullptr || strstr(result.errorMsg, "must be 0-5") != nullptr, "Error should mention range");
}

void test_decode_debug_audio_set_invalid_baseInterval_range_low() {
    const char* json = R"({"baseInterval": 0})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    DebugAudioSetDecodeResult result = WsDebugCodec::decodeDebugAudioSet(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result.errorMsg, "out of range") != nullptr || strstr(result.errorMsg, "must be 1-1000") != nullptr, "Error should mention range");
}

void test_decode_debug_audio_set_invalid_baseInterval_range_high() {
    const char* json = R"({"baseInterval": 1001})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    DebugAudioSetDecodeResult result = WsDebugCodec::decodeDebugAudioSet(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result.errorMsg, "out of range") != nullptr || strstr(result.errorMsg, "must be 1-1000") != nullptr, "Error should mention range");
}

// ============================================================================
// Test: Encoder Functions (Response Encoding)
// ============================================================================

void test_encode_debug_audio_state() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    static constexpr const char* levels[] = {
        "Off - No debug output",
        "Minimal - Errors only",
        "Status - 10s health reports",
        "Low - + DMA diagnostics (~5s)",
        "Medium - + 64-bin Goertzel (~2s)",
        "High - + 8-band Goertzel (~1s)"
    };

    WsDebugCodec::encodeDebugAudioState(3, 500, 100, 200, 300, levels, 6, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(3, data["verbosity"].as<uint8_t>(), "verbosity should be 3");
    TEST_ASSERT_EQUAL_INT_MESSAGE(500, data["baseInterval"].as<uint16_t>(), "baseInterval should be 500");
    
    // Check intervals object
    JsonObject intervals = data["intervals"].as<JsonObject>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, intervals["8band"].as<uint16_t>(), "8band interval should be 100");
    TEST_ASSERT_EQUAL_INT_MESSAGE(200, intervals["64bin"].as<uint16_t>(), "64bin interval should be 200");
    TEST_ASSERT_EQUAL_INT_MESSAGE(300, intervals["dma"].as<uint16_t>(), "dma interval should be 300");
    
    // Check levels array
    JsonArray levelsArray = data["levels"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(6, levelsArray.size(), "levels array should have 6 elements");
    TEST_ASSERT_EQUAL_STRING("Off - No debug output", levelsArray[0].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("High - + 8-band Goertzel (~1s)", levelsArray[5].as<const char*>());
    
    // Allow-list validation for top-level keys
    const char* allowedKeys[] = {"verbosity", "baseInterval", "intervals", "levels"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 4),
        "Should only have required keys, no extras allowed");
    
    // Allow-list validation for intervals keys
    const char* allowedIntervalKeys[] = {"8band", "64bin", "dma"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(intervals, allowedIntervalKeys, 3),
        "intervals should only have required keys, no extras allowed");
}

void test_encode_debug_audio_updated() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsDebugCodec::encodeDebugAudioUpdated(2, 250, 50, 100, 150, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(2, data["verbosity"].as<uint8_t>(), "verbosity should be 2");
    TEST_ASSERT_EQUAL_INT_MESSAGE(250, data["baseInterval"].as<uint16_t>(), "baseInterval should be 250");
    
    // Check intervals object
    JsonObject intervals = data["intervals"].as<JsonObject>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(50, intervals["8band"].as<uint16_t>(), "8band interval should be 50");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, intervals["64bin"].as<uint16_t>(), "64bin interval should be 100");
    TEST_ASSERT_EQUAL_INT_MESSAGE(150, intervals["dma"].as<uint16_t>(), "dma interval should be 150");
    
    // Should NOT have levels array
    TEST_ASSERT_FALSE_MESSAGE(data.containsKey("levels"), "updated response should not have levels array");
    
    // Allow-list validation for top-level keys
    const char* allowedKeys[] = {"verbosity", "baseInterval", "intervals"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 3),
        "Should only have required keys, no extras allowed");
    
    // Allow-list validation for intervals keys
    const char* allowedIntervalKeys[] = {"8band", "64bin", "dma"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(intervals, allowedIntervalKeys, 3),
        "intervals should only have required keys, no extras allowed");
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
    RUN_TEST(test_debug_simple_valid);
    RUN_TEST(test_debug_simple_valid_no_requestId);

    // Decode tests
    RUN_TEST(test_decode_debug_audio_set_valid_verbosity_only);
    RUN_TEST(test_decode_debug_audio_set_valid_baseInterval_only);
    RUN_TEST(test_decode_debug_audio_set_valid_both);
    RUN_TEST(test_decode_debug_audio_set_invalid_missing_both);
    RUN_TEST(test_decode_debug_audio_set_invalid_verbosity_range);
    RUN_TEST(test_decode_debug_audio_set_invalid_baseInterval_range_low);
    RUN_TEST(test_decode_debug_audio_set_invalid_baseInterval_range_high);

    // Encoder tests (response payloads)
    RUN_TEST(test_encode_debug_audio_state);
    RUN_TEST(test_encode_debug_audio_updated);

    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD
