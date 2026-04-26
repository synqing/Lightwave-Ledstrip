// SYNTHETIC TEST FIXTURE — exercises check_fragmentation_patterns rule (item 15 boundary-flag).
// Expected verdict: WARN (not FAIL) with [fragmentation-warn] — reviewer must classify PASS / KILL.
//
// BRAND_VOICE_POSTURE.md §3.3 — multi-element fragmentation kill (Pendulum chain).
// Block 2 item 15 — KILL / boundary-flag depending on certainty.
//
// This pattern is naming-convention-dependent — the warning surfaces it for
// human review.  A continuum-class implementation that happens to use a
// fragmentation-style name should be renamed or allowlisted.

#include "TestStub.h"

struct Pendulum {
    float angle;
    float velocity;
    uint16_t led_pos;
};

class PendulumChainEffect {  // FLAG — class name matches §3.3 fragmentation pattern
public:
    void render(EffectContext& ctx) {
        for (uint8_t i = 0; i < 16; ++i) {
            ctx.leds[pendulums_[i].led_pos] = CRGB(255, 0, 0);  // 16 visible elements
        }
    }
private:
    Pendulum pendulums_[16];
};
