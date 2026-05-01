// SYNTHETIC TEST FIXTURE — exercises check_geo_kill_patterns rule (filename + class name).
// Expected verdict: FAIL with [geo-kill] violation on filename match AND class name match.
//
// BRAND_VOICE_POSTURE.md §3.6 — centre-origin violation kill.
// Block 2 item 20 — GEO-06 CircularRing / GEO-10 AsymmetricDriftOrigin → KILL.

#include "TestStub.h"

class CircularRingEffect {  // VIOLATION — class name matches GEO-06 kill pattern
public:
    void render(EffectContext& ctx) {
        for (uint16_t i = 0; i < 320; ++i) {
            const float angle = (float(i) / 320.0f) * 6.28318f;
            ctx.leds[i] = CRGB(uint8_t(127.0f + 127.0f * angle), 0, 0);
        }
    }
};
