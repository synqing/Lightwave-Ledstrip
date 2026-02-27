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
#include "../../src/effects/zones/ZoneComposer.h"

using namespace lightwaveos::codec;

// ============================================================================
// Native Test Stubs (minimal implementations for encoder tests)
// ============================================================================

namespace lightwaveos {
namespace zones {

ZoneComposer::ZoneComposer() {}

uint8_t ZoneComposer::getZoneEffect(uint8_t) const { return 7; }
uint8_t ZoneComposer::getZoneBrightness(uint8_t) const { return 140; }
uint8_t ZoneComposer::getZoneSpeed(uint8_t) const { return 33; }
uint8_t ZoneComposer::getZonePalette(uint8_t) const { return 4; }
BlendMode ZoneComposer::getZoneBlendMode(uint8_t) const { return BlendMode::ALPHA; }

} // namespace zones
} // namespace lightwaveos

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
 * @brief Count keys in a JsonObjectConst
 */
static size_t countKeys(JsonObjectConst obj) {
    size_t count = 0;
    for (JsonPairConst kv : obj) {
        (void)kv;
        count++;
    }
    return count;
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
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, countKeys(data.as<JsonObjectConst>()), "No extra keys allowed");
}

void test_encode_zones_layout_changed() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsZonesCodec::encodeZonesLayoutChanged(3, data);

    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("zoneCount"), "zoneCount should be present");
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, data["zoneCount"].as<int>(), "zoneCount should be 3");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, countKeys(data.as<JsonObjectConst>()), "No extra keys allowed");
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
    TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(lightwaveos::zones::BlendMode::ALPHA), current["blendMode"].as<int>(), "blendMode should match stub");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("Alpha", current["blendModeName"].as<const char*>(), "blendModeName should match stub");

    TEST_ASSERT_EQUAL_INT_MESSAGE(3, countKeys(data.as<JsonObjectConst>()), "No extra keys allowed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(6, countKeys(current.as<JsonObjectConst>()), "No extra keys allowed in current");
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

    TEST_ASSERT_EQUAL_INT_MESSAGE(3, countKeys(data.as<JsonObjectConst>()), "No extra keys allowed");
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
    TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(lightwaveos::zones::BlendMode::ALPHA), current["blendMode"].as<int>(), "blendMode should match stub");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("Alpha", current["blendModeName"].as<const char*>(), "blendModeName should match stub");

    TEST_ASSERT_EQUAL_INT_MESSAGE(2, countKeys(data.as<JsonObjectConst>()), "No extra keys allowed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(7, countKeys(current.as<JsonObjectConst>()), "No extra keys allowed in current");
}

void test_encode_zone_palette_changed() {
    lightwaveos::zones::ZoneComposer composer;

    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsZonesCodec::encodeZonePaletteChanged(2, 6, composer, nullptr, data);

    JsonObject current = data["current"].as<JsonObject>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(6, current["paletteId"].as<int>(), "paletteId should be 6");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", current["effectName"].as<const char*>(), "effectName should be empty with null renderer");

    TEST_ASSERT_EQUAL_INT_MESSAGE(2, countKeys(data.as<JsonObjectConst>()), "No extra keys allowed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(7, countKeys(current.as<JsonObjectConst>()), "No extra keys allowed in current");
}

void test_encode_zone_blend_changed() {
    lightwaveos::zones::ZoneComposer composer;

    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsZonesCodec::encodeZoneBlendChanged(2, 3, composer, nullptr, data);

    JsonObject current = data["current"].as<JsonObject>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, current["blendMode"].as<int>(), "blendMode should be 3");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("Screen", current["blendModeName"].as<const char*>(), "blendModeName should match blend mode");

    TEST_ASSERT_EQUAL_INT_MESSAGE(2, countKeys(data.as<JsonObjectConst>()), "No extra keys allowed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(7, countKeys(current.as<JsonObjectConst>()), "No extra keys allowed in current");
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

    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD
