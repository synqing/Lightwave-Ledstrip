// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file test_ws_device_codec.cpp
 * @brief Unit tests for WsDeviceCodec JSON parsing and encoder allow-list validation
 *
 * Tests device WebSocket command decoding (requestId extraction) and encoder
 * functions (response payload allow-lists).
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>
#include "../../src/codec/WsDeviceCodec.h"

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

void test_device_decode_with_request_id() {
    const char* json = R"({"requestId": "test123"})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    DeviceDecodeResult result = WsDeviceCodec::decode(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("test123", result.request.requestId);
}

void test_device_decode_without_request_id() {
    const char* json = R"({})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    DeviceDecodeResult result = WsDeviceCodec::decode(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("", result.request.requestId);
}

// ============================================================================
// Encode tests (response payload allow-lists)
// ============================================================================

void test_device_encode_get_status_with_renderer() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsDeviceCodec::RendererStats stats{120, 85, 5000};
    WsDeviceCodec::NetworkInfo network{true, false, "192.168.1.100", -45};

    WsDeviceCodec::encodeDeviceGetStatus(3600, 100000, 327680, 240, &stats, network, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(3600, data["uptime"].as<uint32_t>(), "uptime should be 3600");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100000, data["freeHeap"].as<uint32_t>(), "freeHeap should be 100000");
    TEST_ASSERT_EQUAL_INT_MESSAGE(327680, data["heapSize"].as<uint32_t>(), "heapSize should be 327680");
    TEST_ASSERT_EQUAL_INT_MESSAGE(240, data["cpuFreq"].as<uint8_t>(), "cpuFreq should be 240");
    TEST_ASSERT_EQUAL_INT_MESSAGE(120, data["fps"].as<uint8_t>(), "fps should be 120");
    TEST_ASSERT_EQUAL_INT_MESSAGE(85, data["cpuPercent"].as<uint8_t>(), "cpuPercent should be 85");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5000, data["framesRendered"].as<uint32_t>(), "framesRendered should be 5000");

    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("network"), "network object should be present");
    JsonObject networkObj = data["network"].as<JsonObject>();
    TEST_ASSERT_TRUE_MESSAGE(networkObj["connected"].as<bool>(), "connected should be true");
    TEST_ASSERT_FALSE_MESSAGE(networkObj["apMode"].as<bool>(), "apMode should be false");
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", networkObj["ip"].as<const char*>());
    TEST_ASSERT_EQUAL_INT_MESSAGE(-45, networkObj["rssi"].as<int32_t>(), "rssi should be -45");

    const char* topKeys[] = {"uptime", "freeHeap", "heapSize", "cpuFreq", "fps", "cpuPercent", "framesRendered", "network"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, topKeys, 8), "top-level data should only have required keys");

    const char* networkKeys[] = {"connected", "apMode", "ip", "rssi"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(networkObj, networkKeys, 4), "network object should only have required keys");
}

void test_device_encode_get_status_without_renderer() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsDeviceCodec::NetworkInfo network{false, true, nullptr, 0};

    WsDeviceCodec::encodeDeviceGetStatus(1800, 150000, 327680, 240, nullptr, network, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(1800, data["uptime"].as<uint32_t>(), "uptime should be 1800");
    TEST_ASSERT_FALSE_MESSAGE(data.containsKey("fps"), "fps should not be present without renderer");
    TEST_ASSERT_FALSE_MESSAGE(data.containsKey("cpuPercent"), "cpuPercent should not be present without renderer");
    TEST_ASSERT_FALSE_MESSAGE(data.containsKey("framesRendered"), "framesRendered should not be present without renderer");

    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("network"), "network object should be present");
    JsonObject networkObj = data["network"].as<JsonObject>();
    TEST_ASSERT_FALSE_MESSAGE(networkObj["connected"].as<bool>(), "connected should be false");
    TEST_ASSERT_TRUE_MESSAGE(networkObj["apMode"].as<bool>(), "apMode should be true");
    TEST_ASSERT_FALSE_MESSAGE(networkObj.containsKey("ip"), "ip should not be present when not connected");
    TEST_ASSERT_FALSE_MESSAGE(networkObj.containsKey("rssi"), "rssi should not be present when not connected");

    const char* topKeys[] = {"uptime", "freeHeap", "heapSize", "cpuFreq", "network"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, topKeys, 5), "top-level data should only have required keys (no renderer stats)");

    const char* networkKeys[] = {"connected", "apMode"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(networkObj, networkKeys, 2), "network object should only have connected+apMode when not connected");
}

void test_device_encode_get_info_with_effect_count() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsDeviceCodec::encodeDeviceGetInfo("ESP32-S3", 1, 2, 240, 8388608, 200000, 327680, 1500000, 5000000, 50, data);

    TEST_ASSERT_EQUAL_STRING("ESP32-S3", data["chipModel"].as<const char*>());
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, data["chipRevision"].as<uint8_t>(), "chipRevision should be 1");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, data["chipCores"].as<uint8_t>(), "chipCores should be 2");
    TEST_ASSERT_EQUAL_INT_MESSAGE(240, data["cpuFreqMHz"].as<uint8_t>(), "cpuFreqMHz should be 240");
    TEST_ASSERT_EQUAL_INT_MESSAGE(8388608, data["flashSize"].as<uint32_t>(), "flashSize should be 8388608");
    TEST_ASSERT_EQUAL_INT_MESSAGE(200000, data["freeHeap"].as<uint32_t>(), "freeHeap should be 200000");
    TEST_ASSERT_EQUAL_INT_MESSAGE(327680, data["heapSize"].as<uint32_t>(), "heapSize should be 327680");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1500000, data["sketchSize"].as<uint32_t>(), "sketchSize should be 1500000");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5000000, data["freeSketchSpace"].as<uint32_t>(), "freeSketchSpace should be 5000000");
    TEST_ASSERT_EQUAL_INT_MESSAGE(50, data["effectCount"].as<uint8_t>(), "effectCount should be 50");

    const char* keys[] = {"chipModel", "chipRevision", "chipCores", "cpuFreqMHz", "flashSize", "freeHeap", "heapSize", "sketchSize", "freeSketchSpace", "effectCount"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, keys, 10), "getInfo should only have required keys");
}

void test_device_encode_get_info_without_effect_count() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsDeviceCodec::encodeDeviceGetInfo("ESP32-S3", 1, 2, 240, 8388608, 200000, 327680, 1500000, 5000000, 0, data);

    TEST_ASSERT_FALSE_MESSAGE(data.containsKey("effectCount"), "effectCount should not be present when 0");

    const char* keys[] = {"chipModel", "chipRevision", "chipCores", "cpuFreqMHz", "flashSize", "freeHeap", "heapSize", "sketchSize", "freeSketchSpace"};
    TEST_ASSERT_TRUE_MESSAGE(validateKeysAgainstAllowList(data, keys, 9), "getInfo should only have required keys (no effectCount)");
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
    RUN_TEST(test_device_decode_with_request_id);
    RUN_TEST(test_device_decode_without_request_id);

    // Encode allow-lists
    RUN_TEST(test_device_encode_get_status_with_renderer);
    RUN_TEST(test_device_encode_get_status_without_renderer);
    RUN_TEST(test_device_encode_get_info_with_effect_count);
    RUN_TEST(test_device_encode_get_info_without_effect_count);

    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD
