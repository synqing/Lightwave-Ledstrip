/**
 * @file test_ws_color_codec.cpp
 * @brief Unit tests for WsColorCodec decode behaviour
 *
 * Covers required-field validation, defaults, and range checks for current
 * WebSocket colour decode APIs.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>
#include "../../src/codec/WsColorCodec.h"

using namespace lightwaveos::codec;

// ============================================================================
// Helpers
// ============================================================================

static bool loadJsonString(const char* jsonStr, JsonDocument& doc) {
    DeserializationError error = deserializeJson(doc, jsonStr);
    return error == DeserializationError::Ok;
}

// ============================================================================
// Group B: Simple setters
// ============================================================================

void test_decode_enable_blend_valid() {
    const char* json = R"({"requestId":"r1","enable":true})";
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(json, doc));

    ColorEnableBlendDecodeResult result = WsColorCodec::decodeEnableBlend(doc.as<JsonObjectConst>());
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_TRUE(result.request.enable);
    TEST_ASSERT_EQUAL_STRING("r1", result.request.requestId);
}

void test_decode_enable_blend_missing_enable() {
    const char* json = R"({"requestId":"r1"})";
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(json, doc));

    ColorEnableBlendDecodeResult result = WsColorCodec::decodeEnableBlend(doc.as<JsonObjectConst>());
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(strlen(result.errorMsg) > 0);
}

void test_decode_enable_rotation_valid() {
    const char* json = R"({"enable":false})";
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(json, doc));

    ColorEnableRotationDecodeResult result = WsColorCodec::decodeEnableRotation(doc.as<JsonObjectConst>());
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_FALSE(result.request.enable);
}

void test_decode_enable_diffusion_valid() {
    const char* json = R"({"enable":true})";
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(json, doc));

    ColorEnableDiffusionDecodeResult result = WsColorCodec::decodeEnableDiffusion(doc.as<JsonObjectConst>());
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_TRUE(result.request.enable);
}

void test_decode_set_diffusion_amount_valid() {
    const char* json = R"({"requestId":"r2","amount":128})";
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(json, doc));

    ColorSetDiffusionAmountDecodeResult result = WsColorCodec::decodeSetDiffusionAmount(doc.as<JsonObjectConst>());
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_UINT8(128, result.request.amount);
    TEST_ASSERT_EQUAL_STRING("r2", result.request.requestId);
}

void test_decode_set_diffusion_amount_out_of_range() {
    const char* json = R"({"amount":256})";
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(json, doc));

    ColorSetDiffusionAmountDecodeResult result = WsColorCodec::decodeSetDiffusionAmount(doc.as<JsonObjectConst>());
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(strlen(result.errorMsg) > 0);
}

void test_decode_set_mode_valid() {
    const char* json = R"({"mode":2})";
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(json, doc));

    ColorCorrectionSetModeDecodeResult result = WsColorCodec::decodeSetMode(doc.as<JsonObjectConst>());
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_UINT8(2, result.request.mode);
}

void test_decode_set_mode_out_of_range() {
    const char* json = R"({"mode":4})";
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(json, doc));

    ColorCorrectionSetModeDecodeResult result = WsColorCodec::decodeSetMode(doc.as<JsonObjectConst>());
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(strlen(result.errorMsg) > 0);
}

void test_decode_set_rotation_speed_valid() {
    const char* json = R"({"degreesPerFrame":2.5})";
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(json, doc));

    ColorSetRotationSpeedDecodeResult result = WsColorCodec::decodeSetRotationSpeed(doc.as<JsonObjectConst>());
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, result.request.degreesPerFrame);
}

void test_decode_set_rotation_speed_missing() {
    const char* json = R"({"requestId":"r3"})";
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(json, doc));

    ColorSetRotationSpeedDecodeResult result = WsColorCodec::decodeSetRotationSpeed(doc.as<JsonObjectConst>());
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(strlen(result.errorMsg) > 0);
}

// ============================================================================
// Group C: Multi-field
// ============================================================================

void test_decode_set_blend_palettes_valid_defaults_palette3() {
    const char* json = R"({"palette1":5,"palette2":10})";
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(json, doc));

    ColorSetBlendPalettesDecodeResult result = WsColorCodec::decodeSetBlendPalettes(doc.as<JsonObjectConst>());
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_UINT8(5, result.request.palette1);
    TEST_ASSERT_EQUAL_UINT8(10, result.request.palette2);
    TEST_ASSERT_EQUAL_UINT8(255, result.request.palette3);
}

void test_decode_set_blend_palettes_valid_with_palette3() {
    const char* json = R"({"palette1":5,"palette2":10,"palette3":15})";
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(json, doc));

    ColorSetBlendPalettesDecodeResult result = WsColorCodec::decodeSetBlendPalettes(doc.as<JsonObjectConst>());
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_UINT8(15, result.request.palette3);
}

void test_decode_set_blend_palettes_missing_required() {
    const char* json = R"({"palette1":5})";
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(json, doc));

    ColorSetBlendPalettesDecodeResult result = WsColorCodec::decodeSetBlendPalettes(doc.as<JsonObjectConst>());
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(strlen(result.errorMsg) > 0);
}

void test_decode_set_blend_factors_valid_defaults_factor3() {
    const char* json = R"({"factor1":100,"factor2":150})";
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(json, doc));

    ColorSetBlendFactorsDecodeResult result = WsColorCodec::decodeSetBlendFactors(doc.as<JsonObjectConst>());
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_UINT8(100, result.request.factor1);
    TEST_ASSERT_EQUAL_UINT8(150, result.request.factor2);
    TEST_ASSERT_EQUAL_UINT8(0, result.request.factor3);
}

void test_decode_set_blend_factors_out_of_range() {
    const char* json = R"({"factor1":100,"factor2":300})";
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(json, doc));

    ColorSetBlendFactorsDecodeResult result = WsColorCodec::decodeSetBlendFactors(doc.as<JsonObjectConst>());
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(strlen(result.errorMsg) > 0);
}

// ============================================================================
// Group D: Complex optional fields
// ============================================================================

void test_decode_set_config_partial_valid() {
    const char* json = R"({
        "requestId":"cfg1",
        "mode":2,
        "autoExposureEnabled":true,
        "gammaValue":2.4,
        "maxGreenPercentOfRed":28
    })";
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(json, doc));

    ColorCorrectionSetConfigDecodeResult result = WsColorCodec::decodeSetConfig(doc.as<JsonObjectConst>());
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_STRING("cfg1", result.request.requestId);
    TEST_ASSERT_TRUE(result.request.hasMode);
    TEST_ASSERT_EQUAL_UINT8(2, result.request.mode);
    TEST_ASSERT_TRUE(result.request.hasAutoExposureEnabled);
    TEST_ASSERT_TRUE(result.request.autoExposureEnabled);
    TEST_ASSERT_TRUE(result.request.hasGammaValue);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.4f, result.request.gammaValue);
    TEST_ASSERT_TRUE(result.request.hasMaxGreenPercentOfRed);
    TEST_ASSERT_EQUAL_UINT8(28, result.request.maxGreenPercentOfRed);
}

void test_decode_set_config_invalid_gamma_value() {
    const char* json = R"({"gammaValue":3.5})";
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(json, doc));

    ColorCorrectionSetConfigDecodeResult result = WsColorCodec::decodeSetConfig(doc.as<JsonObjectConst>());
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(strlen(result.errorMsg) > 0);
}

// ============================================================================
// Unity
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_decode_enable_blend_valid);
    RUN_TEST(test_decode_enable_blend_missing_enable);
    RUN_TEST(test_decode_enable_rotation_valid);
    RUN_TEST(test_decode_enable_diffusion_valid);
    RUN_TEST(test_decode_set_diffusion_amount_valid);
    RUN_TEST(test_decode_set_diffusion_amount_out_of_range);
    RUN_TEST(test_decode_set_mode_valid);
    RUN_TEST(test_decode_set_mode_out_of_range);
    RUN_TEST(test_decode_set_rotation_speed_valid);
    RUN_TEST(test_decode_set_rotation_speed_missing);
    RUN_TEST(test_decode_set_blend_palettes_valid_defaults_palette3);
    RUN_TEST(test_decode_set_blend_palettes_valid_with_palette3);
    RUN_TEST(test_decode_set_blend_palettes_missing_required);
    RUN_TEST(test_decode_set_blend_factors_valid_defaults_factor3);
    RUN_TEST(test_decode_set_blend_factors_out_of_range);
    RUN_TEST(test_decode_set_config_partial_valid);
    RUN_TEST(test_decode_set_config_invalid_gamma_value);

    UNITY_END();
    return 0;
}

#endif // NATIVE_BUILD
