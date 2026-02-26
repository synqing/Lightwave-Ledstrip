/**
 * @file test_ws_palette_codec.cpp
 * @brief Unit tests for WsPaletteCodec decode contracts
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <ArduinoJson.h>
#include "../../src/codec/WsPaletteCodec.h"

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

// Decode tests

void test_palette_decode_list_defaults() {
    const char* json = R"({})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    PalettesListDecodeResult result = WsPaletteCodec::decodeList(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, result.request.page, "default page should be 1");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, result.request.limit, "default limit should be 20");
    TEST_ASSERT_EQUAL_STRING("", result.request.requestId);
}

void test_palette_decode_list_invalid_page() {
    const char* json = R"({"page":0})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    PalettesListDecodeResult result = WsPaletteCodec::decodeList(root);

    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail for page < 1");
}

void test_palette_decode_list_invalid_limit_zero() {
    const char* json = R"({"limit":0})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    PalettesListDecodeResult result = WsPaletteCodec::decodeList(root);

    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail for limit 0");
}

void test_palette_decode_list_invalid_limit_high() {
    const char* json = R"({"limit":51})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    PalettesListDecodeResult result = WsPaletteCodec::decodeList(root);

    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail for limit > 50");
}

void test_palette_decode_get_missing_palette_id() {
    const char* json = R"({})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    PalettesGetDecodeResult result = WsPaletteCodec::decodeGet(root);

    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail if paletteId missing");
    TEST_ASSERT_EQUAL_STRING("", result.request.requestId);
}

void test_palette_decode_get_negative_palette_id() {
    const char* json = R"({"paletteId":-1})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    PalettesGetDecodeResult result = WsPaletteCodec::decodeGet(root);

    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail if paletteId negative");
}

void test_palette_decode_get_request_id_optional() {
    const char* json = R"({"requestId":"r1","paletteId":3})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    PalettesGetDecodeResult result = WsPaletteCodec::decodeGet(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, result.request.paletteId, "paletteId should be 3");
    TEST_ASSERT_EQUAL_STRING("r1", result.request.requestId);
}

void test_palette_decode_set_missing_palette_id() {
    const char* json = R"({})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    PalettesSetDecodeResult result = WsPaletteCodec::decodeSet(root);

    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail if paletteId missing");
    TEST_ASSERT_EQUAL_STRING("", result.request.requestId);
}

void test_palette_decode_set_negative_palette_id() {
    const char* json = R"({"paletteId":-2})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    PalettesSetDecodeResult result = WsPaletteCodec::decodeSet(root);

    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail if paletteId negative");
}

void test_palette_decode_set_request_id_optional() {
    const char* json = R"({"requestId":"r2","paletteId":7})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    PalettesSetDecodeResult result = WsPaletteCodec::decodeSet(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(7, result.request.paletteId, "paletteId should be 7");
    TEST_ASSERT_EQUAL_STRING("r2", result.request.requestId);
}

void setUp(void) {}
void tearDown(void) {}

int main() {
    UNITY_BEGIN();

    // Decode
    RUN_TEST(test_palette_decode_list_defaults);
    RUN_TEST(test_palette_decode_list_invalid_page);
    RUN_TEST(test_palette_decode_list_invalid_limit_zero);
    RUN_TEST(test_palette_decode_list_invalid_limit_high);
    RUN_TEST(test_palette_decode_get_missing_palette_id);
    RUN_TEST(test_palette_decode_get_negative_palette_id);
    RUN_TEST(test_palette_decode_get_request_id_optional);
    RUN_TEST(test_palette_decode_set_missing_palette_id);
    RUN_TEST(test_palette_decode_set_negative_palette_id);
    RUN_TEST(test_palette_decode_set_request_id_optional);

    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD
