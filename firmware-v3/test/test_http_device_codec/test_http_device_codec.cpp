/**
 * @file test_http_device_codec.cpp
 * @brief Unit tests for HttpDeviceCodec JSON encoder allow-list validation
 *
 * Tests HTTP device endpoint encoding (response payload allow-lists).
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>
#include "../../src/codec/HttpDeviceCodec.h"

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

void test_device_status_extended_encoding_allowlist() {
    HttpDeviceStatusExtendedData data;
    data.uptime = 42;
    data.freeHeap = 1000;
    data.heapSize = 2000;
    data.cpuFreq = 240;
    data.fps = 120;
    data.cpuPercent = 55;
    data.framesRendered = 12345;
    data.networkConnected = true;
    data.apMode = false;
    data.networkIP = "192.168.1.100";
    data.networkRSSI = -42;
    data.wsClients = 2;

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    HttpDeviceCodec::encodeStatusExtended(data, obj);

    const char* allowedKeys[] = {
        "uptime", "freeHeap", "heapSize", "cpuFreq", "fps", "cpuPercent",
        "framesRendered", "network", "wsClients"
    };
    TEST_ASSERT_TRUE(validateKeysAgainstAllowList(obj, allowedKeys, sizeof(allowedKeys) / sizeof(allowedKeys[0])));
    TEST_ASSERT_TRUE(obj.containsKey("network"));

    JsonObject network = obj["network"].as<JsonObject>();
    const char* networkKeys[] = {"connected", "apMode", "ip", "rssi"};
    TEST_ASSERT_TRUE(validateKeysAgainstAllowList(network, networkKeys, sizeof(networkKeys) / sizeof(networkKeys[0])));
}

void test_device_info_encoding_allowlist() {
    HttpDeviceInfoData data;
    data.firmware = "2.0.0";
    data.board = "ESP32-S3-DevKitC-1";
    data.sdk = "SDK";
    data.flashSize = 1024;
    data.sketchSize = 2048;
    data.freeSketch = 512;
    data.architecture = "Actor System v2";

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    HttpDeviceCodec::encodeInfo(data, obj);

    const char* allowedKeys[] = {
        "firmware", "board", "sdk", "flashSize", "sketchSize", "freeSketch", "architecture"
    };
    TEST_ASSERT_TRUE(validateKeysAgainstAllowList(obj, allowedKeys, sizeof(allowedKeys) / sizeof(allowedKeys[0])));
}

void setUp(void) {}
void tearDown(void) {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_device_status_extended_encoding_allowlist);
    RUN_TEST(test_device_info_encoding_allowlist);
    return UNITY_END();
}

#endif
