#include <unity.h>

#include "audio/contracts/ControlBus.h"

using lightwaveos::audio::AudioTime;
using lightwaveos::audio::ControlBus;
using lightwaveos::audio::ControlBusRawInput;

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

} // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_bench_lookahead_false_bypasses_warmup_delay);
    RUN_TEST(test_bench_zone_agc_false_preserves_band_levels);
    RUN_TEST(test_bench_chroma_zone_agc_false_preserves_chroma_levels);
    return UNITY_END();
}
