// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file test_ws_narrative_codec.cpp
 * @brief Unit tests for WsNarrativeCodec JSON parsing and validation
 *
 * Tests narrative WebSocket command decoding with type checking, unknown-key rejection,
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
#include "../../src/codec/WsNarrativeCodec.h"

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

void test_narrative_simple_valid() {
    const char* json = R"({"requestId": "test123"})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    NarrativeSimpleDecodeResult result = WsNarrativeCodec::decodeSimple(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("test123", result.request.requestId);
}

void test_narrative_simple_valid_no_requestId() {
    const char* json = R"({})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    NarrativeSimpleDecodeResult result = WsNarrativeCodec::decodeSimple(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("", result.request.requestId);
}

// ============================================================================
// Test: Config Decode (GET vs SET detection)
// ============================================================================

void test_narrative_config_get() {
    const char* json = R"({"requestId": "test"})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    NarrativeConfigDecodeResult result = WsNarrativeCodec::decodeConfig(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_FALSE_MESSAGE(result.request.isSet, "Should detect GET operation");
    TEST_ASSERT_EQUAL_STRING("test", result.request.requestId);
}

void test_narrative_config_set_durations() {
    const char* json = R"({"durations": {"build": 2.0, "hold": 1.0, "release": 2.5, "rest": 0.8}})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    NarrativeConfigDecodeResult result = WsNarrativeCodec::decodeConfig(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_TRUE_MESSAGE(result.request.isSet, "Should detect SET operation");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasDurations, "Should have durations");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 2.0f, result.request.buildDuration, "buildDuration should be 2.0");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 1.0f, result.request.holdDuration, "holdDuration should be 1.0");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 2.5f, result.request.releaseDuration, "releaseDuration should be 2.5");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.8f, result.request.restDuration, "restDuration should be 0.8");
}

void test_narrative_config_set_curves() {
    const char* json = R"({"curves": {"build": 3, "release": 7}})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    NarrativeConfigDecodeResult result = WsNarrativeCodec::decodeConfig(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_TRUE_MESSAGE(result.request.isSet, "Should detect SET operation");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasCurves, "Should have curves");
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, result.request.buildCurveId, "buildCurveId should be 3");
    TEST_ASSERT_EQUAL_INT_MESSAGE(7, result.request.releaseCurveId, "releaseCurveId should be 7");
}

void test_narrative_config_set_holdBreathe() {
    const char* json = R"({"holdBreathe": 0.5})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    NarrativeConfigDecodeResult result = WsNarrativeCodec::decodeConfig(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_TRUE_MESSAGE(result.request.isSet, "Should detect SET operation");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasHoldBreathe, "Should have holdBreathe");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.5f, result.request.holdBreathe, "holdBreathe should be 0.5");
}

void test_narrative_config_set_enabled() {
    const char* json = R"({"enabled": true})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    NarrativeConfigDecodeResult result = WsNarrativeCodec::decodeConfig(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_TRUE_MESSAGE(result.request.isSet, "Should detect SET operation");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasEnabled, "Should have enabled");
    TEST_ASSERT_TRUE_MESSAGE(result.request.enabled, "enabled should be true");
}

void test_narrative_config_set_defaults() {
    const char* json = R"({"durations": {"build": 2.0}})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    NarrativeConfigDecodeResult result = WsNarrativeCodec::decodeConfig(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasDurations, "Should have durations");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 2.0f, result.request.buildDuration, "buildDuration should be 2.0");
    // Other durations should use defaults from operator|
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.5f, result.request.holdDuration, "holdDuration should default to 0.5");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 1.5f, result.request.releaseDuration, "releaseDuration should default to 1.5");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.5f, result.request.restDuration, "restDuration should default to 0.5");
}

void test_narrative_config_set_mixed() {
    const char* json = R"({"durations": {"build": 2.0}, "holdBreathe": 0.3, "enabled": false})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    NarrativeConfigDecodeResult result = WsNarrativeCodec::decodeConfig(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_TRUE_MESSAGE(result.request.isSet, "Should detect SET operation");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasDurations, "Should have durations");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasHoldBreathe, "Should have holdBreathe");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasEnabled, "Should have enabled");
    TEST_ASSERT_FALSE_MESSAGE(result.request.enabled, "enabled should be false");
}

// ============================================================================
// Test: Encoder Functions (Response Encoding)
// ============================================================================

void test_encode_status() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsNarrativeCodec::encodeStatus(
        true, 0.75f, 0.5f, 0.3f, "BUILD", 0,
        1.5f, 0.5f, 1.5f, 0.5f, 4.0f,
        1.25f, 0.875f,
        data
    );

    TEST_ASSERT_TRUE_MESSAGE(data["enabled"].as<bool>(), "enabled should be true");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.75f, data["tension"].as<float>(), "tension should be 0.75");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.5f, data["phaseT"].as<float>(), "phaseT should be 0.5");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.3f, data["cycleT"].as<float>(), "cycleT should be 0.3");
    TEST_ASSERT_EQUAL_STRING("BUILD", data["phase"].as<const char*>());
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, data["phaseId"].as<uint8_t>(), "phaseId should be 0");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("durations"), "durations object should be present");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 1.25f, data["tempoMultiplier"].as<float>(), "tempoMultiplier should be 1.25");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.875f, data["complexityScaling"].as<float>(), "complexityScaling should be 0.875");
    
    JsonObject durations = data["durations"].as<JsonObject>();
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 1.5f, durations["build"].as<float>(), "build duration should be 1.5");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.5f, durations["hold"].as<float>(), "hold duration should be 0.5");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 1.5f, durations["release"].as<float>(), "release duration should be 1.5");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.5f, durations["rest"].as<float>(), "rest duration should be 0.5");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 4.0f, durations["total"].as<float>(), "total duration should be 4.0");
    
    const char* allowedKeys[] = {"enabled", "tension", "phaseT", "cycleT", "phase", "phaseId", "durations", "tempoMultiplier", "complexityScaling"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 9),
        "Should only have required keys, no extras allowed");
    
    const char* durationsKeys[] = {"build", "hold", "release", "rest", "total"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(durations, durationsKeys, 5),
        "Durations object should only have required keys, no extras allowed");
}

void test_encode_config_get() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsNarrativeCodec::encodeConfigGet(
        1.5f, 0.5f, 1.5f, 0.5f, 4.0f,
        1, 6,
        0.2f, 0.1f, 0.05f,
        true,
        data
    );

    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("durations"), "durations object should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("curves"), "curves object should be present");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.2f, data["holdBreathe"].as<float>(), "holdBreathe should be 0.2");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.1f, data["snapAmount"].as<float>(), "snapAmount should be 0.1");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.05f, data["durationVariance"].as<float>(), "durationVariance should be 0.05");
    TEST_ASSERT_TRUE_MESSAGE(data["enabled"].as<bool>(), "enabled should be true");
    
    JsonObject durations = data["durations"].as<JsonObject>();
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 4.0f, durations["total"].as<float>(), "total duration should be 4.0");
    
    JsonObject curves = data["curves"].as<JsonObject>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, curves["build"].as<uint8_t>(), "build curve should be 1");
    TEST_ASSERT_EQUAL_INT_MESSAGE(6, curves["release"].as<uint8_t>(), "release curve should be 6");
    
    const char* allowedKeys[] = {"durations", "curves", "holdBreathe", "snapAmount", "durationVariance", "enabled"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 6),
        "Should only have required keys, no extras allowed");
    
    const char* durationsKeys[] = {"build", "hold", "release", "rest", "total"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(durations, durationsKeys, 5),
        "Durations object should only have required keys, no extras allowed");
    
    const char* curvesKeys[] = {"build", "release"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(curves, curvesKeys, 2),
        "Curves object should only have required keys, no extras allowed");
}

void test_encode_config_set_result() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsNarrativeCodec::encodeConfigSetResult(true, data);

    TEST_ASSERT_EQUAL_STRING("Narrative config updated", data["message"].as<const char*>());
    TEST_ASSERT_TRUE_MESSAGE(data["updated"].as<bool>(), "updated should be true");
    
    const char* allowedKeys[] = {"message", "updated"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 2),
        "Should only have message and updated keys, no extras allowed");
}

void test_encode_config_set_result_no_changes() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsNarrativeCodec::encodeConfigSetResult(false, data);

    TEST_ASSERT_EQUAL_STRING("No changes", data["message"].as<const char*>());
    TEST_ASSERT_FALSE_MESSAGE(data["updated"].as<bool>(), "updated should be false");
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
    RUN_TEST(test_narrative_simple_valid);
    RUN_TEST(test_narrative_simple_valid_no_requestId);
    RUN_TEST(test_narrative_config_get);
    RUN_TEST(test_narrative_config_set_durations);
    RUN_TEST(test_narrative_config_set_curves);
    RUN_TEST(test_narrative_config_set_holdBreathe);
    RUN_TEST(test_narrative_config_set_enabled);
    RUN_TEST(test_narrative_config_set_defaults);
    RUN_TEST(test_narrative_config_set_mixed);

    // Encoder tests (response payloads)
    RUN_TEST(test_encode_status);
    RUN_TEST(test_encode_config_get);
    RUN_TEST(test_encode_config_set_result);
    RUN_TEST(test_encode_config_set_result_no_changes);

    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD
