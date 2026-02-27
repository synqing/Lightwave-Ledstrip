/**
 * @file test_ws_effects_codec.cpp
 * @brief Unit tests for WsEffectsCodec JSON parsing and validation
 *
 * Tests effects WebSocket command decoding with type checking, unknown-key rejection,
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
#include "../../src/codec/WsEffectsCodec.h"

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

void test_effects_simple_valid() {
    const char* json = R"({"requestId": "test123"})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    EffectsSimpleDecodeResult result = WsEffectsCodec::decodeSimple(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("test123", result.request.requestId);
}

void test_effects_simple_valid_no_requestId() {
    const char* json = R"({})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    EffectsSimpleDecodeResult result = WsEffectsCodec::decodeSimple(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("", result.request.requestId);
}

// ============================================================================
// Test: Encoder Functions (Response Encoding)
// ============================================================================

void test_encode_get_current() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsEffectsCodec::encodeGetCurrent(5, "TestEffect", 200, 25, 3, 180, 150, 200, 100, 50, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(5, data["effectId"].as<uint8_t>(), "effectId should be 5");
    TEST_ASSERT_EQUAL_STRING("TestEffect", data["name"].as<const char*>());
    TEST_ASSERT_EQUAL_INT_MESSAGE(200, data["brightness"].as<uint8_t>(), "brightness should be 200");
    TEST_ASSERT_EQUAL_INT_MESSAGE(25, data["speed"].as<uint8_t>(), "speed should be 25");
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, data["paletteId"].as<uint8_t>(), "paletteId should be 3");
    TEST_ASSERT_EQUAL_INT_MESSAGE(180, data["hue"].as<uint8_t>(), "hue should be 180");
    TEST_ASSERT_EQUAL_INT_MESSAGE(150, data["intensity"].as<uint8_t>(), "intensity should be 150");
    TEST_ASSERT_EQUAL_INT_MESSAGE(200, data["saturation"].as<uint8_t>(), "saturation should be 200");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, data["complexity"].as<uint8_t>(), "complexity should be 100");
    TEST_ASSERT_EQUAL_INT_MESSAGE(50, data["variation"].as<uint8_t>(), "variation should be 50");
    
    const char* allowedKeys[] = {"effectId", "name", "brightness", "speed", "paletteId", "hue", "intensity", "saturation", "complexity", "variation"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 10),
        "Should only have required keys, no extras allowed");
}

void test_encode_changed() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsEffectsCodec::encodeChanged(10, "NewEffect", true, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(10, data["effectId"].as<uint8_t>(), "effectId should be 10");
    TEST_ASSERT_EQUAL_STRING("NewEffect", data["name"].as<const char*>());
    TEST_ASSERT_TRUE_MESSAGE(data["transitionActive"].as<bool>(), "transitionActive should be true");
    
    const char* allowedKeys[] = {"effectId", "name", "transitionActive"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 3),
        "Should only have required keys, no extras allowed");
}

void test_encode_metadata() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsEffectsCodec::encodeMetadata(5, "TestEffect", "Interference", 0, "A test story", "Optical intent", 0x25, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(5, data["id"].as<uint8_t>(), "id should be 5");
    TEST_ASSERT_EQUAL_STRING("TestEffect", data["name"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("Interference", data["family"].as<const char*>());
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, data["familyId"].as<uint8_t>(), "familyId should be 0");
    TEST_ASSERT_EQUAL_STRING("A test story", data["story"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("Optical intent", data["opticalIntent"].as<const char*>());
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("tags"), "tags array should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("properties"), "properties object should be present");
    
    JsonArray tags = data["tags"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, tags.size(), "tags should have 3 entries (STANDING, CENTER_ORIGIN, PHYSICS)");
    
    JsonObject properties = data["properties"].as<JsonObject>();
    TEST_ASSERT_TRUE_MESSAGE(properties["centerOrigin"].as<bool>(), "centerOrigin should be true");
    TEST_ASSERT_TRUE_MESSAGE(properties["symmetricStrips"].as<bool>(), "symmetricStrips should be true");
    TEST_ASSERT_TRUE_MESSAGE(properties["paletteAware"].as<bool>(), "paletteAware should be true");
    TEST_ASSERT_TRUE_MESSAGE(properties["speedResponsive"].as<bool>(), "speedResponsive should be true");
    
    const char* allowedKeys[] = {"id", "name", "family", "familyId", "story", "opticalIntent", "tags", "properties"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 8),
        "Should only have required keys, no extras allowed");
    
    const char* propertiesKeys[] = {"centerOrigin", "symmetricStrips", "paletteAware", "speedResponsive"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(properties, propertiesKeys, 4),
        "Properties object should only have required keys, no extras allowed");
}

void test_encode_list() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    const char* effectNames[] = {"Effect0", "Effect1", "Effect2", nullptr, nullptr};
    const char* categories[] = {"Classic", "Wave", "Physics", nullptr, nullptr};
    
    WsEffectsCodec::encodeList(50, 0, 3, 1, 20, true, effectNames, categories, data);

    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("effects"), "effects array should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("pagination"), "pagination object should be present");
    
    JsonArray effects = data["effects"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, effects.size(), "effects array should have 3 entries");
    
    JsonObject pagination = data["pagination"].as<JsonObject>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, pagination["page"].as<uint8_t>(), "page should be 1");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, pagination["limit"].as<uint8_t>(), "limit should be 20");
    TEST_ASSERT_EQUAL_INT_MESSAGE(50, pagination["total"].as<uint8_t>(), "total should be 50");
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, pagination["pages"].as<uint8_t>(), "pages should be 3");
    
    const char* allowedKeys[] = {"effects", "pagination"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 2),
        "Should only have effects and pagination keys, no extras allowed");
    
    const char* paginationKeys[] = {"page", "limit", "total", "pages"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(pagination, paginationKeys, 4),
        "Pagination object should only have required keys, no extras allowed");
}

void test_encode_by_family() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    uint8_t patternIndices[] = {5, 10, 15, 20};
    WsEffectsCodec::encodeByFamily(2, "Advanced Optical", patternIndices, 4, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(2, data["familyId"].as<uint8_t>(), "familyId should be 2");
    TEST_ASSERT_EQUAL_STRING("Advanced Optical", data["familyName"].as<const char*>());
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, data["count"].as<uint8_t>(), "count should be 4");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("effects"), "effects array should be present");
    
    JsonArray effects = data["effects"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, effects.size(), "effects array should have 4 entries");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, effects[0].as<uint8_t>(), "first effect should be 5");
    
    const char* allowedKeys[] = {"familyId", "familyName", "effects", "count"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 4),
        "Should only have required keys, no extras allowed");
}

void test_encode_categories() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    const char* familyNames[] = {"Interference", "Geometric", "Advanced Optical", "Organic", "Quantum", "Color Mixing", "Physics-Based", "Novel Physics", "Fluid & Plasma", "Mathematical"};
    uint8_t familyCounts[] = {13, 8, 6, 12, 11, 12, 6, 5, 5, 5};
    
    WsEffectsCodec::encodeCategories(familyNames, familyCounts, 10, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(10, data["total"].as<uint8_t>(), "total should be 10");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("categories"), "categories array should be present");
    
    JsonArray categories = data["categories"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(10, categories.size(), "categories array should have 10 entries");
    
    JsonObject first = categories[0].as<JsonObject>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, first["id"].as<uint8_t>(), "first category id should be 0");
    TEST_ASSERT_EQUAL_STRING("Interference", first["name"].as<const char*>());
    TEST_ASSERT_EQUAL_INT_MESSAGE(13, first["count"].as<uint8_t>(), "first category count should be 13");
    
    const char* allowedKeys[] = {"categories", "total"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 2),
        "Should only have categories and total keys, no extras allowed");
}

void test_encode_parameters_get() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    const char* paramNames[] = {"speed", "intensity", nullptr, nullptr};
    const char* paramDisplayNames[] = {"Speed", "Intensity", nullptr, nullptr};
    float paramMins[] = {0.0f, 0.0f, 0.0f, 0.0f};
    float paramMaxs[] = {1.0f, 1.0f, 0.0f, 0.0f};
    float paramDefaults[] = {0.5f, 0.7f, 0.0f, 0.0f};
    float paramValues[] = {0.6f, 0.8f, 0.0f, 0.0f};
    
    WsEffectsCodec::encodeParametersGet(5, "TestEffect", true, paramNames, paramDisplayNames, paramMins, paramMaxs, paramDefaults, paramValues, 2, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(5, data["effectId"].as<uint8_t>(), "effectId should be 5");
    TEST_ASSERT_EQUAL_STRING("TestEffect", data["name"].as<const char*>());
    TEST_ASSERT_TRUE_MESSAGE(data["hasParameters"].as<bool>(), "hasParameters should be true");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("parameters"), "parameters array should be present");
    
    JsonArray params = data["parameters"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, params.size(), "parameters array should have 2 entries");
    
    JsonObject firstParam = params[0].as<JsonObject>();
    TEST_ASSERT_EQUAL_STRING("speed", firstParam["name"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("Speed", firstParam["displayName"].as<const char*>());
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.0f, firstParam["min"].as<float>(), "min should be 0.0");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 1.0f, firstParam["max"].as<float>(), "max should be 1.0");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.5f, firstParam["default"].as<float>(), "default should be 0.5");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.6f, firstParam["value"].as<float>(), "value should be 0.6");
    
    const char* allowedKeys[] = {"effectId", "name", "hasParameters", "parameters"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 4),
        "Should only have required keys, no extras allowed");
}

void test_encode_parameters_set_changed() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    const char* queuedKeys[] = {"speed", "intensity", nullptr, nullptr};
    const char* failedKeys[] = {"unknown", nullptr, nullptr, nullptr};
    
    WsEffectsCodec::encodeParametersSetChanged(5, "TestEffect", queuedKeys, 2, failedKeys, 1, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(5, data["effectId"].as<uint8_t>(), "effectId should be 5");
    TEST_ASSERT_EQUAL_STRING("TestEffect", data["name"].as<const char*>());
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("queued"), "queued array should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("failed"), "failed array should be present");
    
    JsonArray queued = data["queued"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, queued.size(), "queued array should have 2 entries");
    TEST_ASSERT_EQUAL_STRING("speed", queued[0].as<const char*>());
    
    JsonArray failed = data["failed"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, failed.size(), "failed array should have 1 entry");
    TEST_ASSERT_EQUAL_STRING("unknown", failed[0].as<const char*>());
    
    const char* allowedKeys[] = {"effectId", "name", "queued", "failed"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 4),
        "Should only have required keys, no extras allowed");
}

void test_encode_global_parameters_get() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsEffectsCodec::encodeGlobalParametersGet(200, 25, 3, 180, 150, 200, 100, 50, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(200, data["brightness"].as<uint8_t>(), "brightness should be 200");
    TEST_ASSERT_EQUAL_INT_MESSAGE(25, data["speed"].as<uint8_t>(), "speed should be 25");
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, data["paletteId"].as<uint8_t>(), "paletteId should be 3");
    TEST_ASSERT_EQUAL_INT_MESSAGE(180, data["hue"].as<uint8_t>(), "hue should be 180");
    TEST_ASSERT_EQUAL_INT_MESSAGE(150, data["intensity"].as<uint8_t>(), "intensity should be 150");
    TEST_ASSERT_EQUAL_INT_MESSAGE(200, data["saturation"].as<uint8_t>(), "saturation should be 200");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, data["complexity"].as<uint8_t>(), "complexity should be 100");
    TEST_ASSERT_EQUAL_INT_MESSAGE(50, data["variation"].as<uint8_t>(), "variation should be 50");
    
    const char* allowedKeys[] = {"brightness", "speed", "paletteId", "hue", "intensity", "saturation", "complexity", "variation"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 8),
        "Should only have required keys, no extras allowed");
}

void test_encode_parameters_changed() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    const char* updatedKeys[] = {"brightness", "speed", "paletteId", nullptr, nullptr};
    
    WsEffectsCodec::encodeParametersChanged(updatedKeys, 3, 200, 25, 3, 180, 150, 200, 100, 50, data);

    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("updated"), "updated array should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("current"), "current object should be present");
    
    JsonArray updated = data["updated"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, updated.size(), "updated array should have 3 entries");
    TEST_ASSERT_EQUAL_STRING("brightness", updated[0].as<const char*>());
    
    JsonObject current = data["current"].as<JsonObject>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(200, current["brightness"].as<uint8_t>(), "current brightness should be 200");
    TEST_ASSERT_EQUAL_INT_MESSAGE(25, current["speed"].as<uint8_t>(), "current speed should be 25");
    
    const char* allowedKeys[] = {"updated", "current"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 2),
        "Should only have updated and current keys, no extras allowed");
    
    const char* currentKeys[] = {"brightness", "speed", "paletteId", "hue", "intensity", "saturation", "complexity", "variation"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(current, currentKeys, 8),
        "Current object should only have required keys, no extras allowed");
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
    RUN_TEST(test_effects_simple_valid);
    RUN_TEST(test_effects_simple_valid_no_requestId);

    // Encoder tests (response payloads)
    RUN_TEST(test_encode_get_current);
    RUN_TEST(test_encode_changed);
    RUN_TEST(test_encode_metadata);
    RUN_TEST(test_encode_list);
    RUN_TEST(test_encode_by_family);
    RUN_TEST(test_encode_categories);
    RUN_TEST(test_encode_parameters_get);
    RUN_TEST(test_encode_parameters_set_changed);
    RUN_TEST(test_encode_global_parameters_get);
    RUN_TEST(test_encode_parameters_changed);

    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD
