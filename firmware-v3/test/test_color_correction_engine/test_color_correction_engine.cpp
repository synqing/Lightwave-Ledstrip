#include <unity.h>

#include <Preferences.h>

#include "effects/enhancement/ColorCorrectionEngine.h"

using lightwaveos::enhancement::ColorCorrectionConfig;
using lightwaveos::enhancement::ColorCorrectionEngine;
using lightwaveos::enhancement::CorrectionMode;

namespace {

uint8_t gammaRedFor(uint8_t input) {
    CRGB pixel(input, 0, 0);
    ColorCorrectionEngine::getInstance().applyGamma(&pixel, 1);
    return pixel.r;
}

void seedPersistedGamma(float gammaValue) {
    Preferences::resetMockStore();
    Preferences prefs;
    TEST_ASSERT_TRUE(prefs.begin("colorCorr", false));
    prefs.putUChar("mode", static_cast<uint8_t>(CorrectionMode::BOTH));
    prefs.putBool("gammaEn", true);
    prefs.putFloat("gammaVal", gammaValue);
    prefs.end();
}

void test_load_from_nvs_rebuilds_gamma_lut_to_persisted_value() {
    auto& engine = ColorCorrectionEngine::getInstance();

    ColorCorrectionConfig reset = engine.getConfig();
    reset.gammaEnabled = true;
    reset.gammaValue = 2.2f;
    engine.setConfig(reset);
    const uint8_t gamma22Mid = gammaRedFor(128);

    seedPersistedGamma(1.0f);
    engine.loadFromNVS();

    TEST_ASSERT_EQUAL_FLOAT(1.0f, engine.getConfig().gammaValue);
    TEST_ASSERT_EQUAL_UINT8(128, gammaRedFor(128));
    TEST_ASSERT_NOT_EQUAL(gamma22Mid, gammaRedFor(128));
}

void test_gamma_lut_status_reports_generation_and_samples() {
    auto& engine = ColorCorrectionEngine::getInstance();

    ColorCorrectionConfig cfg = engine.getConfig();
    cfg.gammaEnabled = true;
    cfg.gammaValue = 2.2f;
    engine.setConfig(cfg);

    const auto status22 = engine.getGammaLutStatus();
    TEST_ASSERT_TRUE(status22.gammaEnabled);
    TEST_ASSERT_EQUAL_FLOAT(2.2f, status22.gammaValue);
    TEST_ASSERT_EQUAL_UINT8(0, status22.lut0);
    TEST_ASSERT_EQUAL_UINT8(255, status22.lut255);

    cfg.gammaValue = 1.4f;
    engine.setConfig(cfg);

    const auto status14 = engine.getGammaLutStatus();
    TEST_ASSERT_TRUE(status14.lutGenerationId > status22.lutGenerationId);
    TEST_ASSERT_EQUAL_FLOAT(1.4f, status14.gammaValue);
    TEST_ASSERT_EQUAL_UINT8(0, status14.lut0);
    TEST_ASSERT_EQUAL_UINT8(255, status14.lut255);
    TEST_ASSERT_NOT_EQUAL(status22.lut128, status14.lut128);

    cfg.gammaEnabled = false;
    cfg.gammaValue = 2.8f;
    engine.setConfig(cfg);

    const auto status28Disabled = engine.getGammaLutStatus();
    TEST_ASSERT_FALSE(status28Disabled.gammaEnabled);
    TEST_ASSERT_TRUE(status28Disabled.lutGenerationId > status14.lutGenerationId);
    TEST_ASSERT_EQUAL_FLOAT(2.8f, status28Disabled.gammaValue);
    TEST_ASSERT_NOT_EQUAL(status14.lut128, status28Disabled.lut128);
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_load_from_nvs_rebuilds_gamma_lut_to_persisted_value);
    RUN_TEST(test_gamma_lut_status_reports_generation_and_samples);
    return UNITY_END();
}
