/**
 * @file test_http_effects_codec.cpp
 * @brief Unit tests for HttpEffectsCodec JSON parsing and encoder allow-list validation
 *
 * Tests HTTP effects endpoint decoding (optional fields, defaults) and encoder
 * functions (response payload allow-lists).
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>
#include "../../src/codec/HttpEffectsCodec.h"

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
 * @brief Validate keys against allow-list (no extras, no missing)
 */
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

void test_http_effects_set_decode_basic() {
    const char* json = R"({"effectId": 5})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    HttpEffectsSetDecodeResult result = HttpEffectsCodec::decodeSet(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(5, result.request.effectId, "effectId should be 5");
    TEST_ASSERT_FALSE_MESSAGE(result.request.useTransition, "useTransition should default to false");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, result.request.transitionType, "transitionType should default to 0");
}

void test_http_effects_set_decode_with_transition() {
    const char* json = R"({"effectId": 10, "transition": true, "transitionType": 2})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    HttpEffectsSetDecodeResult result = HttpEffectsCodec::decodeSet(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(10, result.request.effectId, "effectId should be 10");
    TEST_ASSERT_TRUE_MESSAGE(result.request.useTransition, "useTransition should be true");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(2, result.request.transitionType, "transitionType should be 2");
}

void test_http_effects_set_decode_missing_effectId() {
    const char* json = R"({"transition": true})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    HttpEffectsSetDecodeResult result = HttpEffectsCodec::decodeSet(root);

    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(strlen(result.errorMsg) > 0, "Error message should be set");
}

void test_http_effects_set_decode_invalid_effectId_range() {
    const char* json = R"({"effectId": 200})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    HttpEffectsSetDecodeResult result = HttpEffectsCodec::decodeSet(root);

    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(strlen(result.errorMsg) > 0, "Error message should be set");
}

void test_http_effects_parameters_set_decode_valid() {
    const char* json = R"({"effectId": 5, "parameters": {"speed": 20.0, "intensity": 0.8}})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    HttpEffectsParametersSetDecodeResult result = HttpEffectsCodec::decodeParametersSet(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(5, result.request.effectId, "effectId should be 5");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasParameters, "hasParameters should be true");
    TEST_ASSERT_TRUE_MESSAGE(result.request.parameters.containsKey("speed"), "parameters should contain speed");
    TEST_ASSERT_TRUE_MESSAGE(result.request.parameters.containsKey("intensity"), "parameters should contain intensity");
}

void test_http_effects_parameters_set_decode_missing_effectId() {
    const char* json = R"({"parameters": {"speed": 20.0}})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    HttpEffectsParametersSetDecodeResult result = HttpEffectsCodec::decodeParametersSet(root);

    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(strlen(result.errorMsg) > 0, "Error message should be set");
}

void test_http_effects_parameters_set_decode_missing_parameters() {
    const char* json = R"({"effectId": 5})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    HttpEffectsParametersSetDecodeResult result = HttpEffectsCodec::decodeParametersSet(root);

    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(strlen(result.errorMsg) > 0, "Error message should be set");
}

void test_http_effects_parameters_set_decode_parameters_not_object() {
    const char* json = R"({"effectId": 5, "parameters": "invalid"})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    HttpEffectsParametersSetDecodeResult result = HttpEffectsCodec::decodeParametersSet(root);

    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(strlen(result.errorMsg) > 0, "Error message should be set");
}

// ============================================================================
// Encode allow-list tests
// ============================================================================

void test_http_effects_encode_list_pagination_allow_list() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();
    
    HttpEffectsListPaginationData paginationData;
    paginationData.total = 100;
    paginationData.offset = 20;
    paginationData.limit = 10;
    
    HttpEffectsCodec::encodeListPagination(paginationData, data);
    
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, data["total"].as<int>(), "total should be 100");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, data["offset"].as<int>(), "offset should be 20");
    TEST_ASSERT_EQUAL_INT_MESSAGE(10, data["limit"].as<int>(), "limit should be 10");
    
    const char* keys[] = {"total", "offset", "limit"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, keys, 3), "pagination should only have total, offset, limit");
}

void test_http_effects_encode_list_pagination_zero_values() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();
    
    HttpEffectsListPaginationData paginationData;
    paginationData.total = 0;
    paginationData.offset = 0;
    paginationData.limit = 20;
    
    HttpEffectsCodec::encodeListPagination(paginationData, data);
    
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, data["total"].as<int>(), "total should be 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, data["offset"].as<int>(), "offset should be 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, data["limit"].as<int>(), "limit should be 20");
}

void test_http_effects_encode_current_allow_list() {
    HttpEffectsCurrentData currentData;
    currentData.effectId = 2;
    currentData.name = "TestEffect";
    currentData.brightness = 100;
    currentData.speed = 10;
    currentData.paletteId = 3;
    currentData.hue = 5;
    currentData.intensity = 30;
    currentData.saturation = 40;
    currentData.complexity = 50;
    currentData.variation = 60;
    currentData.isIEffect = true;
    currentData.description = "Desc";
    currentData.version = 1;
    currentData.hasVersion = true;

    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();
    HttpEffectsCodec::encodeCurrent(currentData, data);

    const char* keys[] = {"effectId", "name", "brightness", "speed", "paletteId", "hue",
                          "intensity", "saturation", "complexity", "variation", "isIEffect",
                          "description", "version"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, keys, sizeof(keys) / sizeof(keys[0])),
                             "current effect should only have allowed keys");
}

void test_http_effects_encode_families_allow_list() {
    HttpEffectsFamilyItemData familyItems[1];
    familyItems[0].id = 1;
    familyItems[0].name = "Wave";
    familyItems[0].count = 5;

    HttpEffectsFamiliesData familiesData;
    familiesData.families = familyItems;
    familiesData.familyCount = 1;
    familiesData.total = 1;

    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();
    HttpEffectsCodec::encodeFamilies(familiesData, data);

    const char* keys[] = {"families", "total"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, keys, sizeof(keys) / sizeof(keys[0])),
                             "families should only have families and total keys");
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
    
    // Decode tests
    RUN_TEST(test_http_effects_set_decode_basic);
    RUN_TEST(test_http_effects_set_decode_with_transition);
    RUN_TEST(test_http_effects_set_decode_missing_effectId);
    RUN_TEST(test_http_effects_set_decode_invalid_effectId_range);
    RUN_TEST(test_http_effects_parameters_set_decode_valid);
    RUN_TEST(test_http_effects_parameters_set_decode_missing_effectId);
    RUN_TEST(test_http_effects_parameters_set_decode_missing_parameters);
    RUN_TEST(test_http_effects_parameters_set_decode_parameters_not_object);
    
    // Encode allow-list tests
    RUN_TEST(test_http_effects_encode_list_pagination_allow_list);
    RUN_TEST(test_http_effects_encode_list_pagination_zero_values);
    RUN_TEST(test_http_effects_encode_current_allow_list);
    RUN_TEST(test_http_effects_encode_families_allow_list);
    
    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD
