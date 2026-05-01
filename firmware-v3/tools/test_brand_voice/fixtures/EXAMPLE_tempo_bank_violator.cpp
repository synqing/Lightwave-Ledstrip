// SYNTHETIC TEST FIXTURE — exercises check_tempo_bank_in_render rule.
// This file is NOT part of the effect catalogue; it lives under tools/test_brand_voice/
// so the main lint scanner does NOT pick it up.  The brand-voice test runner
// invokes the rule against this fixture directory explicitly.
//
// Expected verdict: FAIL with [tempo-bank-in-render] violation.
//
// BRAND_VOICE_POSTURE.md §3.5 — literal ES tempo-bank N-pendulum rendering banned.
// Block 2 item 19 — INF-05 TempoBank per-bin → KILL for V1.0.
//
// Note: render() is declared out-of-line as Class::render to match the existing
// RENDER_START_PATTERN convention used by check_heap_alloc_in_render.

#include "TestStub.h"

class TempoBankViolator {
public:
    void render(EffectContext& ctx);
};

void TempoBankViolator::render(EffectContext& ctx) {
    for (uint8_t i = 0; i < 8; ++i) {
        float phase = ctx.audio.tempi[i].phase;  // VIOLATION — per-bin tempo bank read in render()
        ctx.leds[i] = CRGB(uint8_t(phase * 255), 0, 0);
    }
}
