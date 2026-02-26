/**
 * @file test_ws_color_codec.cpp
 * @brief Unit tests for WsColorCodec decode contracts
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <ArduinoJson.h>

#include "../../src/codec/WsColorCodec.h"

using namespace lightwaveos::codec;

static bool loadJsonString(const char* jsonStr, JsonDocument& doc) {
    DeserializationError error = deserializeJson(doc, jsonStr);
    return error == DeserializationError::Ok;
}

void test_decode_enable_blend_valid() {
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(R"({"requestId":"r1","enable":true})", doc));

    ColorEnableBlendDecodeResult result = WsColorCodec::decodeEnableBlend(doc.as<JsonObjectConst>());
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_TRUE(result.request.enable);
    TEST_ASSERT_EQUAL_STRING("r1", result.request.requestId);
}

void test_decode_enable_blend_missing_enable() {
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(R"({"requestId":"r1"})", doc));

    ColorEnableBlendDecodeResult result = WsColorCodec::decodeEnableBlend(doc.as<JsonObjectConst>());
    TEST_ASSERT_FALSE(result.success);
}

void test_decode_set_diffusion_amount_valid() {
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(R"({"amount":128})", doc));

    ColorSetDiffusionAmountDecodeResult result = WsColorCodec::decodeSetDiffusionAmount(doc.as<JsonObjectConst>());
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_UINT8(128, result.request.amount);
}

void test_decode_set_diffusion_amount_out_of_range() {
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(R"({"amount":300})", doc));

    ColorSetDiffusionAmountDecodeResult result = WsColorCodec::decodeSetDiffusionAmount(doc.as<JsonObjectConst>());
    TEST_ASSERT_FALSE(result.success);
}

void test_decode_set_mode_valid() {
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(R"({"mode":2})", doc));

    ColorCorrectionSetModeDecodeResult result = WsColorCodec::decodeSetMode(doc.as<JsonObjectConst>());
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_UINT8(2, result.request.mode);
}

void test_decode_set_mode_out_of_range() {
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(R"({"mode":5})", doc));

    ColorCorrectionSetModeDecodeResult result = WsColorCodec::decodeSetMode(doc.as<JsonObjectConst>());
    TEST_ASSERT_FALSE(result.success);
}

void test_decode_set_rotation_speed_valid() {
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(R"({"degreesPerFrame":2.5})", doc));

    ColorSetRotationSpeedDecodeResult result = WsColorCodec::decodeSetRotationSpeed(doc.as<JsonObjectConst>());
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, result.request.degreesPerFrame);
}

void test_decode_set_blend_palettes_defaults_palette3() {
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(R"({"palette1":5,"palette2":10})", doc));

    ColorSetBlendPalettesDecodeResult result = WsColorCodec::decodeSetBlendPalettes(doc.as<JsonObjectConst>());
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_UINT8(5, result.request.palette1);
    TEST_ASSERT_EQUAL_UINT8(10, result.request.palette2);
    TEST_ASSERT_EQUAL_UINT8(255, result.request.palette3);
}

void test_decode_set_blend_factors_defaults_factor3() {
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(R"({"factor1":100,"factor2":150})", doc));

    ColorSetBlendFactorsDecodeResult result = WsColorCodec::decodeSetBlendFactors(doc.as<JsonObjectConst>());
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_UINT8(100, result.request.factor1);
    TEST_ASSERT_EQUAL_UINT8(150, result.request.factor2);
    TEST_ASSERT_EQUAL_UINT8(0, result.request.factor3);
}

void test_decode_set_config_valid() {
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(
        R"({"mode":2,"hsvMinSaturation":120,"gammaEnabled":true,"gammaValue":2.2})",
        doc));

    ColorCorrectionSetConfigDecodeResult result = WsColorCodec::decodeSetConfig(doc.as<JsonObjectConst>());
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_TRUE(result.request.hasMode);
    TEST_ASSERT_TRUE(result.request.hasHsvMinSaturation);
    TEST_ASSERT_TRUE(result.request.hasGammaEnabled);
    TEST_ASSERT_TRUE(result.request.hasGammaValue);
    TEST_ASSERT_EQUAL_UINT8(2, result.request.mode);
    TEST_ASSERT_EQUAL_UINT8(120, result.request.hsvMinSaturation);
    TEST_ASSERT_TRUE(result.request.gammaEnabled);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.2f, result.request.gammaValue);
}

void test_decode_set_config_invalid_gamma() {
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(R"({"gammaValue":3.5})", doc));

    ColorCorrectionSetConfigDecodeResult result = WsColorCodec::decodeSetConfig(doc.as<JsonObjectConst>());
    TEST_ASSERT_FALSE(result.success);
}

void setUp(void) {}
void tearDown(void) {}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_decode_enable_blend_valid);
    RUN_TEST(test_decode_enable_blend_missing_enable);
    RUN_TEST(test_decode_set_diffusion_amount_valid);
    RUN_TEST(test_decode_set_diffusion_amount_out_of_range);
    RUN_TEST(test_decode_set_mode_valid);
    RUN_TEST(test_decode_set_mode_out_of_range);
    RUN_TEST(test_decode_set_rotation_speed_valid);
    RUN_TEST(test_decode_set_blend_palettes_defaults_palette3);
    RUN_TEST(test_decode_set_blend_factors_defaults_factor3);
    RUN_TEST(test_decode_set_config_valid);
    RUN_TEST(test_decode_set_config_invalid_gamma);

    UNITY_END();
    return 0;
}

#endif
