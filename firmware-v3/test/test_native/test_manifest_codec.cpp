/**
 * @file test_manifest_codec.cpp
 * @brief Unit tests for ManifestCodec JSON parsing and validation
 *
 * Tests manifest schema versioning, type checking, unknown-key rejection,
 * and default value handling using golden JSON test files.
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
#include "../../src/codec/ManifestCodec.h"

using namespace lightwaveos::codec;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Load JSON file and parse into JsonDocument
 * 
 * Path is relative to firmware/v2 directory (where test is run from)
 */
static bool loadJsonFile(const char* path, JsonDocument& doc) {
    // Try multiple path locations (build dir vs source dir)
    std::string path1 = std::string("../../../../") + path;  // From .pio/build/native_codec_test_manifest/
    std::string path2 = std::string("../") + path;  // Alternative
    std::string path3 = path;  // Direct path
    
    std::ifstream file;
    file.open(path1);
    if (!file.is_open()) {
        file.open(path2);
        if (!file.is_open()) {
            file.open(path3);
        }
    }
    
    if (!file.is_open()) {
        return false;
    }

    std::string jsonStr((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    DeserializationError error = deserializeJson(doc, jsonStr);
    return error == DeserializationError::Ok;
}

// ============================================================================
// Test: Valid Schema 1 Manifest
// ============================================================================

void test_manifest_v1_valid() {
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(
        loadJsonFile("test/testdata/manifest_v1_valid.json", doc),
        "Failed to load manifest_v1_valid.json");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    ManifestDecodeResult result = ManifestCodec::decode(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, result.config.schemaVersion, "Should default to schema 1");
    TEST_ASSERT_EQUAL_STRING("Test Plugin V1", result.config.pluginName);
    TEST_ASSERT_FALSE_MESSAGE(result.config.overrideMode, "Should default to additive mode");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, result.config.effectCount, "Should have 2 effects");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, result.config.effectIds[0], "First effect ID should be 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, result.config.effectIds[1], "Second effect ID should be 1");
}

// ============================================================================
// Test: Valid Schema 2 Manifest
// ============================================================================

void test_manifest_v2_valid() {
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(
        loadJsonFile("test/testdata/manifest_v2_valid.json", doc),
        "Failed to load manifest_v2_valid.json");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    ManifestDecodeResult result = ManifestCodec::decode(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, result.config.schemaVersion, "Should be schema 2");
    TEST_ASSERT_EQUAL_STRING("Test Plugin V2", result.config.pluginName);
    TEST_ASSERT_FALSE_MESSAGE(result.config.overrideMode, "Should be additive mode");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, result.config.effectCount, "Should have 2 effects");
}

// ============================================================================
// Test: Missing Schema (Defaults to V1)
// ============================================================================

void test_manifest_missing_schema() {
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(
        loadJsonFile("test/testdata/manifest_missing_schema.json", doc),
        "Failed to load manifest_missing_schema.json");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    ManifestDecodeResult result = ManifestCodec::decode(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, result.config.schemaVersion, "Should default to schema 1");
}

// ============================================================================
// Test: Missing Required Field
// ============================================================================

void test_manifest_missing_required() {
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(
        loadJsonFile("test/testdata/manifest_missing_required.json", doc),
        "Failed to load manifest_missing_required.json");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    ManifestDecodeResult result = ManifestCodec::decode(root);

    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(
        strstr(result.errorMsg, "plugin.name") != nullptr ||
        strstr(result.errorMsg, "Missing required field") != nullptr,
        "Error should mention missing plugin.name");
}

// ============================================================================
// Test: Unknown Key in Schema 1 (Should Pass)
// ============================================================================

void test_manifest_unknown_key_v1() {
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(
        loadJsonFile("test/testdata/manifest_unknown_key_v1.json", doc),
        "Failed to load manifest_unknown_key_v1.json");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    ManifestDecodeResult result = ManifestCodec::decode(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Schema 1 should allow unknown keys");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, result.config.schemaVersion, "Should be schema 1");
}

// ============================================================================
// Test: Unknown Key in Schema 2 (Should Fail)
// ============================================================================

void test_manifest_unknown_key_v2() {
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(
        loadJsonFile("test/testdata/manifest_unknown_key_v2.json", doc),
        "Failed to load manifest_unknown_key_v2.json");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    ManifestDecodeResult result = ManifestCodec::decode(root);

    TEST_ASSERT_FALSE_MESSAGE(result.success, "Schema 2 should reject unknown keys");
    TEST_ASSERT_TRUE_MESSAGE(
        strstr(result.errorMsg, "Unknown key") != nullptr,
        "Error should mention unknown key");
}

// ============================================================================
// Test: Wrong Type for Required Field
// ============================================================================

void test_manifest_wrong_type() {
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(
        loadJsonFile("test/testdata/manifest_wrong_type.json", doc),
        "Failed to load manifest_wrong_type.json");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    ManifestDecodeResult result = ManifestCodec::decode(root);

    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(
        strstr(result.errorMsg, "plugin.name") != nullptr ||
        strstr(result.errorMsg, "must be a string") != nullptr,
        "Error should mention wrong type for plugin.name");
}

// ============================================================================
// Test: Future Schema Version (Should Reject)
// ============================================================================

void test_manifest_schema_3() {
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(
        loadJsonFile("test/testdata/manifest_schema_3.json", doc),
        "Failed to load manifest_schema_3.json");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    ManifestDecodeResult result = ManifestCodec::decode(root);

    TEST_ASSERT_FALSE_MESSAGE(result.success, "Future schema should be rejected");
    TEST_ASSERT_TRUE_MESSAGE(
        strstr(result.errorMsg, "Unsupported schema version") != nullptr ||
        strstr(result.errorMsg, "3") != nullptr,
        "Error should mention unsupported schema version 3");
}

// ============================================================================
// Test: Default Mode Handling
// ============================================================================

void test_manifest_default_mode() {
    // Create minimal valid manifest without mode field
    const char* json = R"({
      "version": "1.0",
      "plugin": {"name": "Test"},
      "effects": [{"id": 0}]
    })";

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    TEST_ASSERT_TRUE_MESSAGE(err == DeserializationError::Ok, "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    ManifestDecodeResult result = ManifestCodec::decode(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_FALSE_MESSAGE(result.config.overrideMode, "Mode should default to additive");
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

    RUN_TEST(test_manifest_v1_valid);
    RUN_TEST(test_manifest_v2_valid);
    RUN_TEST(test_manifest_missing_schema);
    RUN_TEST(test_manifest_missing_required);
    RUN_TEST(test_manifest_unknown_key_v1);
    RUN_TEST(test_manifest_unknown_key_v2);
    RUN_TEST(test_manifest_wrong_type);
    RUN_TEST(test_manifest_schema_3);
    RUN_TEST(test_manifest_default_mode);

    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD
