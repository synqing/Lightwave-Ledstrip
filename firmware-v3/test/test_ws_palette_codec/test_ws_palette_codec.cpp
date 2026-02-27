// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file test_ws_palette_codec.cpp
 * @brief Unit tests for WsPaletteCodec JSON parsing and encoder allow-list validation
 *
 * Tests palette WebSocket command decoding (defaults + range checks) and encoder
 * functions (response payload allow-lists).
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>
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

// ============================================================================
// Encode tests (response payload allow-lists)
// ============================================================================

void test_palette_encode_list_allow_list() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsPaletteCodec::PaletteSummary items[2] = {
        {1, "P1", "CatA"},
        {2, "P2", "CatB"},
    };

    WsPaletteCodec::encodePalettesList(items, 2, 1, 20, 100, 5, data);

    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("palettes"), "palettes array should be present");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("pagination"), "pagination object should be present");

    JsonArray palettes = data["palettes"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, palettes.size(), "palettes array should have 2 entries");

    const char* paletteKeys[] = {"id", "name", "category"};
    for (JsonVariant v : palettes) {
        JsonObject p = v.as<JsonObject>();
        TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(p, paletteKeys, 3), "palette object should only have id,name,category");
    }

    JsonObject pagination = data["pagination"].as<JsonObject>();
    const char* paginationKeys[] = {"page", "limit", "total", "pages"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(pagination, paginationKeys, 4), "pagination should only have required keys");

    const char* topKeys[] = {"palettes", "pagination"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, topKeys, 2), "top-level data should only have palettes+pagination");
}

void test_palette_encode_get_allow_list() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsPaletteCodec::PaletteFlags flags{true, false, true, false, true, false};
    WsPaletteCodec::encodePalettesGet(3, "P3", "CatC", flags, 120, 240, data);

    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("palette"), "palette object should be present");

    JsonObject palette = data["palette"].as<JsonObject>();
    const char* paletteKeys[] = {"id", "name", "category", "flags", "avgBrightness", "maxBrightness"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(palette, paletteKeys, 6), "palette should only have required keys");

    JsonObject flagsObj = palette["flags"].as<JsonObject>();
    const char* flagKeys[] = {"warm", "cool", "calm", "vivid", "cvdFriendly", "whiteHeavy"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(flagsObj, flagKeys, 6), "flags should only have required keys");

    const char* topKeys[] = {"palette"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, topKeys, 1), "top-level data should only have palette");
}

void test_palette_encode_set_allow_list() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsPaletteCodec::encodePalettesSet(7, "P7", "CatZ", data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(7, data["paletteId"].as<uint8_t>(), "paletteId should be 7");
    TEST_ASSERT_EQUAL_STRING("P7", data["name"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("CatZ", data["category"].as<const char*>());

    const char* keys[] = {"paletteId", "name", "category"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, keys, 3), "set response should only have paletteId,name,category");
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

    // Encode allow-lists
    RUN_TEST(test_palette_encode_list_allow_list);
    RUN_TEST(test_palette_encode_get_allow_list);
    RUN_TEST(test_palette_encode_set_allow_list);

    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD

