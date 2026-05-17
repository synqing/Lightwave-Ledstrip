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
    data.ledTransportFrameCount = 300;
    data.ledTransportShowSkips = 1;
    data.ledTransportLastShowUs = 6100;
    data.ledTransportAvgShowUs = 6000;
    data.ledTransportMaxShowUs = 6400;
    data.ledTransportLastFastLedShowCallUs = 240;
    data.ledTransportAvgFastLedShowCallUs = 220;
    data.ledTransportLastRmtFenceUs = 5600;
    data.ledTransportAvgRmtFenceUs = 5590;
    data.ledTransportLastLatchWaitUs = 12;
    data.ledTransportAvgLatchWaitUs = 8;
    data.ledTransportFailures = 2;
    data.ledTransportRmtErrors = 3;
    data.ledTransportUnderruns = 4;

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    HttpDeviceCodec::encodeStatusExtended(data, obj);

    const char* allowedKeys[] = {
        "uptime", "freeHeap", "heapSize", "cpuFreq", "fps", "cpuPercent", "frameBudgetPercent",
        "framesRendered", "ledTransport", "network", "wsClients"
    };
    TEST_ASSERT_TRUE(validateKeysAgainstAllowList(obj, allowedKeys, sizeof(allowedKeys) / sizeof(allowedKeys[0])));
    TEST_ASSERT_EQUAL(55, obj["cpuPercent"].as<int>());
    TEST_ASSERT_EQUAL(55, obj["frameBudgetPercent"].as<int>());
    JsonObject ledTransport = obj["ledTransport"].as<JsonObject>();
    const char* ledTransportKeys[] = {
        "frameCount", "showSkips", "lastShowUs", "avgShowUs", "maxShowUs",
        "lastFastLedShowCallUs", "avgFastLedShowCallUs", "lastRmtFenceUs",
        "avgRmtFenceUs", "lastLatchWaitUs", "avgLatchWaitUs", "failures",
        "rmtErrors", "underruns"
    };
    TEST_ASSERT_TRUE(validateKeysAgainstAllowList(ledTransport, ledTransportKeys,
                                                  sizeof(ledTransportKeys) / sizeof(ledTransportKeys[0])));
    TEST_ASSERT_EQUAL_UINT32(300, ledTransport["frameCount"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(1, ledTransport["showSkips"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(240, ledTransport["lastFastLedShowCallUs"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(5600, ledTransport["lastRmtFenceUs"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(12, ledTransport["lastLatchWaitUs"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(2, ledTransport["failures"].as<uint32_t>());
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
