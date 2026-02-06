/**
 * @file test_audio_mapping_registry.cpp
 * @brief Native unit tests for AudioMappingRegistry PSRAM allocation + disabled-state guards.
 */

#include <unity.h>

#include <cstdlib>

#include "audio/contracts/AudioEffectMapping.h"

using namespace lightwaveos::audio;

static void* alloc_fail(size_t, size_t) { return nullptr; }
static void* alloc_ok(size_t count, size_t size) { return std::calloc(count, size); }

static void test_registry_before_begin_is_safe() {
    auto& registry = AudioMappingRegistry::instance();
    TEST_ASSERT_NULL(registry.getMapping(0));

    ControlBusFrame bus{};
    MusicalGridSnapshot grid{};

    uint8_t brightness = 10;
    uint8_t speed = 10;
    uint8_t intensity = 10;
    uint8_t saturation = 10;
    uint8_t complexity = 10;
    uint8_t variation = 10;
    uint8_t hue = 10;

    registry.applyMappings(0, bus, grid, true, brightness, speed, intensity, saturation, complexity, variation, hue);

    TEST_ASSERT_EQUAL_UINT8(10, brightness);
    TEST_ASSERT_EQUAL_UINT8(10, speed);
    TEST_ASSERT_EQUAL_UINT8(10, intensity);
    TEST_ASSERT_EQUAL_UINT8(10, saturation);
    TEST_ASSERT_EQUAL_UINT8(10, complexity);
    TEST_ASSERT_EQUAL_UINT8(10, variation);
    TEST_ASSERT_EQUAL_UINT8(10, hue);
}

static void test_registry_begin_failure_then_recover() {
    auto& registry = AudioMappingRegistry::instance();

    AudioMappingRegistry::setTestAllocator(alloc_fail);
    TEST_ASSERT_FALSE(registry.begin());
    TEST_ASSERT_NULL(registry.getMapping(0));

    AudioMappingRegistry::setTestAllocator(alloc_ok);
    TEST_ASSERT_TRUE(registry.begin());

    const EffectAudioMapping* mapping0 = registry.getMapping(0);
    TEST_ASSERT_NOT_NULL(mapping0);
    TEST_ASSERT_EQUAL_UINT8(EffectAudioMapping::VERSION, mapping0->version);
    TEST_ASSERT_EQUAL_UINT8(0, mapping0->effectId);
}

static void test_registry_set_mapping_and_apply() {
    auto& registry = AudioMappingRegistry::instance();
    TEST_ASSERT_TRUE(registry.begin());

    EffectAudioMapping cfg{};
    cfg.globalEnabled = true;
    cfg.mappingCount = 1;
    cfg.mappings[0].source = AudioSource::RMS;
    cfg.mappings[0].target = VisualTarget::BRIGHTNESS;
    cfg.mappings[0].curve = MappingCurve::LINEAR;
    cfg.mappings[0].inputMin = 0.0f;
    cfg.mappings[0].inputMax = 1.0f;
    cfg.mappings[0].outputMin = 0.0f;
    cfg.mappings[0].outputMax = 160.0f;
    cfg.mappings[0].smoothingAlpha = 1.0f;  // Immediate for deterministic test
    cfg.mappings[0].gain = 1.0f;
    cfg.mappings[0].enabled = true;
    cfg.mappings[0].additive = false;

    TEST_ASSERT_TRUE(registry.setMapping(0, cfg));
    TEST_ASSERT_TRUE(registry.hasActiveMappings(0));

    ControlBusFrame bus{};
    bus.rms = 0.5f;
    MusicalGridSnapshot grid{};

    uint8_t brightness = 0;
    uint8_t speed = 10;
    uint8_t intensity = 10;
    uint8_t saturation = 10;
    uint8_t complexity = 10;
    uint8_t variation = 10;
    uint8_t hue = 10;

    registry.applyMappings(0, bus, grid, true, brightness, speed, intensity, saturation, complexity, variation, hue);
    TEST_ASSERT_GREATER_THAN_UINT8(0, brightness);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_registry_before_begin_is_safe);
    RUN_TEST(test_registry_begin_failure_then_recover);
    RUN_TEST(test_registry_set_mapping_and_apply);
    return UNITY_END();
}

