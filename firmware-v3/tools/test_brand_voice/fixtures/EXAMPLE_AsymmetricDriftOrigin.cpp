// SYNTHETIC TEST FIXTURE — exercises check_geo_kill_patterns rule (GEO-10 AsymmetricDriftOrigin).
// Expected verdict: FAIL with [geo-kill] violation on filename match.
//
// BRAND_VOICE_POSTURE.md §3.6 — centre-origin violation kill (asymmetric drift).
// Block 2 item 20.

#include "TestStub.h"

class DriftOriginPlasma {
public:
    void render(EffectContext& ctx) {
        const uint16_t origin = 42;  // Non-centre origin — banned outside zone 0xFF.
        for (uint16_t i = 0; i < 320; ++i) {
            const int16_t d = int16_t(i) - int16_t(origin);
            ctx.leds[i] = CRGB(uint8_t(d & 0xFF), 0, 0);
        }
    }
};
