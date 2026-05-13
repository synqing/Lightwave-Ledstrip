// Focused native regression for K1 Waveform Hybrid colour budget.
//
// Captain visual judgement, 2026-05-07:
//   - 0x1313 Hybrid responds more musically than 0x1302 standard Waveform.
//   - Hybrid is too dark unless PHOTONS brightness is near maximum.
//   - After the first lift, colour saturation appears to max around brightness
//     200, while anything below roughly 150 remains unacceptable.
//   - Both 0x1302 and 0x1313 must run at native speed 27; lower speed values
//     appear sluggish and out of sync with music.
//
// This test fixes the expected non-max-brightness floor without touching
// global VP defaults. It also locks the waveform-family speed floor locally.

#include <unity.h>

#include <cstdint>
#include <cstring>

#include "effects/ieffect/sensorybridge_reference/SbK1WaveformEffect.h"
#include "effects/ieffect/sensorybridge_reference/SbK1WaveformHybridEffect.h"
#include "plugins/api/EffectContext.h"

using lightwaveos::effects::ieffect::sensorybridge_reference::SbK1WaveformEffect;
using lightwaveos::effects::ieffect::sensorybridge_reference::SbK1WaveformHybridEffect;
using lightwaveos::plugins::EffectContext;
using lightwaveos::plugins::PaletteRef;

namespace {

constexpr uint16_t kTotalLeds = 320;

void primeContext(EffectContext& ctx, CRGB* leds) {
    std::memset(leds, 0, sizeof(CRGB) * kTotalLeds);

    ctx.leds = leds;
    ctx.ledCount = kTotalLeds;
    ctx.centerPoint = 80;
    ctx.brightness = 149;
    ctx.speed = 25;
    ctx.saturation = 255;
    ctx.deltaTimeMs = 8;
    ctx.rawDeltaTimeMs = 8;
    ctx.deltaTimeSeconds = 1.0f / 120.0f;
    ctx.rawDeltaTimeSeconds = 1.0f / 120.0f;
    ctx.palette = PaletteRef(reinterpret_cast<const void*>(1));

    ctx.audio.available = true;
    auto& bus = ctx.audio.controlBus;
    std::memset(bus.chroma, 0, sizeof(bus.chroma));
    std::memset(bus.waveform, 0, sizeof(bus.waveform));
    bus.chroma[8] = 1.0f;  // High native palette index avoids black-anchor false negatives.
    bus.rms = 0.10f;
    bus.flux = 0.0f;
    bus.audioConfidence = 1.0f;
    bus.silentScale = 1.0f;
}

uint8_t maxChannel(const CRGB* leds, uint16_t count) {
    uint8_t maxValue = 0;
    for (uint16_t i = 0; i < count; ++i) {
        const uint8_t localMax = leds[i].getMaxChannel();
        if (localMax > maxValue) maxValue = localMax;
    }
    return maxValue;
}

uint16_t furthestLitRight(const CRGB* leds, uint8_t floor) {
    for (uint16_t i = 159; i >= 80; --i) {
        if (leds[i].getMaxChannel() >= floor) return i;
        if (i == 80) break;
    }
    return 80;
}

uint16_t litCount(const CRGB* leds, uint16_t start, uint16_t endExclusive, uint8_t floor) {
    uint16_t count = 0;
    for (uint16_t i = start; i < endExclusive; ++i) {
        if (leds[i].getMaxChannel() >= floor) ++count;
    }
    return count;
}

template <typename EffectT>
uint16_t renderFurthestRightAtSpeed(uint8_t speed) {
    EffectT fx;
    EffectContext ctx;
    CRGB leds[kTotalLeds];

    primeContext(ctx, leds);
    ctx.brightness = 255;
    ctx.speed = speed;
    TEST_ASSERT_TRUE(fx.init(ctx));

    for (uint8_t frame = 0; frame < 18; ++frame) {
        fx.render(ctx);
    }

    const uint16_t furthest = furthestLitRight(leds, 1);
    fx.cleanup();
    return furthest;
}

template <typename EffectT>
uint16_t renderLitCountBelowWaveformFloor(float confidence, float silentScale) {
    EffectT fx;
    EffectContext ctx;
    CRGB leds[kTotalLeds];

    primeContext(ctx, leds);
    ctx.brightness = 255;
    ctx.speed = 27;
    ctx.audio.controlBus.rms = 0.018f;
    ctx.audio.controlBus.audioConfidence = confidence;
    ctx.audio.controlBus.silentScale = silentScale;
    std::memset(ctx.audio.controlBus.waveform, 0, sizeof(ctx.audio.controlBus.waveform));
    TEST_ASSERT_TRUE(fx.init(ctx));

    for (uint8_t frame = 0; frame < 28; ++frame) {
        fx.render(ctx);
    }

    const uint16_t count = litCount(leds, 0, 160, 1);
    fx.cleanup();
    return count;
}

}  // namespace

void test_hybrid_keeps_chroma_readable_below_max_brightness() {
    SbK1WaveformHybridEffect fx;
    EffectContext ctx;
    CRGB leds[kTotalLeds];

    primeContext(ctx, leds);
    TEST_ASSERT_TRUE(fx.init(ctx));

    for (uint8_t frame = 0; frame < 24; ++frame) {
        fx.render(ctx);
    }

    TEST_ASSERT_GREATER_OR_EQUAL_UINT8_MESSAGE(
        104,
        maxChannel(leds, kTotalLeds),
        "Hybrid should stay visibly readable below max PHOTONS brightness");
    TEST_ASSERT_EQUAL_UINT8(leds[80].getMaxChannel(), leds[79].getMaxChannel());
    TEST_ASSERT_EQUAL_UINT8(leds[80].getMaxChannel(), leds[240].getMaxChannel());

    fx.cleanup();
}

void test_standard_waveform_speed_25_uses_native_speed_27_motion_floor() {
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(
        renderFurthestRightAtSpeed<SbK1WaveformEffect>(27),
        renderFurthestRightAtSpeed<SbK1WaveformEffect>(25),
        "Standard Waveform should not render slower than the native speed 27 floor");
}

void test_hybrid_waveform_speed_25_uses_native_speed_27_motion_floor() {
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(
        renderFurthestRightAtSpeed<SbK1WaveformHybridEffect>(27),
        renderFurthestRightAtSpeed<SbK1WaveformHybridEffect>(25),
        "Hybrid Waveform should not render slower than the native speed 27 floor");
}

void test_standard_waveform_loiter_state_is_real_below_waveform_floor() {
    TEST_ASSERT_GREATER_THAN_UINT16_MESSAGE(
        80,
        renderLitCountBelowWaveformFloor<SbK1WaveformEffect>(1.0f, 1.0f),
        "Standard Waveform keeps painting a centre-sourced sheet while waveform is below floor");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(
        0,
        renderLitCountBelowWaveformFloor<SbK1WaveformEffect>(0.0f, 0.0f),
        "Standard Waveform loiter requires the confidence/silence surfaces to remain open");
}

void test_hybrid_waveform_loiter_state_is_real_below_waveform_floor() {
    TEST_ASSERT_GREATER_THAN_UINT16_MESSAGE(
        80,
        renderLitCountBelowWaveformFloor<SbK1WaveformHybridEffect>(1.0f, 1.0f),
        "Hybrid Waveform keeps painting a centre-sourced sheet while waveform is below floor");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(
        0,
        renderLitCountBelowWaveformFloor<SbK1WaveformHybridEffect>(0.0f, 0.0f),
        "Hybrid Waveform loiter requires the confidence/silence surfaces to remain open");
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_hybrid_keeps_chroma_readable_below_max_brightness);
    RUN_TEST(test_standard_waveform_speed_25_uses_native_speed_27_motion_floor);
    RUN_TEST(test_hybrid_waveform_speed_25_uses_native_speed_27_motion_floor);
    RUN_TEST(test_standard_waveform_loiter_state_is_real_below_waveform_floor);
    RUN_TEST(test_hybrid_waveform_loiter_state_is_real_below_waveform_floor);
    return UNITY_END();
}
