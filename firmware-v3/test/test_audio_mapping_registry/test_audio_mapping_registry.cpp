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

    registry.applyMappings(0, bus, grid, true, 0.008f, brightness, speed, intensity, saturation, complexity, variation, hue);

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
    TEST_ASSERT_EQUAL_UINT16(0, registry.getActiveEffectCount());
    TEST_ASSERT_EQUAL_UINT16(0, registry.getTotalMappingCount());
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

    registry.applyMappings(0, bus, grid, true, 0.008f, brightness, speed, intensity, saturation, complexity, variation, hue);
    TEST_ASSERT_GREATER_THAN_UINT8(0, brightness);
}

static void test_move_1_5_sources_reuse_existing_controlbus_fields() {
    ControlBusFrame bus{};
    MusicalGridSnapshot grid{};

    bus.heavy_bands[2] = 0.3f;
    bus.heavy_bands[3] = 0.6f;
    bus.heavy_bands[4] = 0.9f;
    bus.audioConfidence = 0.7f;
    bus.liveliness = 0.8f;
    bus.silentScale = 0.4f;
    bus.onsetEvent = 0.5f;
    bus.kickTrigger = true;
    bus.snareEnergy = 0.35f;
    bus.hihatEnergy = 0.45f;
    bus.chroma[0] = 0.2f;
    bus.chroma[7] = 0.9f;
    bus.chordState.confidence = 0.65f;
    bus.saliency.overallSaliency = 0.55f;
    bus.saliency.harmonicNoveltySmooth = 0.25f;
    bus.saliency.rhythmicNoveltySmooth = 0.75f;
    bus.saliency.timbralNoveltySmooth = 0.45f;
    bus.saliency.dynamicNoveltySmooth = 0.85f;
    bus.scene.beat_pulse = 0.95f;
    bus.scene.phrase_progress = 0.15f;
    bus.scene.tension = 0.6f;

    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.6f,
        AudioMappingRegistry::getAudioValue(AudioSource::HEAVY_MID, bus, grid));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.7f,
        AudioMappingRegistry::getAudioValue(AudioSource::AUDIO_CONFIDENCE, bus, grid));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.8f,
        AudioMappingRegistry::getAudioValue(AudioSource::LIVELINESS, bus, grid));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.4f,
        AudioMappingRegistry::getAudioValue(AudioSource::SILENT_SCALE, bus, grid));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5f,
        AudioMappingRegistry::getAudioValue(AudioSource::ONSET_EVENT, bus, grid));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f,
        AudioMappingRegistry::getAudioValue(AudioSource::KICK_LEVEL, bus, grid));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.35f,
        AudioMappingRegistry::getAudioValue(AudioSource::SNARE_LEVEL, bus, grid));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.45f,
        AudioMappingRegistry::getAudioValue(AudioSource::HIHAT_LEVEL, bus, grid));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.9f,
        AudioMappingRegistry::getAudioValue(AudioSource::CHROMA_MAX, bus, grid));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.65f,
        AudioMappingRegistry::getAudioValue(AudioSource::CHORD_CONFIDENCE, bus, grid));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.55f,
        AudioMappingRegistry::getAudioValue(AudioSource::OVERALL_SALIENCY, bus, grid));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.25f,
        AudioMappingRegistry::getAudioValue(AudioSource::HARMONIC_SALIENCY, bus, grid));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.75f,
        AudioMappingRegistry::getAudioValue(AudioSource::RHYTHMIC_SALIENCY, bus, grid));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.45f,
        AudioMappingRegistry::getAudioValue(AudioSource::TIMBRAL_SALIENCY, bus, grid));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.85f,
        AudioMappingRegistry::getAudioValue(AudioSource::DYNAMIC_SALIENCY, bus, grid));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.95f,
        AudioMappingRegistry::getAudioValue(AudioSource::BEAT_PULSE, bus, grid));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.15f,
        AudioMappingRegistry::getAudioValue(AudioSource::PHRASE_PROGRESS, bus, grid));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.6f,
        AudioMappingRegistry::getAudioValue(AudioSource::TENSION, bus, grid));

    TEST_ASSERT_EQUAL_STRING("OVERALL_SALIENCY",
        AudioMappingRegistry::getSourceName(AudioSource::OVERALL_SALIENCY));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AudioSource::BEAT_PULSE),
        static_cast<uint8_t>(AudioMappingRegistry::parseSource("BEAT_PULSE")));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_registry_before_begin_is_safe);
    RUN_TEST(test_registry_begin_failure_then_recover);
    RUN_TEST(test_registry_set_mapping_and_apply);
    RUN_TEST(test_move_1_5_sources_reuse_existing_controlbus_fields);
    return UNITY_END();
}
