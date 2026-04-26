// SYNTHETIC TEST FIXTURE — continuum-class compliant.
// Expected verdict: PASS (no violation, no warning).
//
// Demonstrates BRAND_VOICE_POSTURE.md §4.3 boundary case — continuum-class
// PDEs over coupled-element physics.  Despite being a 320-cell coupled
// physics simulation (heat equation), the name and structure read as
// continuum, not fragmentation.

#include "TestStub.h"

class HeatEquationDiffusionEffect {
public:
    void render(EffectContext& ctx) {
        for (uint16_t i = 1; i < 319; ++i) {
            buffer_[i] = 0.5f * (buffer_[i - 1] + buffer_[i + 1]);
        }
        // Centre-pair render mapping
        SET_CENTER_PAIR(ctx.leds, 79, 80, CRGB(uint8_t(buffer_[79] * 255), 0, 0));
    }
private:
    float buffer_[320];
};
