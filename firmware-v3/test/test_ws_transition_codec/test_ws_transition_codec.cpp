/**
 * @file test_ws_transition_codec.cpp
 * @brief Unit tests for WsTransitionCodec JSON parsing and validation
 *
 * Tests transition WebSocket command decoding with type checking, unknown-key rejection,
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
#include "../../src/codec/WsTransitionCodec.h"

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
// Test: Valid Transition Trigger
// ============================================================================

void test_transition_trigger_valid() {
    const char* json = R"({"toEffect": 5, "transitionType": 2, "random": false})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    TransitionTriggerDecodeResult result = WsTransitionCodec::decodeTrigger(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, result.request.toEffect, "toEffect should be 5");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, result.request.transitionType, "transitionType should be 2");
    TEST_ASSERT_FALSE_MESSAGE(result.request.random, "random should be false");
}

void test_transition_trigger_valid_defaults() {
    const char* json = R"({"toEffect": 10})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    TransitionTriggerDecodeResult result = WsTransitionCodec::decodeTrigger(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(10, result.request.toEffect, "toEffect should be 10");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, result.request.transitionType, "transitionType should default to 0");
    TEST_ASSERT_FALSE_MESSAGE(result.request.random, "random should default to false");
}

void test_transition_trigger_missing_toEffect() {
    const char* json = R"({"transitionType": 1})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    TransitionTriggerDecodeResult result = WsTransitionCodec::decodeTrigger(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result.errorMsg, "toEffect") != nullptr, "Error should mention toEffect");
}

void test_transition_trigger_out_of_range() {
    const char* json = R"({"toEffect": 200})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    TransitionTriggerDecodeResult result = WsTransitionCodec::decodeTrigger(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result.errorMsg, "range") != nullptr, "Error should mention range");
}

// ============================================================================
// Test: Valid Simple Request (requestId only)
// ============================================================================

void test_transition_simple_valid() {
    const char* json = R"({"requestId": "test123"})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    TransitionSimpleDecodeResult result = WsTransitionCodec::decodeSimple(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("test123", result.request.requestId);
}

void test_transition_simple_valid_no_requestId() {
    const char* json = R"({})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    TransitionSimpleDecodeResult result = WsTransitionCodec::decodeSimple(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("", result.request.requestId);
}

// ============================================================================
// Test: Valid Config Set
// ============================================================================

void test_transition_config_set_valid() {
    const char* json = R"({"requestId": "test", "defaultDuration": 2000, "defaultType": 3})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    TransitionConfigSetDecodeResult result = WsTransitionCodec::decodeConfigSet(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("test", result.request.requestId);
    TEST_ASSERT_EQUAL_INT_MESSAGE(2000, result.request.defaultDuration, "defaultDuration should be 2000");
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, result.request.defaultType, "defaultType should be 3");
}

void test_transition_config_set_defaults() {
    const char* json = R"({})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    TransitionConfigSetDecodeResult result = WsTransitionCodec::decodeConfigSet(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1000, result.request.defaultDuration, "defaultDuration should default to 1000");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, result.request.defaultType, "defaultType should default to 0");
}

// ============================================================================
// Test: Encoder Functions (Response Encoding)
// ============================================================================

void test_encode_get_types() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsTransitionCodec::encodeGetTypes(data);

    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("types"), "types array should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("total"), "total should be present");
    TEST_ASSERT_EQUAL_INT_MESSAGE(12, data["total"].as<int>(), "total should be 12 (TYPE_COUNT)");
    
    JsonArray types = data["types"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(12, types.size(), "types array should have 12 entries");
    
    const char* allowedKeys[] = {"types", "total"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 2),
        "Should only have types and total keys, no extras allowed");
}

void test_encode_config_get() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsTransitionCodec::encodeConfigGet(data);

    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("enabled"), "enabled should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("defaultDuration"), "defaultDuration should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("defaultType"), "defaultType should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("defaultTypeName"), "defaultTypeName should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("easings"), "easings array should be present");
    
    TEST_ASSERT_TRUE_MESSAGE(data["enabled"].as<bool>(), "enabled should be true");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1000, data["defaultDuration"].as<int>(), "defaultDuration should be 1000");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, data["defaultType"].as<int>(), "defaultType should be 0");
    
    JsonArray easings = data["easings"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(10, easings.size(), "easings array should have 10 entries");
    
    const char* allowedKeys[] = {"enabled", "defaultDuration", "defaultType", "defaultTypeName", "easings"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 5),
        "Should only have required keys, no extras allowed");
}

void test_encode_config_set() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsTransitionCodec::encodeConfigSet(2000, 3, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(2000, data["defaultDuration"].as<int>(), "defaultDuration should be 2000");
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, data["defaultType"].as<int>(), "defaultType should be 3");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("defaultTypeName"), "defaultTypeName should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("message"), "message should be present");
    TEST_ASSERT_EQUAL_STRING("Transition config updated", data["message"].as<const char*>());
    
    const char* allowedKeys[] = {"defaultDuration", "defaultType", "defaultTypeName", "message"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 4),
        "Should only have required keys, no extras allowed");
}

void test_encode_list() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsTransitionCodec::encodeList(data);

    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("types"), "types array should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("easingCurves"), "easingCurves array should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("total"), "total should be present");
    
    JsonArray types = data["types"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(12, types.size(), "types array should have 12 entries");
    
    JsonArray easings = data["easingCurves"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(10, easings.size(), "easingCurves array should have 10 entries");
    
    const char* allowedKeys[] = {"types", "easingCurves", "total"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 3),
        "Should only have types, easingCurves, and total keys, no extras allowed");
}

void test_encode_trigger_started() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsTransitionCodec::encodeTriggerStarted(5, 10, "TestEffect", 2, "Wipe Out", 1500, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(5, data["fromEffect"].as<int>(), "fromEffect should be 5");
    TEST_ASSERT_EQUAL_INT_MESSAGE(10, data["toEffect"].as<int>(), "toEffect should be 10");
    TEST_ASSERT_EQUAL_STRING("TestEffect", data["toEffectName"].as<const char*>());
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, data["transitionType"].as<int>(), "transitionType should be 2");
    TEST_ASSERT_EQUAL_STRING("Wipe Out", data["transitionName"].as<const char*>());
    TEST_ASSERT_EQUAL_INT_MESSAGE(1500, data["duration"].as<int>(), "duration should be 1500");
    
    const char* allowedKeys[] = {"fromEffect", "toEffect", "toEffectName", "transitionType", "transitionName", "duration"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 6),
        "Should only have required keys, no extras allowed");
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
    RUN_TEST(test_transition_trigger_valid);
    RUN_TEST(test_transition_trigger_valid_defaults);
    RUN_TEST(test_transition_simple_valid);
    RUN_TEST(test_transition_simple_valid_no_requestId);
    RUN_TEST(test_transition_config_set_valid);
    RUN_TEST(test_transition_config_set_defaults);

    // Missing required fields
    RUN_TEST(test_transition_trigger_missing_toEffect);

    // Range validation
    RUN_TEST(test_transition_trigger_out_of_range);

    // Encoder tests (response payloads)
    RUN_TEST(test_encode_get_types);
    RUN_TEST(test_encode_config_get);
    RUN_TEST(test_encode_config_set);
    RUN_TEST(test_encode_list);
    RUN_TEST(test_encode_trigger_started);

    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD
