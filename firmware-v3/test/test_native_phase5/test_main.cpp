// Phase 5 verification test runner.
//
// Aggregates ONLY tests that verify Phase 5 effects + their substrates,
// bypassing the pre-existing rot in test_native (RendererActor / Actor /
// ZoneComposer / WsCommandRouter dependencies that pull ESP-IDF and
// Arduino-only headers).
//
// To run:  pio test -e native_test_phase5

#include <unity.h>
#include <cstdio>

// ── Phase 5 effects ────────────────────────────────────────────────────────
extern void run_radial_time_scope_tests();             // Move 5.4 — LIN-06
extern void run_attack_only_pitch_velocity_tests();    // Move 5.6 — LIN-08
extern void run_psram_frame_ring_tests();              // Move 5.1 — INF-03 substrate

// ── Phase 4 audio substrates ──────────────────────────────────────────────
extern void run_first_light_ignition_tests();          // Move 4.4 — F6 boot
extern void run_audio_gated_decay_tests();             // Move 4.2 — PER-18
extern void run_voice_music_classifier_tests();        // Move 4.1 — AUD-21

// ── Phase 2 substrate ─────────────────────────────────────────────────────
extern void run_psram_scalar_ring_tests();             // Move 2.1 — INF-12

// ── Phase 1 substrates ────────────────────────────────────────────────────
extern void run_persistence_helpers_tests();           // Move 1.1
extern void run_effect_role_flags_tests();             // Move 1.2 — INF-06
extern void run_framebuffer_lpf_tests();               // Move 1.3 — INF-02
extern void run_layer_stack_tests();                   // Move 1.4 — INF-01
extern void run_control_bus_reuse_helpers_tests();     // Move 1.5
extern void run_math_substrate_tests();                // Move 1.6

void setUp(void)    { /* no-op */ }
void tearDown(void) { /* no-op */ }

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    printf("\n========== Phase 5 Verification Test Suite ==========\n\n");

    printf("--- Phase 1 Substrates ---\n");
    run_persistence_helpers_tests();
    run_effect_role_flags_tests();
    run_framebuffer_lpf_tests();
    run_layer_stack_tests();
    run_control_bus_reuse_helpers_tests();
    run_math_substrate_tests();

    printf("--- Phase 2 Substrate ---\n");
    run_psram_scalar_ring_tests();

    printf("--- Phase 4 Audio Substrates ---\n");
    run_voice_music_classifier_tests();
    run_audio_gated_decay_tests();
    run_first_light_ignition_tests();

    printf("--- Phase 5 Effects ---\n");
    run_psram_frame_ring_tests();
    run_radial_time_scope_tests();
    run_attack_only_pitch_velocity_tests();

    return UNITY_END();
}
