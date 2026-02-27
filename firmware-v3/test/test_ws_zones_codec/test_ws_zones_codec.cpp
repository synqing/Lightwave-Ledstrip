// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file test_ws_zones_codec.cpp
 * @brief Unit tests for WsZonesCodec JSON parsing and validation
 *
 * Tests zone WebSocket command decoding with type checking, unknown-key rejection,
 * and default value handling.
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
#include "../../src/codec/WsZonesCodec.h"
#include "../../src/codec/ZoneComposerStub.h"

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
 * @brief Count keys in a JsonObjectConst (deprecated, use validateKeysAgainstAllowList)
 */
static size_t countKeys(const JsonObject& obj) {
    size_t count = 0;
    for (JsonPair kv : obj) {
        (void)kv;
        count++;
    }
    return count;
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
// Test: Valid Zone Enable
// ============================================================================

void test_zone_enable_valid() {
    const char* json = R"({"enable": true, "requestId": "test123"})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    ZoneEnableDecodeResult result = WsZonesCodec::decodeZoneEnable(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_TRUE_MESSAGE(result.request.enable, "enable should be true");
    TEST_ASSERT_EQUAL_STRING("test123", result.request.requestId);
}

void test_zone_enable_valid_no_requestId() {
    const char* json = R"({"enable": false})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    ZoneEnableDecodeResult result = WsZonesCodec::decodeZoneEnable(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_FALSE_MESSAGE(result.request.enable, "enable should be false");
    TEST_ASSERT_EQUAL_STRING("", result.request.requestId);
}

// ============================================================================
// Test: Valid Zone Set Effect
// ============================================================================

void test_zone_set_effect_valid() {
    const char* json = R"({"zoneId": 1, "effectId": 5, "requestId": "req1"})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    ZoneSetEffectDecodeResult result = WsZonesCodec::decodeZoneSetEffect(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, result.request.zoneId, "zoneId should be 1");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, result.request.effectId, "effectId should be 5");
    TEST_ASSERT_EQUAL_STRING("req1", result.request.requestId);
}

// ============================================================================
// Test: Missing Required Field
// ============================================================================

void test_zone_set_effect_missing_zoneId() {
    const char* json = R"({"effectId": 5})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    ZoneSetEffectDecodeResult result = WsZonesCodec::decodeZoneSetEffect(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(
        strstr(result.errorMsg, "Missing required field") != nullptr ||
        strstr(result.errorMsg, "zoneId") != nullptr,
        "Error should mention missing zoneId");
}

void test_zone_set_effect_missing_effectId() {
    const char* json = R"({"zoneId": 1})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    ZoneSetEffectDecodeResult result = WsZonesCodec::decodeZoneSetEffect(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(
        strstr(result.errorMsg, "Missing required field") != nullptr ||
        strstr(result.errorMsg, "effectId") != nullptr,
        "Error should mention missing effectId");
}

// ============================================================================
// Test: Wrong Type
// ============================================================================

void test_zone_set_effect_wrong_type_zoneId() {
    const char* json = R"({"zoneId": "invalid", "effectId": 5})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    ZoneSetEffectDecodeResult result = WsZonesCodec::decodeZoneSetEffect(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(
        strstr(result.errorMsg, "Missing required field") != nullptr ||
        strstr(result.errorMsg, "zoneId") != nullptr,
        "Error should mention wrong type for zoneId");
}

void test_zone_set_effect_wrong_type_effectId() {
    const char* json = R"({"zoneId": 1, "effectId": false})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    ZoneSetEffectDecodeResult result = WsZonesCodec::decodeZoneSetEffect(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(
        strstr(result.errorMsg, "Missing required field") != nullptr ||
        strstr(result.errorMsg, "effectId") != nullptr,
        "Error should mention wrong type for effectId");
}

// ============================================================================
// Test: Unknown Key (Drift-Killer)
// ============================================================================

void test_zone_set_effect_unknown_key() {
    const char* json = R"({"zoneId": 0, "effectId": 1, "typo": "value"})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    ZoneSetEffectDecodeResult result = WsZonesCodec::decodeZoneSetEffect(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(
        strstr(result.errorMsg, "Unknown key") != nullptr &&
        strstr(result.errorMsg, "typo") != nullptr,
        "Error should mention unknown key 'typo'");
}

void test_zone_enable_unknown_key() {
    const char* json = R"({"enable": true, "extraField": 123})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    ZoneEnableDecodeResult result = WsZonesCodec::decodeZoneEnable(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(
        strstr(result.errorMsg, "Unknown key") != nullptr,
        "Error should mention unknown key");
}

// ============================================================================
// Test: Default Handling via operator|
// ============================================================================

void test_zone_set_brightness_default_requestId() {
    const char* json = R"({"zoneId": 2, "brightness": 200})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    ZoneSetBrightnessDecodeResult result = WsZonesCodec::decodeZoneSetBrightness(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, result.request.zoneId, "zoneId should be 2");
    TEST_ASSERT_EQUAL_INT_MESSAGE(200, result.request.brightness, "brightness should be 200");
    TEST_ASSERT_EQUAL_STRING("", result.request.requestId);
}

// ============================================================================
// Test: Numeric-Default Typing Edge Case
// ============================================================================

void test_zone_load_preset_default_presetId() {
    const char* json = R"({})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    ZoneLoadPresetDecodeResult result = WsZonesCodec::decodeZoneLoadPreset(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, result.request.presetId, "presetId should default to 0");
}

void test_zone_load_preset_valid_range() {
    const char* json = R"({"presetId": 4})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    ZoneLoadPresetDecodeResult result = WsZonesCodec::decodeZoneLoadPreset(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, result.request.presetId, "presetId should be 4");
}

void test_zone_load_preset_out_of_range() {
    const char* json = R"({"presetId": 10})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    ZoneLoadPresetDecodeResult result = WsZonesCodec::decodeZoneLoadPreset(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(
        strstr(result.errorMsg, "out of range") != nullptr ||
        strstr(result.errorMsg, "presetId") != nullptr,
        "Error should mention presetId out of range");
}

// ============================================================================
// Test: Zones Get (minimal request)
// ============================================================================

void test_zones_get_valid() {
    const char* json = R"({"requestId": "get1"})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    ZonesGetDecodeResult result = WsZonesCodec::decodeZonesGet(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("get1", result.request.requestId);
}

void test_zones_get_unknown_key() {
    const char* json = R"({"requestId": "get1", "invalid": true})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    ZonesGetDecodeResult result = WsZonesCodec::decodeZonesGet(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(
        strstr(result.errorMsg, "Unknown key") != nullptr,
        "Error should mention unknown key");
}

// ============================================================================
// Encoder Tests: Zone Event Payloads
// ============================================================================

void test_encode_zone_enabled_changed() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsZonesCodec::encodeZoneEnabledChanged(true, data);

    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("enabled"), "enabled should be present");
    TEST_ASSERT_TRUE_MESSAGE(data["enabled"].as<bool>(), "enabled should be true");
    const char* allowedKeys[] = {"enabled"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 1),
        "Should only have 'enabled' key, no extras allowed");
}

void test_encode_zones_layout_changed() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsZonesCodec::encodeZonesLayoutChanged(3, data);

    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("zoneCount"), "zoneCount should be present");
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, data["zoneCount"].as<int>(), "zoneCount should be 3");
    const char* allowedKeys[] = {"zoneCount"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 1),
        "Should only have 'zoneCount' key, no extras allowed");
}

void test_encode_zones_changed_single_field() {
    lightwaveos::zones::ZoneComposer composer;
    const char* updatedFields[] = {"brightness"};

    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsZonesCodec::encodeZonesChanged(1, updatedFields, 1, composer, nullptr, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, data["zoneId"].as<int>(), "zoneId should be 1");
    JsonArray updated = data["updated"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, updated.size(), "updated should have 1 entry");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("brightness", updated[0].as<const char*>(), "updated field should be brightness");

    JsonObject current = data["current"].as<JsonObject>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(7, current["effectId"].as<int>(), "effectId should match stub");
    TEST_ASSERT_EQUAL_INT_MESSAGE(140, current["brightness"].as<int>(), "brightness should match stub");
    TEST_ASSERT_EQUAL_INT_MESSAGE(33, current["speed"].as<int>(), "speed should match stub");
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, current["paletteId"].as<int>(), "paletteId should match stub");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, current["blendMode"].as<int>(), "blendMode should match stub");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("Alpha", current["blendModeName"].as<const char*>(), "blendModeName should match stub");

    const char* allowedKeys[] = {"zoneId", "updated", "current"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 3),
        "Should only have zoneId, updated, current keys, no extras allowed");
    const char* allowedCurrentKeys[] = {"effectId", "brightness", "speed", "paletteId", "blendMode", "blendModeName"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(current, allowedCurrentKeys, 6),
        "Should only have required current keys, no extras allowed");
}

void test_encode_zones_changed_multiple_fields() {
    lightwaveos::zones::ZoneComposer composer;
    const char* updatedFields[] = {"effectId", "speed", "paletteId"};

    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsZonesCodec::encodeZonesChanged(2, updatedFields, 3, composer, nullptr, data);

    JsonArray updated = data["updated"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, updated.size(), "updated should have 3 entries");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("effectId", updated[0].as<const char*>(), "updated field should be effectId");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("speed", updated[1].as<const char*>(), "updated field should be speed");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("paletteId", updated[2].as<const char*>(), "updated field should be paletteId");

    const char* allowedKeys[] = {"zoneId", "updated", "current"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 3),
        "Should only have zoneId, updated, current keys, no extras allowed");
}

void test_encode_zones_effect_changed() {
    lightwaveos::zones::ZoneComposer composer;

    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsZonesCodec::encodeZonesEffectChanged(1, 9, composer, nullptr, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, data["zoneId"].as<int>(), "zoneId should be 1");
    JsonObject current = data["current"].as<JsonObject>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(9, current["effectId"].as<int>(), "effectId should be 9");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", current["effectName"].as<const char*>(), "effectName should be empty with null renderer");
    TEST_ASSERT_EQUAL_INT_MESSAGE(140, current["brightness"].as<int>(), "brightness should match stub");
    TEST_ASSERT_EQUAL_INT_MESSAGE(33, current["speed"].as<int>(), "speed should match stub");
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, current["paletteId"].as<int>(), "paletteId should match stub");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, current["blendMode"].as<int>(), "blendMode should match stub");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("Alpha", current["blendModeName"].as<const char*>(), "blendModeName should match stub");

    const char* allowedKeys[] = {"zoneId", "current"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 2),
        "Should only have zoneId, current keys, no extras allowed");
    const char* allowedCurrentKeys[] = {"effectId", "effectName", "brightness", "speed", "paletteId", "blendMode", "blendModeName"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(current, allowedCurrentKeys, 7),
        "Should only have required current keys, no extras allowed");
}

void test_encode_zone_palette_changed() {
    lightwaveos::zones::ZoneComposer composer;

    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsZonesCodec::encodeZonePaletteChanged(2, 6, composer, nullptr, data);

    JsonObject current = data["current"].as<JsonObject>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(6, current["paletteId"].as<int>(), "paletteId should be 6");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", current["effectName"].as<const char*>(), "effectName should be empty with null renderer");

    const char* allowedKeys[] = {"zoneId", "current"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 2),
        "Should only have zoneId, current keys, no extras allowed");
    const char* allowedCurrentKeys[] = {"effectId", "effectName", "brightness", "speed", "paletteId", "blendMode", "blendModeName"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(current, allowedCurrentKeys, 7),
        "Should only have required current keys, no extras allowed");
}

void test_encode_zone_blend_changed() {
    lightwaveos::zones::ZoneComposer composer;

    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsZonesCodec::encodeZoneBlendChanged(2, 3, composer, nullptr, data);

    JsonObject current = data["current"].as<JsonObject>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, current["blendMode"].as<int>(), "blendMode should be 3");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("Alpha", current["blendModeName"].as<const char*>(), "blendModeName should match blend mode");

    const char* allowedKeys[] = {"zoneId", "current"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 2),
        "Should only have zoneId, current keys, no extras allowed");
    const char* allowedCurrentKeys[] = {"effectId", "effectName", "brightness", "speed", "paletteId", "blendMode", "blendModeName"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(current, allowedCurrentKeys, 7),
        "Should only have required current keys, no extras allowed");
}

// ============================================================================
// Test: Default Handling Gotcha (operator|, not as<T>())
// ============================================================================

void test_encode_default_handling_gotcha() {
    // This test documents the ArduinoJson gotcha:
    // - as<T>() returns library defaults (0, NULL, false, "")
    // - operator| deduces type from RHS (use typed literals like 0U, -1L)
    // This is a documentation test - encoder doesn't use defaults, but this
    // ensures we understand the pattern for any future optional fields
    JsonDocument doc;
    doc["optional"] = 42;
    
    // Correct: operator| for defaults
    uint8_t value1 = doc["optional"] | 0U;  // Type-safe default
    TEST_ASSERT_EQUAL_INT_MESSAGE(42, value1, "operator| should use provided value");
    
    uint8_t value2 = doc["missing"] | 255U;  // Explicit default
    TEST_ASSERT_EQUAL_INT_MESSAGE(255, value2, "operator| should use default when missing");
    
    // Wrong pattern (documented as anti-pattern):
    // uint8_t value3 = doc["missing"].as<uint8_t>();  // Returns 0, not your default
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
    RUN_TEST(test_zone_enable_valid);
    RUN_TEST(test_zone_enable_valid_no_requestId);
    RUN_TEST(test_zone_set_effect_valid);
    RUN_TEST(test_zone_set_brightness_default_requestId);
    RUN_TEST(test_zone_load_preset_default_presetId);
    RUN_TEST(test_zone_load_preset_valid_range);
    RUN_TEST(test_zones_get_valid);

    // Missing required fields
    RUN_TEST(test_zone_set_effect_missing_zoneId);
    RUN_TEST(test_zone_set_effect_missing_effectId);

    // Wrong types
    RUN_TEST(test_zone_set_effect_wrong_type_zoneId);
    RUN_TEST(test_zone_set_effect_wrong_type_effectId);

    // Unknown keys (drift-killer)
    RUN_TEST(test_zone_set_effect_unknown_key);
    RUN_TEST(test_zone_enable_unknown_key);
    RUN_TEST(test_zones_get_unknown_key);

    // Range validation
    RUN_TEST(test_zone_load_preset_out_of_range);

    // Encoder tests (event payloads)
    RUN_TEST(test_encode_zone_enabled_changed);
    RUN_TEST(test_encode_zones_layout_changed);
    RUN_TEST(test_encode_zones_changed_single_field);
    RUN_TEST(test_encode_zones_changed_multiple_fields);
    RUN_TEST(test_encode_zones_effect_changed);
    RUN_TEST(test_encode_zone_palette_changed);
    RUN_TEST(test_encode_zone_blend_changed);

    // Default handling gotcha (documentation)
    RUN_TEST(test_encode_default_handling_gotcha);

    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD
