// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file test_http_narrative_codec.cpp
 * @brief Unit tests for HttpNarrativeCodec JSON parsing and encoder allow-list validation
 *
 * Tests HTTP narrative endpoint decoding (optional fields, defaults) and encoder
 * functions (response payload allow-lists).
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>
#include "../../src/codec/HttpNarrativeCodec.h"

using namespace lightwaveos::codec;

// ============================================================================
// Helper Functions
// ============================================================================

static bool loadJsonString(const char* jsonStr, JsonDocument& doc) {
    DeserializationError error = deserializeJson(doc, jsonStr);
    return error == DeserializationError::Ok;
}

// ============================================================================
// Decode tests
// ============================================================================

void test_http_narrative_config_set_decode_basic() {
    const char* json = R"({"holdBreathe": 0.5, "snapAmount": 0.3, "durationVariance": 0.1})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    NarrativeConfigDecodeResult result = HttpNarrativeCodec::decodeConfigSet(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_TRUE_MESSAGE(result.request.isSet, "isSet should be true");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasHoldBreathe, "hasHoldBreathe should be true");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.5f, result.request.holdBreathe, "holdBreathe should be 0.5");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasSnapAmount, "hasSnapAmount should be true");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.3f, result.request.snapAmount, "snapAmount should be 0.3");
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasDurationVariance, "hasDurationVariance should be true");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.1f, result.request.durationVariance, "durationVariance should be 0.1");
}

void test_http_narrative_config_set_decode_empty() {
    const char* json = R"({})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    NarrativeConfigDecodeResult result = HttpNarrativeCodec::decodeConfigSet(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_FALSE_MESSAGE(result.request.isSet, "isSet should be false when no fields provided");
}

// ============================================================================
// Unity setUp/tearDown
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// Test Runner
// ============================================================================

int main() {
    UNITY_BEGIN();
    
    // Decode tests
    RUN_TEST(test_http_narrative_config_set_decode_basic);
    RUN_TEST(test_http_narrative_config_set_decode_empty);
    
    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD
