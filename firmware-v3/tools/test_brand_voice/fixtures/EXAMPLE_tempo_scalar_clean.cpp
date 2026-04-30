// SYNTHETIC TEST FIXTURE — compliant scalar-tempo render path.
// Expected verdict: PASS.
//
// Demonstrates the BRAND_VOICE_POSTURE.md §4.5 boundary case (tempo-bank as
// engine plumbing only; render-side reads scalar tempoPhase) AND the
// engine-plumbing escape for index-0 single-tempo (tempi[0] is a bank-of-1).

#include "TestStub.h"

class TempoScalarClean {
public:
    void render(EffectContext& ctx);
};

void TempoScalarClean::render(EffectContext& ctx) {
    const float phase   = ctx.audio.tempoPhase;       // OK — scalar
    const float scalar2 = ctx.audio.tempi[0].phase;   // OK — index-0 escape
    const uint8_t v     = uint8_t(phase * 255.0f);
    SET_CENTER_PAIR(ctx.leds, 79, 80, CRGB(v, 0, 0));
    (void)scalar2;
}
