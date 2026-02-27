// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file test_http_transition_codec.cpp
 * @brief Unit tests for HttpTransitionCodec JSON parsing and encoder allow-list validation
 *
 * Tests HTTP transition endpoint decoding (optional fields, defaults) and encoder
 * functions (response payload allow-lists).
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>
#include "../../src/codec/HttpTransitionCodec.h"

using namespace lightwaveos::codec;

// ============================================================================
// Helper Functions
// ============================================================================

static bool loadJsonString(const char* jsonStr, JsonDocument& doc) {
    DeserializationError error = deserializeJson(doc, jsonStr);
    return error == DeserializationError::Ok;
}

static bool validateKeysAgainstAllowList(const JsonObject& obj, const char* allowedKeys[], size_t allowedCount) {
    size_t foundCount = 0;
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
            return false;
        }
    }
    for (size_t i = 0; i < allowedCount; i++) {
        if (!obj.containsKey(allowedKeys[i])) {
            return false;
        }
    }
    return foundCount == allowedCount;
}

// ============================================================================
// Decode tests
// ============================================================================

void test_http_transition_trigger_decode_basic() {
    const char* json = R"({"toEffect": 5})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    HttpTransitionTriggerDecodeResult result = HttpTransitionCodec::decodeTrigger(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(5, result.request.toEffect, "toEffect should be 5");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, result.request.transitionType, "transitionType should default to 0");
    TEST_ASSERT_FALSE_MESSAGE(result.request.random, "random should default to false");
}

void test_http_transition_trigger_decode_with_type() {
    const char* json = R"({"toEffect": 10, "type": 2})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    HttpTransitionTriggerDecodeResult result = HttpTransitionCodec::decodeTrigger(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(10, result.request.toEffect, "toEffect should be 10");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(2, result.request.transitionType, "transitionType should be 2");
}

void test_http_transition_trigger_decode_with_random() {
    const char* json = R"({"toEffect": 5, "random": true})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    HttpTransitionTriggerDecodeResult result = HttpTransitionCodec::decodeTrigger(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_TRUE_MESSAGE(result.request.random, "random should be true");
}

void test_http_transition_trigger_decode_missing_toEffect() {
    const char* json = R"({"random": true})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    HttpTransitionTriggerDecodeResult result = HttpTransitionCodec::decodeTrigger(root);

    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(strlen(result.errorMsg) > 0, "Error message should be set");
}

void test_http_transition_config_set_decode_basic() {
    const char* json = R"({})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    HttpTransitionConfigSetDecodeResult result = HttpTransitionCodec::decodeConfigSet(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(1000, result.request.defaultDuration, "defaultDuration should default to 1000");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, result.request.defaultType, "defaultType should default to 0");
}

void test_http_transition_config_set_decode_with_values() {
    const char* json = R"({"defaultDuration": 2000, "defaultType": 3})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    HttpTransitionConfigSetDecodeResult result = HttpTransitionCodec::decodeConfigSet(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(2000, result.request.defaultDuration, "defaultDuration should be 2000");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(3, result.request.defaultType, "defaultType should be 3");
}

// ============================================================================
// Encode allow-list tests
// ============================================================================

void test_http_transition_encode_config_get_allow_list() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();
    
    HttpTransitionConfigGetData configData;
    configData.enabled = true;
    configData.defaultDuration = 1000;
    configData.defaultType = 0;
    
    HttpTransitionCodec::encodeConfigGet(configData, data);
    
    TEST_ASSERT_TRUE_MESSAGE(data["enabled"].as<bool>(), "enabled should be true");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(1000, data["defaultDuration"].as<uint16_t>(), "defaultDuration should be 1000");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, data["defaultType"].as<uint8_t>(), "defaultType should be 0");
    
    const char* keys[] = {"enabled", "defaultDuration", "defaultType"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, keys, 3), "config.get should only have enabled, defaultDuration, defaultType");
}

// ============================================================================
// Unity setUp/tearDown
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// Test Runner
// ============================================================================

int main() {
    UNITY_BEGIN();
    
    // Decode tests
    RUN_TEST(test_http_transition_trigger_decode_basic);
    RUN_TEST(test_http_transition_trigger_decode_with_type);
    RUN_TEST(test_http_transition_trigger_decode_with_random);
    RUN_TEST(test_http_transition_trigger_decode_missing_toEffect);
    RUN_TEST(test_http_transition_config_set_decode_basic);
    RUN_TEST(test_http_transition_config_set_decode_with_values);
    
    // Encode allow-list tests
    RUN_TEST(test_http_transition_encode_config_get_allow_list);
    
    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD
