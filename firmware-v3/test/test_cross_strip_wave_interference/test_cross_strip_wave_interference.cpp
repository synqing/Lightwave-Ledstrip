// Phase 3 Move 3.2 — CrossStripWaveInterferenceEffect native contract tests.

#include <unity.h>
#include <cstddef>
#include <cstdint>

#include <FastLED.h>

#include "effects/ieffect/CrossStripWaveInterferenceEffect.h"
#include "plugins/api/EffectContext.h"

using lightwaveos::effects::ieffect::CrossStripWaveInterferenceEffect;
using lightwaveos::plugins::EffectRoleFlags;

namespace {

constexpr uint16_t kStripLen = 160;

CRGB g_stripA[kStripLen];
CRGB g_stripB[kStripLen];
CRGB g_unified[kStripLen * 2];

void clearBuffers() {
    for (uint16_t i = 0; i < kStripLen; ++i) {
        g_stripA[i] = CRGB(0, 0, 0);
        g_stripB[i] = CRGB(0, 0, 0);
    }
    for (uint16_t i = 0; i < kStripLen * 2; ++i) {
        g_unified[i] = CRGB(0, 0, 0);
    }
}

lightwaveos::plugins::EffectContext makeContext() {
    lightwaveos::plugins::EffectContext ctx{};
    ctx.leds = g_unified;
    ctx.ledCount = kStripLen * 2;
    ctx.stripLeds[0] = g_stripA;
    ctx.stripLeds[1] = g_stripB;
    ctx.stripLength = kStripLen;
    ctx.stripCount = 2;
    ctx.stripCenter = 79;
    ctx.dualChannelMode = false;
    ctx.brightness = 220;
    ctx.speed = 25;
    ctx.intensity = 220;
    ctx.rawDeltaTimeSeconds = 1.0f / 120.0f;
    ctx.deltaTimeSeconds = 1.0f / 120.0f;
    return ctx;
}

bool equalColour(const CRGB& a, const CRGB& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

uint16_t nonBlackCount(const CRGB* strip) {
    uint16_t count = 0;
    for (uint16_t i = 0; i < kStripLen; ++i) {
        if ((strip[i].r | strip[i].g | strip[i].b) != 0) {
            ++count;
        }
    }
    return count;
}

void test_metadata_declares_dual_channel() {
    CrossStripWaveInterferenceEffect effect;
    const auto& meta = effect.getMetadata();
    TEST_ASSERT_EQUAL_STRING("Cross-Strip Wave Interference", meta.name);
    TEST_ASSERT_TRUE((static_cast<uint8_t>(meta.roleFlags) &
                      static_cast<uint8_t>(EffectRoleFlags::DUAL_CHANNEL)) != 0);
}

void test_render_sets_dual_channel_and_writes_both_strips() {
    clearBuffers();
    CrossStripWaveInterferenceEffect effect;
    auto ctx = makeContext();
    TEST_ASSERT_TRUE(effect.init(ctx));

    effect.render(ctx);

    TEST_ASSERT_TRUE(ctx.dualChannelMode);
    TEST_ASSERT_GREATER_THAN_UINT16(120, nonBlackCount(g_stripA));
    TEST_ASSERT_GREATER_THAN_UINT16(120, nonBlackCount(g_stripB));
}

void test_default_render_is_centre_symmetric_per_strip() {
    clearBuffers();
    CrossStripWaveInterferenceEffect effect;
    auto ctx = makeContext();
    TEST_ASSERT_TRUE(effect.init(ctx));

    effect.render(ctx);

    for (uint16_t dist = 0; dist < 79; ++dist) {
        TEST_ASSERT_TRUE(equalColour(g_stripA[79 - dist], g_stripA[80 + dist]));
        TEST_ASSERT_TRUE(equalColour(g_stripB[79 - dist], g_stripB[80 + dist]));
    }
}

void test_default_phase_offset_makes_strips_visibly_different() {
    clearBuffers();
    CrossStripWaveInterferenceEffect effect;
    auto ctx = makeContext();
    TEST_ASSERT_TRUE(effect.init(ctx));

    effect.render(ctx);

    uint16_t different = 0;
    for (uint16_t i = 0; i < kStripLen; ++i) {
        if (!equalColour(g_stripA[i], g_stripB[i])) {
            ++different;
        }
    }
    TEST_ASSERT_GREATER_THAN_UINT16(120, different);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, effect.getParameter("phaseOffset"));
}

void test_phase_parameter_clamps_to_known_offsets() {
    CrossStripWaveInterferenceEffect effect;
    auto ctx = makeContext();
    TEST_ASSERT_TRUE(effect.init(ctx));

    TEST_ASSERT_TRUE(effect.setParameter("phaseOffset", 4.0f));
    TEST_ASSERT_EQUAL_FLOAT(4.0f, effect.getParameter("phaseOffset"));
    TEST_ASSERT_TRUE(effect.setParameter("phaseOffset", -100.0f));
    TEST_ASSERT_EQUAL_FLOAT(3.0f, effect.getParameter("phaseOffset"));
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_metadata_declares_dual_channel);
    RUN_TEST(test_render_sets_dual_channel_and_writes_both_strips);
    RUN_TEST(test_default_render_is_centre_symmetric_per_strip);
    RUN_TEST(test_default_phase_offset_makes_strips_visibly_different);
    RUN_TEST(test_phase_parameter_clamps_to_known_offsets);
    return UNITY_END();
}
