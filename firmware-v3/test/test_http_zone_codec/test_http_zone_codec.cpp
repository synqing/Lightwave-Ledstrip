/**
 * @file test_http_zone_codec.cpp
 * @brief Unit tests for HttpZoneCodec JSON parsing and encoder allow-list validation
 *
 * Tests HTTP zone endpoint decoding and encoding allow-lists.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>
#include "../../src/codec/HttpZoneCodec.h"

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

void test_zone_decode_set_effect_valid() {
    JsonDocument doc;
    doc["effectId"] = 5;
    HttpZoneSetEffectDecodeResult result = HttpZoneCodec::decodeSetEffect(doc.as<JsonObjectConst>());
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_UINT8(5, result.effectId);
}

void test_zone_encode_list_full_allowlist() {
    HttpZoneSegmentData segments[1];
    segments[0].zoneId = 0;
    segments[0].s1LeftStart = 0;
    segments[0].s1LeftEnd = 10;
    segments[0].s1RightStart = 0;
    segments[0].s1RightEnd = 10;
    segments[0].totalLeds = 22;

    HttpZoneListItemData zones[1];
    zones[0].id = 0;
    zones[0].enabled = true;
    zones[0].effectId = 1;
    zones[0].effectName = "Test";
    zones[0].brightness = 100;
    zones[0].speed = 10;
    zones[0].paletteId = 2;
    zones[0].blendMode = 1;
    zones[0].blendModeName = "Normal";

    HttpZonePresetData presets[1];
    presets[0].id = 0;
    presets[0].name = "Preset";

    HttpZoneListFullData data;
    data.enabled = true;
    data.zoneCount = 1;
    data.segments = segments;
    data.segmentCount = 1;
    data.zones = zones;
    data.zoneItemCount = 1;
    data.presets = presets;
    data.presetCount = 1;

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    HttpZoneCodec::encodeListFull(data, obj);

    const char* allowedKeys[] = {"enabled", "zoneCount", "segments", "zones", "presets"};
    TEST_ASSERT_TRUE(validateKeysAgainstAllowList(obj, allowedKeys, sizeof(allowedKeys) / sizeof(allowedKeys[0])));
}

void test_zone_encode_set_result_allowlist() {
    HttpZoneSetResultData data;
    data.zoneId = 1;
    data.hasBrightness = true;
    data.brightness = 120;
    data.hasEnabled = true;
    data.enabled = true;

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    HttpZoneCodec::encodeSetResult(data, obj);

    const char* allowedKeys[] = {"zoneId", "brightness", "enabled"};
    TEST_ASSERT_TRUE(validateKeysAgainstAllowList(obj, allowedKeys, sizeof(allowedKeys) / sizeof(allowedKeys[0])));
}

void setUp(void) {}
void tearDown(void) {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_zone_decode_set_effect_valid);
    RUN_TEST(test_zone_encode_list_full_allowlist);
    RUN_TEST(test_zone_encode_set_result_allowlist);
    return UNITY_END();
}

#endif
