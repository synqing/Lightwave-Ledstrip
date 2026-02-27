/**
 * @file test_ws_plugins_codec.cpp
 * @brief Unit tests for WsPluginsCodec JSON parsing and validation
 *
 * Tests plugin WebSocket command decoding with type checking, unknown-key rejection,
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
#include "../../src/codec/WsPluginsCodec.h"

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

// ============================================================================
// Test: Valid Plugins List
// ============================================================================

void test_plugins_list_valid() {
    const char* json = R"({"requestId": "list1"})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    PluginsDecodeResult result = WsPluginsCodec::decodePluginsList(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("list1", result.request.requestId);
}

void test_plugins_list_valid_no_requestId() {
    const char* json = R"({})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    PluginsDecodeResult result = WsPluginsCodec::decodePluginsList(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("", result.request.requestId);
}

// ============================================================================
// Test: Valid Plugins Stats
// ============================================================================

void test_plugins_stats_valid() {
    const char* json = R"({"requestId": "stats1"})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    PluginsDecodeResult result = WsPluginsCodec::decodePluginsStats(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("stats1", result.request.requestId);
}

void test_plugins_stats_valid_no_requestId() {
    const char* json = R"({})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    PluginsDecodeResult result = WsPluginsCodec::decodePluginsStats(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("", result.request.requestId);
}

// ============================================================================
// Test: Valid Plugins Reload
// ============================================================================

void test_plugins_reload_valid() {
    const char* json = R"({"requestId": "reload1"})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    PluginsDecodeResult result = WsPluginsCodec::decodePluginsReload(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("reload1", result.request.requestId);
}

void test_plugins_reload_valid_no_requestId() {
    const char* json = R"({})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    PluginsDecodeResult result = WsPluginsCodec::decodePluginsReload(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("", result.request.requestId);
}

// ============================================================================
// Test: Unknown Key (Drift-Killer)
// ============================================================================

void test_plugins_list_unknown_key() {
    const char* json = R"({"requestId": "test", "typo": "value"})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    PluginsDecodeResult result = WsPluginsCodec::decodePluginsList(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(
        strstr(result.errorMsg, "Unknown key") != nullptr &&
        strstr(result.errorMsg, "typo") != nullptr,
        "Error should mention unknown key 'typo'");
}

void test_plugins_stats_unknown_key() {
    const char* json = R"({"requestId": "test", "invalidField": 123})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    PluginsDecodeResult result = WsPluginsCodec::decodePluginsStats(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(
        strstr(result.errorMsg, "Unknown key") != nullptr,
        "Error should mention unknown key");
}

void test_plugins_reload_unknown_key() {
    const char* json = R"({"requestId": "test", "extra": true})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    PluginsDecodeResult result = WsPluginsCodec::decodePluginsReload(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(
        strstr(result.errorMsg, "Unknown key") != nullptr,
        "Error should mention unknown key");
}

// ============================================================================
// Test: Default Handling via operator|
// ============================================================================

void test_plugins_list_default_requestId() {
    const char* json = R"({})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    PluginsDecodeResult result = WsPluginsCodec::decodePluginsList(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("", result.request.requestId);
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
    RUN_TEST(test_plugins_list_valid);
    RUN_TEST(test_plugins_list_valid_no_requestId);
    RUN_TEST(test_plugins_stats_valid);
    RUN_TEST(test_plugins_stats_valid_no_requestId);
    RUN_TEST(test_plugins_reload_valid);
    RUN_TEST(test_plugins_reload_valid_no_requestId);
    RUN_TEST(test_plugins_list_default_requestId);

    // Unknown keys (drift-killer)
    RUN_TEST(test_plugins_list_unknown_key);
    RUN_TEST(test_plugins_stats_unknown_key);
    RUN_TEST(test_plugins_reload_unknown_key);

    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD
