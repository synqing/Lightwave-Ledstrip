/**
 * @file test_ws_effects_codec.cpp
 * @brief Unit tests for WsEffectsCodec JSON parsing and validation
 *
 * Tests effects.setCurrent command decoding with validation, type checking,
 * range validation, and error handling.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>
#include "../../src/codec/WsEffectsCodec.h"

using namespace lightwaveos::codec;

// ============================================================================
// Unity setUp/tearDown (required by Unity framework)
// ============================================================================

void setUp(void) {
    // Called before each test
}

void tearDown(void) {
    // Called after each test
}

// ============================================================================
// Test 1: Valid request with transition
// ============================================================================

void test_effects_set_current_valid_with_transition() {
    StaticJsonDocument<256> doc;
    doc["effectId"] = 42;
    doc["requestId"] = "test-123";
    doc["transition"]["type"] = 1;
    doc["transition"]["duration"] = 2000;
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    EffectsSetCurrentDecodeResult result = WsEffectsCodec::decodeSetCurrent(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(42, result.request.effectId, "effectId should be 42");
    TEST_ASSERT_EQUAL_STRING("test-123", result.request.requestId);
    TEST_ASSERT_TRUE_MESSAGE(result.request.hasTransition, "hasTransition should be true");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, result.request.transitionType, "transitionType should be 1");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2000, result.request.transitionDuration, "transitionDuration should be 2000");
}

// ============================================================================
// Test 2: Missing required field (effectId)
// ============================================================================

void test_effects_set_current_missing_required() {
    StaticJsonDocument<128> doc;
    doc["requestId"] = "test-456";
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    EffectsSetCurrentDecodeResult result = WsEffectsCodec::decodeSetCurrent(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(result.errorMsg, "effectId"), "Error should mention effectId");
}

// ============================================================================
// Test 3: Wrong type for required field
// ============================================================================

void test_effects_set_current_wrong_type() {
    StaticJsonDocument<128> doc;
    doc["effectId"] = "not-a-number";  // Should be int
    doc["requestId"] = "test-789";
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    EffectsSetCurrentDecodeResult result = WsEffectsCodec::decodeSetCurrent(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(result.errorMsg, "effectId"), "Error should mention effectId");
}

// ============================================================================
// Test 4: Out-of-range effectId
// ============================================================================

void test_effects_set_current_out_of_range() {
    StaticJsonDocument<128> doc;
    doc["effectId"] = 255;  // Max is 127
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    EffectsSetCurrentDecodeResult result = WsEffectsCodec::decodeSetCurrent(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(result.errorMsg, "range"), "Error should mention range");
}

// ============================================================================
// Test 5: Valid without transition (minimal)
// ============================================================================

void test_effects_set_current_minimal() {
    StaticJsonDocument<64> doc;
    doc["effectId"] = 5;
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    EffectsSetCurrentDecodeResult result = WsEffectsCodec::decodeSetCurrent(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, result.request.effectId, "effectId should be 5");
    TEST_ASSERT_FALSE_MESSAGE(result.request.hasTransition, "hasTransition should be false");
}

// ============================================================================
// Test Main
// ============================================================================

int main() {
    UNITY_BEGIN();
    
    RUN_TEST(test_effects_set_current_valid_with_transition);
    RUN_TEST(test_effects_set_current_missing_required);
    RUN_TEST(test_effects_set_current_wrong_type);
    RUN_TEST(test_effects_set_current_out_of_range);
    RUN_TEST(test_effects_set_current_minimal);
    
    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD
