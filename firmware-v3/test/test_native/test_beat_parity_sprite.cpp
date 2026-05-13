// Phase 5 Move 5.7 — Beat Parity Sprite tests.
//
// Coverage targets for the corruption repair:
//   1. Sustained RMS without a musical launch event must not paint a bed.
//   2. Silence/low confidence must suppress bed writes before composition.
//   3. Kick launches a centre-origin sprite state.
//
// These tests deliberately exercise render(), not only the native test seam,
// because the observed failure was visual background flicker.

#include <unity.h>

#include <cstdint>
#include <cstring>

#include "effects/ieffect/BeatParitySpriteEffect.h"
#include "plugins/api/EffectContext.h"

using lightwaveos::effects::ieffect::BeatParitySpriteEffect;
using lightwaveos::plugins::EffectContext;

namespace {

constexpr uint16_t kStripTotal = 320;

void primeContext(EffectContext& ctx, CRGB* buf) {
    std::memset(buf, 0, sizeof(CRGB) * kStripTotal);
    ctx.leds = buf;
    ctx.ledCount = kStripTotal;
    ctx.centerPoint = 80;
    ctx.deltaTimeSeconds = 0.008f;
    ctx.rawDeltaTimeSeconds = 0.008f;
    ctx.deltaTimeMs = 8;
    ctx.rawDeltaTimeMs = 8;
    ctx.gHue = 96;  // Non-black in the native palette mock; catches gHue beds.

    auto& bus = ctx.audio.controlBus;
    std::memset(bus.chroma, 0, sizeof(bus.chroma));
    bus.rms = 0.0f;
    bus.fast_rms = 0.0f;
    bus.kickTrigger = false;
    bus.tempoBeatTick = false;
    bus.tempoConfidence = 0.0f;
    bus.audioConfidence = 1.0f;
    bus.silentScale = 1.0f;
    ctx.audio.available = true;
}

bool isLedDark(const CRGB& c) {
    return c.r == 0 && c.g == 0 && c.b == 0;
}

bool stripIsAllDark(const CRGB* buf, uint16_t n) {
    for (uint16_t i = 0; i < n; ++i) {
        if (!isLedDark(buf[i])) return false;
    }
    return true;
}

bool anyLitInCentrePair(const CRGB* buf) {
    return !isLedDark(buf[79]) || !isLedDark(buf[80])
        || !isLedDark(buf[239]) || !isLedDark(buf[240]);
}

}  // namespace

void test_sustained_rms_without_kick_keeps_strip_dark() {
    BeatParitySpriteEffect fx;
    EffectContext ctx;
    CRGB buf[kStripTotal];
    primeContext(ctx, buf);

    TEST_ASSERT_TRUE(fx.init(ctx));

    auto& bus = ctx.audio.controlBus;
    bus.rms = 0.85f;
    bus.fast_rms = 0.85f;
    bus.kickTrigger = false;
    bus.audioConfidence = 1.0f;
    bus.silentScale = 1.0f;

    for (int frame = 0; frame < 30; ++frame) {
        fx.render(ctx);
    }

    TEST_ASSERT_TRUE(stripIsAllDark(buf, kStripTotal));
    fx.cleanup();
}

void test_low_confidence_suppresses_rms_bed_before_write() {
    BeatParitySpriteEffect fx;
    EffectContext ctx;
    CRGB buf[kStripTotal];
    primeContext(ctx, buf);

    TEST_ASSERT_TRUE(fx.init(ctx));

    auto& bus = ctx.audio.controlBus;
    bus.rms = 0.9f;
    bus.fast_rms = 0.9f;
    bus.kickTrigger = false;
    bus.audioConfidence = 0.0f;
    bus.silentScale = 0.0f;

    fx.render(ctx);

    TEST_ASSERT_TRUE(stripIsAllDark(buf, kStripTotal));
    fx.cleanup();
}

void test_kick_launches_centre_origin_sprite_state() {
    BeatParitySpriteEffect fx;
    EffectContext ctx;
    CRGB buf[kStripTotal];
    primeContext(ctx, buf);

    TEST_ASSERT_TRUE(fx.init(ctx));

    auto& bus = ctx.audio.controlBus;
    bus.chroma[4] = 1.0f;  // avoid native palette black anchor
    bus.kickTrigger = true;
    bus.audioConfidence = 1.0f;
    bus.silentScale = 1.0f;

    fx.render(ctx);

    TEST_ASSERT_EQUAL_UINT8(1, fx.activeSpriteCount());
    TEST_ASSERT_TRUE(anyLitInCentrePair(buf));
    fx.cleanup();
}

void run_beat_parity_sprite_tests() {
    RUN_TEST(test_sustained_rms_without_kick_keeps_strip_dark);
    RUN_TEST(test_low_confidence_suppresses_rms_bed_before_write);
    RUN_TEST(test_kick_launches_centre_origin_sprite_state);
}
