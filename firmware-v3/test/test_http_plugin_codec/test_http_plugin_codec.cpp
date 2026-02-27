// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file test_http_plugin_codec.cpp
 * @brief Unit tests for HttpPluginCodec JSON encoder allow-list validation
 *
 * Tests HTTP plugin endpoint encoding allow-lists.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>
#include "../../src/codec/HttpPluginCodec.h"

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

void test_plugin_stats_allowlist() {
    HttpPluginStatsData stats;
    stats.registeredCount = 3;
    stats.loadedFromLittleFS = true;
    stats.overrideModeEnabled = false;
    stats.disabledByOverride = false;
    stats.registrationsFailed = 1;
    stats.unregistrations = 2;
    stats.lastReloadOk = true;
    stats.lastReloadMillis = 1000;
    stats.manifestCount = 2;
    stats.errorCount = 0;
    stats.lastErrorSummary = "";

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    HttpPluginCodec::encodeStats(stats, obj);

    const char* allowedKeys[] = {
        "registeredCount", "loadedFromLittleFS", "overrideModeEnabled",
        "disabledByOverride", "registrationsFailed", "unregistrations",
        "lastReloadOk", "lastReloadMillis", "manifestCount", "errorCount"
    };
    TEST_ASSERT_TRUE(validateKeysAgainstAllowList(obj, allowedKeys, sizeof(allowedKeys) / sizeof(allowedKeys[0])));
}

void test_plugin_manifests_allowlist() {
    HttpPluginManifestItemData manifests[1];
    manifests[0].file = "plugin.json";
    manifests[0].valid = true;
    manifests[0].name = "Test";
    manifests[0].mode = "additive";
    manifests[0].effectCount = 3;

    HttpPluginManifestsData data;
    data.count = 1;
    data.manifests = manifests;
    data.manifestCount = 1;

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    HttpPluginCodec::encodeManifests(data, obj);

    const char* allowedKeys[] = {"count", "files"};
    TEST_ASSERT_TRUE(validateKeysAgainstAllowList(obj, allowedKeys, sizeof(allowedKeys) / sizeof(allowedKeys[0])));
    TEST_ASSERT_TRUE(obj.containsKey("files"));
}

void test_plugin_reload_allowlist() {
    HttpPluginReloadData data;
    data.reloadSuccess = true;
    data.stats.registeredCount = 1;
    data.stats.loadedFromLittleFS = true;
    data.stats.overrideModeEnabled = false;
    data.stats.disabledByOverride = false;
    data.stats.lastReloadOk = true;
    data.stats.lastReloadMillis = 100;
    data.stats.manifestCount = 1;
    data.stats.errorCount = 0;

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    HttpPluginCodec::encodeReload(data, obj);

    const char* allowedKeys[] = {"reloadSuccess", "stats", "errors"};
    TEST_ASSERT_TRUE(validateKeysAgainstAllowList(obj, allowedKeys, sizeof(allowedKeys) / sizeof(allowedKeys[0])));
}

void setUp(void) {}
void tearDown(void) {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_plugin_stats_allowlist);
    RUN_TEST(test_plugin_manifests_allowlist);
    RUN_TEST(test_plugin_reload_allowlist);
    return UNITY_END();
}

#endif
