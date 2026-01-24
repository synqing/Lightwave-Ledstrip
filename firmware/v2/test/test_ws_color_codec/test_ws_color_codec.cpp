/**
 * @file test_ws_color_codec.cpp
 * @brief Unit tests for WsColorCodec JSON parsing and validation
 *
 * Tests color WebSocket command decoding with type checking, unknown-key rejection,
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
#include "../../src/codec/WsColorCodec.h"

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

void test_color_simple_valid() {
    const char* json = R"({"requestId": "test123"})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    ColorSimpleDecodeResult result = WsColorCodec::decodeSimple(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("test123", result.request.requestId);
}

void test_color_simple_valid_no_requestId() {
    const char* json = R"({})";
    
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    ColorSimpleDecodeResult result = WsColorCodec::decodeSimple(root);
    
    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("", result.request.requestId);
}

// ============================================================================
// Test: Encoder Functions (Response Encoding)
// ============================================================================

void test_encode_get_status() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsColorCodec::encodeGetStatus(true, true, 100, 150, 200, false, 2.5f, 0.0f, true, 128, data);

    TEST_ASSERT_TRUE_MESSAGE(data["active"].as<bool>(), "active should be true");
    TEST_ASSERT_TRUE_MESSAGE(data["blendEnabled"].as<bool>(), "blendEnabled should be true");
    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("blendFactors"), "blendFactors array should be present");
    TEST_ASSERT_FALSE_MESSAGE(data["rotationEnabled"].as<bool>(), "rotationEnabled should be false");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 2.5f, data["rotationSpeed"].as<float>(), "rotationSpeed should be 2.5");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.0f, data["rotationPhase"].as<float>(), "rotationPhase should be 0.0");
    TEST_ASSERT_TRUE_MESSAGE(data["diffusionEnabled"].as<bool>(), "diffusionEnabled should be true");
    TEST_ASSERT_EQUAL_INT_MESSAGE(128, data["diffusionAmount"].as<uint8_t>(), "diffusionAmount should be 128");
    
    JsonArray blendFactors = data["blendFactors"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, blendFactors.size(), "blendFactors should have 3 entries");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, blendFactors[0].as<uint8_t>(), "first factor should be 100");
    TEST_ASSERT_EQUAL_INT_MESSAGE(150, blendFactors[1].as<uint8_t>(), "second factor should be 150");
    TEST_ASSERT_EQUAL_INT_MESSAGE(200, blendFactors[2].as<uint8_t>(), "third factor should be 200");
    
    const char* allowedKeys[] = {"active", "blendEnabled", "blendFactors", "rotationEnabled", "rotationSpeed", "rotationPhase", "diffusionEnabled", "diffusionAmount"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 8),
        "Should only have required keys, no extras allowed");
}

void test_encode_enable_blend() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsColorCodec::encodeEnableBlend(true, data);

    TEST_ASSERT_TRUE_MESSAGE(data["blendEnabled"].as<bool>(), "blendEnabled should be true");
    
    const char* allowedKeys[] = {"blendEnabled"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 1),
        "Should only have blendEnabled key, no extras allowed");
}

void test_encode_set_blend_palettes() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    uint8_t palettes2[] = {5, 10};
    WsColorCodec::encodeSetBlendPalettes(palettes2, 2, data);

    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("blendPalettes"), "blendPalettes array should be present");
    JsonArray palettes = data["blendPalettes"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, palettes.size(), "blendPalettes should have 2 entries");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, palettes[0].as<uint8_t>(), "first palette should be 5");
    TEST_ASSERT_EQUAL_INT_MESSAGE(10, palettes[1].as<uint8_t>(), "second palette should be 10");
    
    const char* allowedKeys[] = {"blendPalettes"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 1),
        "Should only have blendPalettes key, no extras allowed");
}

void test_encode_set_blend_palettes_three() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    uint8_t palettes3[] = {5, 10, 15};
    WsColorCodec::encodeSetBlendPalettes(palettes3, 3, data);

    JsonArray palettes = data["blendPalettes"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, palettes.size(), "blendPalettes should have 3 entries");
    TEST_ASSERT_EQUAL_INT_MESSAGE(15, palettes[2].as<uint8_t>(), "third palette should be 15");
}

void test_encode_set_blend_factors() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsColorCodec::encodeSetBlendFactors(100, 150, 200, data);

    TEST_ASSERT_TRUE_MESSAGE(data.containsKey("blendFactors"), "blendFactors array should be present");
    JsonArray factors = data["blendFactors"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, factors.size(), "blendFactors should have 3 entries");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, factors[0].as<uint8_t>(), "first factor should be 100");
    TEST_ASSERT_EQUAL_INT_MESSAGE(150, factors[1].as<uint8_t>(), "second factor should be 150");
    TEST_ASSERT_EQUAL_INT_MESSAGE(200, factors[2].as<uint8_t>(), "third factor should be 200");
    
    const char* allowedKeys[] = {"blendFactors"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 1),
        "Should only have blendFactors key, no extras allowed");
}

void test_encode_enable_rotation() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsColorCodec::encodeEnableRotation(true, data);

    TEST_ASSERT_TRUE_MESSAGE(data["rotationEnabled"].as<bool>(), "rotationEnabled should be true");
    
    const char* allowedKeys[] = {"rotationEnabled"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 1),
        "Should only have rotationEnabled key, no extras allowed");
}

void test_encode_set_rotation_speed() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsColorCodec::encodeSetRotationSpeed(2.5f, data);

    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 2.5f, data["rotationSpeed"].as<float>(), "rotationSpeed should be 2.5");
    
    const char* allowedKeys[] = {"rotationSpeed"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 1),
        "Should only have rotationSpeed key, no extras allowed");
}

void test_encode_enable_diffusion() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsColorCodec::encodeEnableDiffusion(true, data);

    TEST_ASSERT_TRUE_MESSAGE(data["diffusionEnabled"].as<bool>(), "diffusionEnabled should be true");
    
    const char* allowedKeys[] = {"diffusionEnabled"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 1),
        "Should only have diffusionEnabled key, no extras allowed");
}

void test_encode_set_diffusion_amount() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsColorCodec::encodeSetDiffusionAmount(128, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(128, data["diffusionAmount"].as<uint8_t>(), "diffusionAmount should be 128");
    
    const char* allowedKeys[] = {"diffusionAmount"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 1),
        "Should only have diffusionAmount key, no extras allowed");
}

void test_encode_correction_get_config() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsColorCodec::encodeCorrectionGetConfig(2, "OFF,HSV,RGB,BOTH", 120, 150, 100, true, 110, true, 2.2f, false, 28, 8, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(2, data["mode"].as<uint8_t>(), "mode should be 2");
    TEST_ASSERT_EQUAL_STRING("OFF,HSV,RGB,BOTH", data["modeNames"].as<const char*>());
    TEST_ASSERT_EQUAL_INT_MESSAGE(120, data["hsvMinSaturation"].as<uint8_t>(), "hsvMinSaturation should be 120");
    TEST_ASSERT_EQUAL_INT_MESSAGE(150, data["rgbWhiteThreshold"].as<uint8_t>(), "rgbWhiteThreshold should be 150");
    TEST_ASSERT_EQUAL_INT_MESSAGE(100, data["rgbTargetMin"].as<uint8_t>(), "rgbTargetMin should be 100");
    TEST_ASSERT_TRUE_MESSAGE(data["autoExposureEnabled"].as<bool>(), "autoExposureEnabled should be true");
    TEST_ASSERT_EQUAL_INT_MESSAGE(110, data["autoExposureTarget"].as<uint8_t>(), "autoExposureTarget should be 110");
    TEST_ASSERT_TRUE_MESSAGE(data["gammaEnabled"].as<bool>(), "gammaEnabled should be true");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 2.2f, data["gammaValue"].as<float>(), "gammaValue should be 2.2");
    TEST_ASSERT_FALSE_MESSAGE(data["brownGuardrailEnabled"].as<bool>(), "brownGuardrailEnabled should be false");
    TEST_ASSERT_EQUAL_INT_MESSAGE(28, data["maxGreenPercentOfRed"].as<uint8_t>(), "maxGreenPercentOfRed should be 28");
    TEST_ASSERT_EQUAL_INT_MESSAGE(8, data["maxBluePercentOfRed"].as<uint8_t>(), "maxBluePercentOfRed should be 8");
    
    const char* allowedKeys[] = {"mode", "modeNames", "hsvMinSaturation", "rgbWhiteThreshold", "rgbTargetMin", "autoExposureEnabled", "autoExposureTarget", "gammaEnabled", "gammaValue", "brownGuardrailEnabled", "maxGreenPercentOfRed", "maxBluePercentOfRed"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 12),
        "Should only have required keys, no extras allowed");
}

void test_encode_correction_set_mode() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsColorCodec::encodeCorrectionSetMode(2, "RGB", data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(2, data["mode"].as<uint8_t>(), "mode should be 2");
    TEST_ASSERT_EQUAL_STRING("RGB", data["modeName"].as<const char*>());
    
    const char* allowedKeys[] = {"mode", "modeName"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 2),
        "Should only have mode and modeName keys, no extras allowed");
}

void test_encode_correction_set_config() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsColorCodec::encodeCorrectionSetConfig(3, true, data);

    TEST_ASSERT_EQUAL_INT_MESSAGE(3, data["mode"].as<uint8_t>(), "mode should be 3");
    TEST_ASSERT_TRUE_MESSAGE(data["updated"].as<bool>(), "updated should be true");
    
    const char* allowedKeys[] = {"mode", "updated"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 2),
        "Should only have mode and updated keys, no extras allowed");
}

void test_encode_correction_save() {
    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();

    WsColorCodec::encodeCorrectionSave(true, data);

    TEST_ASSERT_TRUE_MESSAGE(data["saved"].as<bool>(), "saved should be true");
    
    const char* allowedKeys[] = {"saved"};
    TEST_ASSERT_TRUE_MESSAGE(
        validateKeysAgainstAllowList(data, allowedKeys, 1),
        "Should only have saved key, no extras allowed");
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
    RUN_TEST(test_color_simple_valid);
    RUN_TEST(test_color_simple_valid_no_requestId);

    // Encoder tests (response payloads)
    RUN_TEST(test_encode_get_status);
    RUN_TEST(test_encode_enable_blend);
    RUN_TEST(test_encode_set_blend_palettes);
    RUN_TEST(test_encode_set_blend_palettes_three);
    RUN_TEST(test_encode_set_blend_factors);
    RUN_TEST(test_encode_enable_rotation);
    RUN_TEST(test_encode_set_rotation_speed);
    RUN_TEST(test_encode_enable_diffusion);
    RUN_TEST(test_encode_set_diffusion_amount);
    RUN_TEST(test_encode_correction_get_config);
    RUN_TEST(test_encode_correction_set_mode);
    RUN_TEST(test_encode_correction_set_config);
    RUN_TEST(test_encode_correction_save);

    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD
