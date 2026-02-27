// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file test_http_parameter_codec.cpp
 * @brief Unit tests for HttpParameterCodec JSON parsing and encoder allow-list validation
 *
 * Tests HTTP parameter endpoint decoding and encoding allow-lists.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>
#include "../../src/codec/HttpParameterCodec.h"

using namespace lightwaveos::codec;

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

void test_parameters_decode_set_valid() {
    JsonDocument doc;
    doc["brightness"] = 128;
    doc["speed"] = 25;
    doc["paletteId"] = 2;
    doc["hue"] = 10;
    doc["mood"] = 200;

    HttpParametersSetDecodeResult result = HttpParameterCodec::decodeSet(doc.as<JsonObjectConst>());
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_TRUE(result.request.hasBrightness);
    TEST_ASSERT_EQUAL_UINT8(128, result.request.brightness);
    TEST_ASSERT_TRUE(result.request.hasSpeed);
    TEST_ASSERT_EQUAL_UINT8(25, result.request.speed);
    TEST_ASSERT_TRUE(result.request.hasPaletteId);
    TEST_ASSERT_EQUAL_UINT8(2, result.request.paletteId);
    TEST_ASSERT_TRUE(result.request.hasHue);
    TEST_ASSERT_EQUAL_UINT8(10, result.request.hue);
    TEST_ASSERT_TRUE(result.request.hasMood);
    TEST_ASSERT_EQUAL_UINT8(200, result.request.mood);
}

void test_parameters_encode_get_extended_allowlist() {
    HttpParametersGetExtendedData data;
    data.brightness = 100;
    data.speed = 15;
    data.paletteId = 4;
    data.hue = 12;
    data.intensity = 42;
    data.saturation = 130;
    data.complexity = 64;
    data.variation = 80;
    data.mood = 120;
    data.fadeAmount = 33;

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    HttpParameterCodec::encodeGetExtended(data, obj);

    const char* allowedKeys[] = {
        "brightness", "speed", "paletteId", "hue", "intensity", "saturation",
        "complexity", "variation", "mood", "fadeAmount"
    };
    TEST_ASSERT_TRUE(validateKeysAgainstAllowList(obj, allowedKeys, sizeof(allowedKeys) / sizeof(allowedKeys[0])));
}

void setUp(void) {}
void tearDown(void) {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_parameters_decode_set_valid);
    RUN_TEST(test_parameters_encode_get_extended_allowlist);
    return UNITY_END();
}

#endif
