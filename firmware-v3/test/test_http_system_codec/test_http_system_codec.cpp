/**
 * @file test_http_system_codec.cpp
 * @brief Unit tests for HttpSystemCodec JSON encoder allow-list validation
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>
#include "../../src/codec/HttpSystemCodec.h"

using namespace lightwaveos::codec;

static bool validateKeysAgainstAllowList(const JsonObject& obj,
                                         const char* allowedKeys[],
                                         size_t allowedCount) {
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

void test_system_health_renderer_budget_alias_preserves_legacy_cpu_percent() {
    HttpSystemHealthData data;
    data.uptime = 42;
    data.freeHeap = 1000;
    data.totalHeap = 2000;
    data.minFreeHeap = 900;
    data.hasRenderer = true;
    data.rendererRunning = true;
    data.queueUtilization = 0.25f;
    data.queueLength = 8;
    data.queueCapacity = 32;
    data.fps = 120.0f;
    data.cpuPercent = 55.0f;
    data.hasWebSocket = true;
    data.wsClients = 2;
    data.wsMaxClients = 8;
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
    HttpSystemCodec::encodeHealth(data, obj);

    const char* allowedKeys[] = {
        "uptime", "freeHeap", "totalHeap", "minFreeHeap", "rendererRunning",
        "queueUtilization", "queueLength", "queueCapacity", "fps", "cpuPercent",
        "frameBudgetPercent", "ledTransport", "wsClients", "wsMaxClients"
    };
    TEST_ASSERT_TRUE(validateKeysAgainstAllowList(obj, allowedKeys,
                                                  sizeof(allowedKeys) / sizeof(allowedKeys[0])));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 55.0f, obj["cpuPercent"].as<float>());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 55.0f, obj["frameBudgetPercent"].as<float>());
    JsonObject ledTransport = obj["ledTransport"].as<JsonObject>();
    TEST_ASSERT_EQUAL_UINT32(300, ledTransport["frameCount"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(1, ledTransport["showSkips"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(240, ledTransport["lastFastLedShowCallUs"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(5600, ledTransport["lastRmtFenceUs"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(12, ledTransport["lastLatchWaitUs"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(2, ledTransport["failures"].as<uint32_t>());
}

void setUp(void) {}
void tearDown(void) {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_system_health_renderer_budget_alias_preserves_legacy_cpu_percent);
    return UNITY_END();
}

#endif
