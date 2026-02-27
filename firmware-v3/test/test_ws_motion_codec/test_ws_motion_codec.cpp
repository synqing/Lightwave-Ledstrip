// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file test_ws_motion_codec.cpp
 * @brief Unit tests for WsMotionCodec JSON parsing and validation
 *
 * Tests motion WebSocket command decoding with type checking, unknown-key rejection,
 * and encoder allow-list validation.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <ArduinoJson.h>
#include <fstream>
#include <string>
#include <cstring>
#include "../../src/codec/WsMotionCodec.h"

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
 * @brief Validate keys against allow-list (matches decode test pattern)
 * 
 * Verifies:
 * - All keys in object are in the allow-list (no unknown keys)
 * - All required keys from allow-list are present
 * 
 * @param obj JSON object to validate
 * @param allowedKeys Array of allowed key names
 * @param allowedCount Number of allowed keys
 * @return true if all keys are allowed and all required keys are present
 */
static bool validateKeysAgainstAllowList(const JsonObject& obj, const char* allowedKeys[], size_t allowedCount) {
    size_t foundCount = 0;
    // Check that all keys in object are in allow-list
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
            return false;  // Unknown key found
        }
    }
    // Verify all required keys are present
    for (size_t i = 0; i < allowedCount; i++) {
        if (!obj.containsKey(allowedKeys[i])) {
            return false;  // Required key missing
        }
    }
    return foundCount == allowedCount;  // No extra, no missing
}

// ============================================================================
// Test: Valid Simple Request (requestId only)
// ============================================================================

void test_motion_simple_valid() {
    const char* json = R"({"requestId": "test123"})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    MotionSimpleDecodeResult result = WsMotionCodec::decodeSimple(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("test123", result.request.requestId);
}

void test_motion_simple_valid_no_requestId() {
    const char* json = R"({})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    MotionSimpleDecodeResult result = WsMotionCodec::decodeSimple(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("", result.request.requestId);
}

// ============================================================================
// Test: Decode Functions
// ============================================================================

void test_decode_phase_set_offset_valid() {
    const char* json = R"({"degrees": 45.5, "requestId": "test"})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    MotionPhaseSetOffsetDecodeResult result = WsMotionCodec::decodePhaseSetOffset(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 45.5f, result.request.degreesValue, "degrees should be 45.5");
    TEST_ASSERT_EQUAL_STRING("test", result.request.requestId);
}

void test_decode_phase_set_offset_invalid_range() {
    const char* json = R"({"degrees": 400.0})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    MotionPhaseSetOffsetDecodeResult result = WsMotionCodec::decodePhaseSetOffset(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result.errorMsg, "out of range") != nullptr, "Error should mention range");
}

void test_decode_phase_enable_auto_rotate_valid() {
    const char* json = R"({"degreesPerSecond": 30.5})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    MotionPhaseEnableAutoRotateDecodeResult result = WsMotionCodec::decodePhaseEnableAutoRotate(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 30.5f, result.request.degreesPerSecond, "degreesPerSecond should be 30.5");
}

void test_decode_speed_set_modulation_valid() {
    const char* json = R"({"type": "SINE_WAVE", "depth": 0.75})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    MotionSpeedSetModulationDecodeResult result = WsMotionCodec::decodeSpeedSetModulation(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(MotionModType::SINE_WAVE), static_cast<int>(result.request.modType), "modType should be SINE_WAVE");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.75f, result.request.depth, "depth should be 0.75");
}

void test_decode_speed_set_modulation_invalid_type() {
    const char* json = R"({"type": "INVALID", "depth": 0.5})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    MotionSpeedSetModulationDecodeResult result = WsMotionCodec::decodeSpeedSetModulation(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result.errorMsg, "Invalid type") != nullptr, "Error should mention invalid type");
}

void test_decode_momentum_add_particle_valid() {
    const char* json = R"({"position": 0.3, "velocity": 0.1, "mass": 2.0, "boundary": "BOUNCE"})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    MotionMomentumAddParticleDecodeResult result = WsMotionCodec::decodeMomentumAddParticle(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.3f, result.request.position, "position should be 0.3");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.1f, result.request.velocity, "velocity should be 0.1");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 2.0f, result.request.mass, "mass should be 2.0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(MotionBoundary::BOUNCE), static_cast<int>(result.request.boundary), "boundary should be BOUNCE");
}

void test_decode_momentum_apply_force_valid() {
    const char* json = R"({"particleId": 5, "force": 10.5})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    MotionMomentumApplyForceDecodeResult result = WsMotionCodec::decodeMomentumApplyForce(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, result.request.particleId, "particleId should be 5");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 10.5f, result.request.force, "force should be 10.5");
}

void test_decode_momentum_apply_force_invalid_range() {
    const char* json = R"({"particleId": 50})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    MotionMomentumApplyForceDecodeResult result = WsMotionCodec::decodeMomentumApplyForce(root);
    
    TEST_ASSERT_FALSE_MESSAGE(result.success, "Decode should fail");
    TEST_ASSERT_TRUE_MESSAGE(strstr(result.errorMsg, "out of range") != nullptr, "Error should mention range");
}

// ============================================================================
// Test: Encoder Functions (Response Encoding)
// ============================================================================

void test_encode_get_status() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsMotionCodec::encodeGetStatus(true, 45.0f, 30.5f, 25.0f, data);

    TEST_ASSERT_TRUE_MESSAGE(data["enabled"].as<bool>(), "enabled should be true");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 45.0f, data["phaseOffset"].as<float>(), "phaseOffset should be 45.0");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 30.5f, data["autoRotateSpeed"].as<float>(), "autoRotateSpeed should be 30.5");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 25.0f, data["baseSpeed"].as<float>(), "baseSpeed should be 25.0");
    
    const char* allowedKeys[] = {"enabled", "phaseOffset", "autoRotateSpeed", "baseSpeed"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 4),
        "Should only have required keys, no extras allowed");
}

void test_encode_enabled() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsMotionCodec::encodeEnabled(true, data);

    TEST_ASSERT_TRUE_MESSAGE(data["enabled"].as<bool>(), "enabled should be true");
    
    const char* allowedKeys[] = {"enabled"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 1),
        "Should only have enabled key, no extras allowed");
}

void test_encode_phase_set_offset() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsMotionCodec::encodePhaseSetOffset(90.0f, data);

    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 90.0f, data["degrees"].as<float>(), "degrees should be 90.0");
    
    const char* allowedKeys[] = {"degrees"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 1),
        "Should only have degrees key, no extras allowed");
}

void test_encode_phase_enable_auto_rotate() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsMotionCodec::encodePhaseEnableAutoRotate(45.0f, true, data);

    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 45.0f, data["degreesPerSecond"].as<float>(), "degreesPerSecond should be 45.0");
    TEST_ASSERT_TRUE_MESSAGE(data["autoRotate"].as<bool>(), "autoRotate should be true");
    
    const char* allowedKeys[] = {"degreesPerSecond", "autoRotate"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 2),
        "Should only have required keys, no extras allowed");
}

void test_encode_phase_get_phase() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsMotionCodec::encodePhaseGetPhase(90.0f, 1.5708f, data);

    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 90.0f, data["degrees"].as<float>(), "degrees should be 90.0");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 1.5708f, data["radians"].as<float>(), "radians should be 1.5708");
    
    const char* allowedKeys[] = {"degrees", "radians"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 2),
        "Should only have degrees and radians keys, no extras allowed");
}

void test_encode_speed_set_modulation() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsMotionCodec::encodeSpeedSetModulation("SINE_WAVE", 0.75f, data);

    TEST_ASSERT_EQUAL_STRING("SINE_WAVE", data["type"].as<const char*>());
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.75f, data["depth"].as<float>(), "depth should be 0.75");
    
    const char* allowedKeys[] = {"type", "depth"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 2),
        "Should only have type and depth keys, no extras allowed");
}

void test_encode_momentum_get_status() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsMotionCodec::encodeMomentumGetStatus(5, 32, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(5, data["activeCount"].as<uint8_t>(), "activeCount should be 5");
    TEST_ASSERT_EQUAL_INT_MESSAGE(32, data["maxParticles"].as<uint8_t>(), "maxParticles should be 32");
    
    const char* allowedKeys[] = {"activeCount", "maxParticles"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 2),
        "Should only have activeCount and maxParticles keys, no extras allowed");
}

void test_encode_momentum_add_particle() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsMotionCodec::encodeMomentumAddParticle(10, true, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(10, data["particleId"].as<int>(), "particleId should be 10");
    TEST_ASSERT_TRUE_MESSAGE(data["success"].as<bool>(), "success should be true");
    
    const char* allowedKeys[] = {"particleId", "success"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 2),
        "Should only have particleId and success keys, no extras allowed");
}

void test_encode_momentum_get_particle() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsMotionCodec::encodeMomentumGetParticle(5, 0.5f, 0.1f, 2.0f, true, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(5, data["particleId"].as<int>(), "particleId should be 5");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.5f, data["position"].as<float>(), "position should be 0.5");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.1f, data["velocity"].as<float>(), "velocity should be 0.1");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 2.0f, data["mass"].as<float>(), "mass should be 2.0");
    TEST_ASSERT_TRUE_MESSAGE(data["alive"].as<bool>(), "alive should be true");
    
    const char* allowedKeys[] = {"particleId", "position", "velocity", "mass", "alive"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 5),
        "Should only have required keys, no extras allowed");
}

void test_encode_momentum_reset() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsMotionCodec::encodeMomentumReset("All particles cleared", 0, data);

    TEST_ASSERT_EQUAL_STRING("All particles cleared", data["message"].as<const char*>());
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, data["activeCount"].as<uint8_t>(), "activeCount should be 0");
    
    const char* allowedKeys[] = {"message", "activeCount"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 2),
        "Should only have message and activeCount keys, no extras allowed");
}

void test_encode_momentum_update() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsMotionCodec::encodeMomentumUpdate(0.016f, 3, true, data);

    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.016f, data["deltaTime"].as<float>(), "deltaTime should be 0.016");
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, data["activeCount"].as<uint8_t>(), "activeCount should be 3");
    TEST_ASSERT_TRUE_MESSAGE(data["updated"].as<bool>(), "updated should be true");
    
    const char* allowedKeys[] = {"deltaTime", "activeCount", "updated"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 3),
        "Should only have required keys, no extras allowed");
}

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
// Test Runner
// ============================================================================

int main() {
    UNITY_BEGIN();

    // Valid payloads
    RUN_TEST(test_motion_simple_valid);
    RUN_TEST(test_motion_simple_valid_no_requestId);

    // Decode tests
    RUN_TEST(test_decode_phase_set_offset_valid);
    RUN_TEST(test_decode_phase_set_offset_invalid_range);
    RUN_TEST(test_decode_phase_enable_auto_rotate_valid);
    RUN_TEST(test_decode_speed_set_modulation_valid);
    RUN_TEST(test_decode_speed_set_modulation_invalid_type);
    RUN_TEST(test_decode_momentum_add_particle_valid);
    RUN_TEST(test_decode_momentum_apply_force_valid);
    RUN_TEST(test_decode_momentum_apply_force_invalid_range);

    // Encoder tests (response payloads)
    RUN_TEST(test_encode_get_status);
    RUN_TEST(test_encode_enabled);
    RUN_TEST(test_encode_phase_set_offset);
    RUN_TEST(test_encode_phase_enable_auto_rotate);
    RUN_TEST(test_encode_phase_get_phase);
    RUN_TEST(test_encode_speed_set_modulation);
    RUN_TEST(test_encode_momentum_get_status);
    RUN_TEST(test_encode_momentum_add_particle);
    RUN_TEST(test_encode_momentum_get_particle);
    RUN_TEST(test_encode_momentum_reset);
    RUN_TEST(test_encode_momentum_update);

    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD
