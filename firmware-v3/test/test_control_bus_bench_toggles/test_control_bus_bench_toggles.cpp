#include <unity.h>

#include "audio/contracts/ControlBus.h"

using lightwaveos::audio::AudioTime;
using lightwaveos::audio::ControlBus;
using lightwaveos::audio::ControlBusRawInput;
using lightwaveos::audio::CONTROLBUS_NUM_BANDS;
using lightwaveos::audio::CONTROLBUS_NUM_CHROMA;
using lightwaveos::audio::CONTROLBUS_NUM_ZONES;

namespace {

AudioTime hopTime(uint32_t hop) {
    return AudioTime(static_cast<uint64_t>(hop) * 256ULL, 12800U, static_cast<uint64_t>(hop) * 20000ULL);
}

ControlBusRawInput makeRaw(float band0, float chroma0) {
    ControlBusRawInput raw{};
    raw.rms = 0.5f;
    raw.rmsUngated = 0.5f;
    raw.flux = 0.2f;
    raw.bands[0] = band0;
    raw.chroma[0] = chroma0;
    return raw;
}

ControlBusRawInput makeRaw(const float (&bands)[CONTROLBUS_NUM_BANDS],
                           const float (&chroma)[CONTROLBUS_NUM_CHROMA]) {
    ControlBusRawInput raw{};
    raw.rms = 0.5f;
    raw.rmsUngated = 0.5f;
    raw.flux = 0.2f;
    for (uint8_t i = 0; i < CONTROLBUS_NUM_BANDS; ++i) {
        raw.bands[i] = bands[i];
    }
    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
        raw.chroma[i] = chroma[i];
    }
    return raw;
}

void configureImmediateBands(ControlBus& bus) {
    bus.setSmoothing(1.0f, 1.0f);
    bus.setAttackRelease(1.0f, 1.0f, 1.0f, 1.0f);
    bus.setZoneAGCRates(1.0f, 1.0f);
    bus.setChromaZoneAGCRates(1.0f, 1.0f);
    bus.setZoneMinFloor(0.0001f);
    bus.setLookaheadEnabled(true);
    bus.setZoneAGCEnabled(true);
    bus.setChromaZoneAGCEnabled(true);
}

void assertBandsEqual(const ControlBus& bus, const float (&expected)[CONTROLBUS_NUM_BANDS]) {
    const auto& frame = bus.GetFrameRef();
    for (uint8_t i = 0; i < CONTROLBUS_NUM_BANDS; ++i) {
        TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected[i], frame.bands[i]);
    }
}

void assertChromaEqual(const ControlBus& bus, const float (&expected)[CONTROLBUS_NUM_CHROMA]) {
    const auto& frame = bus.GetFrameRef();
    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
        TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected[i], frame.chroma[i]);
    }
}

void test_bench_lookahead_false_bypasses_warmup_delay() {
    ControlBus delayed;
    configureImmediateBands(delayed);
    delayed.setBenchAudioToggles(true, false, false);
    delayed.UpdateFromHop(hopTime(1), makeRaw(0.6f, 0.7f));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, delayed.GetFrameRef().bands[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, delayed.GetFrameRef().chroma[0]);

    ControlBus passthrough;
    configureImmediateBands(passthrough);
    passthrough.setBenchAudioToggles(false, false, false);
    passthrough.UpdateFromHop(hopTime(1), makeRaw(0.6f, 0.7f));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.6f, passthrough.GetFrameRef().bands[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.7f, passthrough.GetFrameRef().chroma[0]);
}

void test_bench_zone_agc_false_preserves_band_levels() {
    ControlBus enabled;
    configureImmediateBands(enabled);
    enabled.setBenchAudioToggles(false, true, false);
    enabled.UpdateFromHop(hopTime(1), makeRaw(0.2f, 0.2f));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, enabled.GetFrameRef().bands[0]);

    ControlBus disabled;
    configureImmediateBands(disabled);
    disabled.setBenchAudioToggles(false, false, false);
    disabled.UpdateFromHop(hopTime(1), makeRaw(0.2f, 0.2f));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.2f, disabled.GetFrameRef().bands[0]);
}

void test_bench_chroma_zone_agc_false_preserves_chroma_levels() {
    ControlBus enabled;
    configureImmediateBands(enabled);
    enabled.setBenchAudioToggles(false, false, true);
    enabled.UpdateFromHop(hopTime(1), makeRaw(0.2f, 0.2f));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, enabled.GetFrameRef().chroma[0]);

    ControlBus disabled;
    configureImmediateBands(disabled);
    disabled.setBenchAudioToggles(false, false, false);
    disabled.UpdateFromHop(hopTime(1), makeRaw(0.2f, 0.2f));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.2f, disabled.GetFrameRef().chroma[0]);
}

void test_zone_agc_uses_three_semantic_band_partitions() {
    TEST_ASSERT_EQUAL_UINT8(3, CONTROLBUS_NUM_ZONES);

    const float bands[CONTROLBUS_NUM_BANDS] = {
        0.2f, 0.4f,
        0.3f, 0.6f, 0.9f,
        0.5f, 0.8f, 1.0f
    };
    const float chroma[CONTROLBUS_NUM_CHROMA] = {};

    ControlBus bus;
    configureImmediateBands(bus);
    bus.setBenchAudioToggles(false, true, false);
    bus.UpdateFromHop(hopTime(1), makeRaw(bands, chroma));

    const float expected[CONTROLBUS_NUM_BANDS] = {
        0.5f, 1.0f,
        0.3333333f, 0.6666667f, 1.0f,
        0.5f, 0.8f, 1.0f
    };
    assertBandsEqual(bus, expected);

    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.4f, bus.getZoneMaxMag(0));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.9f, bus.getZoneMaxMag(1));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, bus.getZoneMaxMag(2));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, bus.getZoneMaxMag(CONTROLBUS_NUM_ZONES));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, bus.getZoneFollower(CONTROLBUS_NUM_ZONES));
}

void test_chroma_zone_agc_uses_three_four_bin_partitions() {
    TEST_ASSERT_EQUAL_UINT8(3, CONTROLBUS_NUM_ZONES);

    const float bands[CONTROLBUS_NUM_BANDS] = {};
    const float chroma[CONTROLBUS_NUM_CHROMA] = {
        0.2f, 0.4f, 0.6f, 0.8f,
        0.1f, 0.5f, 0.75f, 1.0f,
        0.3f, 0.45f, 0.6f, 0.15f
    };

    ControlBus bus;
    configureImmediateBands(bus);
    bus.setBenchAudioToggles(false, false, true);
    bus.UpdateFromHop(hopTime(1), makeRaw(bands, chroma));

    const float expected[CONTROLBUS_NUM_CHROMA] = {
        0.25f, 0.5f, 0.75f, 1.0f,
        0.1f, 0.5f, 0.75f, 1.0f,
        0.5f, 0.75f, 1.0f, 0.25f
    };
    assertChromaEqual(bus, expected);

    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.8f, bus.getChromaZoneMaxMag(0));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, bus.getChromaZoneMaxMag(1));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.6f, bus.getChromaZoneMaxMag(2));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, bus.getChromaZoneMaxMag(CONTROLBUS_NUM_ZONES));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, bus.getChromaZoneFollower(CONTROLBUS_NUM_ZONES));
}

void test_zone_agc_disabled_preserves_all_band_and_chroma_levels() {
    const float bands[CONTROLBUS_NUM_BANDS] = {
        0.12f, 0.24f, 0.36f, 0.48f, 0.60f, 0.72f, 0.84f, 0.96f
    };
    const float chroma[CONTROLBUS_NUM_CHROMA] = {
        0.05f, 0.15f, 0.25f, 0.35f, 0.45f, 0.55f,
        0.65f, 0.75f, 0.85f, 0.95f, 0.40f, 0.20f
    };

    ControlBus bus;
    configureImmediateBands(bus);
    bus.setBenchAudioToggles(false, false, false);
    bus.UpdateFromHop(hopTime(1), makeRaw(bands, chroma));

    assertBandsEqual(bus, bands);
    assertChromaEqual(bus, chroma);
}

} // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_bench_lookahead_false_bypasses_warmup_delay);
    RUN_TEST(test_bench_zone_agc_false_preserves_band_levels);
    RUN_TEST(test_bench_chroma_zone_agc_false_preserves_chroma_levels);
    RUN_TEST(test_zone_agc_uses_three_semantic_band_partitions);
    RUN_TEST(test_chroma_zone_agc_uses_three_four_bin_partitions);
    RUN_TEST(test_zone_agc_disabled_preserves_all_band_and_chroma_levels);
    return UNITY_END();
}
