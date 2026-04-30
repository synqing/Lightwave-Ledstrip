// Pipeline-reform Phase 1+2 verification test runner.
//
// Aggregates the render-primitives + frame-blend native tests. Bypasses
// test_native pre-existing rot (Actor / RendererActor / ESP-IDF deps) by
// running with a tightly-scoped build_src_filter; see platformio.ini env
// `[env:native_test_render_primitives]`.
//
// To run:  pio test -e native_test_render_primitives

#include <unity.h>
#include <cstdio>

extern void run_render_primitives_tests();

void setUp(void)    { /* no-op */ }
void tearDown(void) { /* no-op */ }

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    printf("\n========== Pipeline Reform Phase 1+2 Test Suite ==========\n");
    printf("Layer 4 — Render Primitives (drawDot / drawSpriteScrolled / fillFromBins)\n");
    printf("Layer 5 — Frame Post-Process (applyFrameBlending)\n\n");

    printf("--- Render Primitives + Frame Blend ---\n");
    run_render_primitives_tests();

    return UNITY_END();
}
