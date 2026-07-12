// =============================================================================
// M1 LGP Fringe-Coherence Measurement Pattern Generator
// =============================================================================
//
// Static dual-strip phase-offset fringe pattern generator for the M1 measurement
// campaign documented in `firmware-v3/docs/measurement_protocols/m1_lgp_fringe.md`.
//
// Purpose: empirically gate the F4 Cross-Strip Wave Interference launch claim
// and the V1.0 marketing copy "physical interference" wording. Renders one of
// N static phase-offset fringe patterns to the dual strip when activated, so a
// calibrated photometer + naïve viewer panel can A/B-test fringe visibility
// against a uniform-brightness control under K1 V2 LGP optics.
//
// Gates:
//   - docs/research/synergy-topology/Topology_Reconciliation.md §3 [C-5], §6 item 7
//   - docs/research/synergy-topology/PASS_4_ADVERSARIAL_STRESS_TEST.md §1 [A-05] + §4.1
//
// Hard constraints:
//   - Centre-origin invariant: BOTH strips render the same wavelength λ; the
//     resulting fringe is symmetric about LED 79/80 by construction (each strip
//     is sampled with the same per-LED index function, so the centre of the
//     panel is the symmetry axis).
//   - No rainbows: amplitude-modulated white only (or palette-locked single
//     colour). No hue cycling.
//   - 2.0 ms render ceiling: pattern is static — once the LUT is computed at
//     activation, render() is an O(N) memcpy-equivalent. Trivially within budget.
//   - British English: comments use centre, colour, behaviour, initialise.
//
// Build inclusion:
//   This translation unit is guarded by `ENABLE_M1_FRINGE_COHERENCE`. Captain
//   commissions an M1 measurement campaign by adding `-D ENABLE_M1_FRINGE_COHERENCE`
//   to a dedicated platformio.ini env (NOT done in this commit — Captain's call).
//   Without that flag, m1_fringe_coherence.cpp compiles to an empty translation
//   unit and adds zero footprint.
//
// Integration coupling:
//   sinLUT256 (SSA-2 in-flight) is intentionally NOT used. We use std::sinf()
//   at activation only (one-shot LUT precompute, ~320 calls, sub-ms one-time
//   cost). Decoupled from in-flight work per task brief.
//
// =============================================================================

#pragma once

#ifdef ENABLE_M1_FRINGE_COHERENCE

#include <FastLED.h>
#include <stdint.h>

namespace lightwaveos {
namespace test_modes {

// -----------------------------------------------------------------------------
// Pattern set — 5 phase offsets (Δφ = φ_B - φ_A)
// -----------------------------------------------------------------------------
//
// Strip A: brightness[i] = 127 + 127 * sin(2π · i / λ + φ_A)
// Strip B: brightness[i] = 127 + 127 * sin(2π · i / λ + φ_B)
//
// φ_A is held at 0 across all patterns; φ_B advances in π/4 increments.
// Wavelength λ = kFringeWavelengthLeds (default 10 LEDs).
//
//   Index 0:  Δφ = 0       — constructive (uniform reinforcement, both strips identical)
//   Index 1:  Δφ = π/4     — slight diagonal phase
//   Index 2:  Δφ = π/2     — quadrature (diagonal phase)
//   Index 3:  Δφ = 3π/4    — near-destructive
//   Index 4:  Δφ = π       — destructive (alternating dark/light)
//
// A 6th "uniform control" pattern (kPatternUniformControl) is provided for the
// A/B forced-choice protocol — both strips render at constant 127 brightness.
//
// All patterns hold the chosen colour palette (default = white). No hue sweep.
// -----------------------------------------------------------------------------

constexpr uint8_t  kFringePatternCount      = 5;     // 5 phase offsets cycled
constexpr uint8_t  kFringeWavelengthLeds    = 10;    // λ ≈ 10 LEDs (one period)
constexpr uint8_t  kFringeStripLength       = 160;   // K1 single-strip length
constexpr uint8_t  kFringeBaseBrightness    = 127;   // DC offset
constexpr uint8_t  kFringeAmplitude         = 127;   // AC amplitude (peak ±127)

// Sentinel pattern index — uniform-brightness control for forced-choice trials.
// Renders both strips at constant kFringeBaseBrightness with no modulation.
constexpr uint8_t  kPatternUniformControl   = 0xFE;

// Default colour for the amplitude-modulated rendering. White preserves the
// "amplitude-modulated single colour" constraint while maximising luminance
// dynamic range for the photometer.
constexpr CRGB     kFringeDefaultColour     = CRGB::White;

// -----------------------------------------------------------------------------
// FringeCoherenceGenerator
// -----------------------------------------------------------------------------
//
// Lightweight pattern generator. Holds two precomputed brightness LUTs (one per
// strip, 160 bytes each = 320 B total) and rewrites them on pattern change.
// render() simply multiplies the per-LED brightness against the chosen colour
// and writes to the dual-strip CRGB buffer.
//
// Lifecycle:
//   - Construct with default state (pattern 0, white, 10-LED wavelength).
//   - Call setPattern(idx) to switch — recomputes LUTs (~320 sinf calls).
//   - Call render(ledsA, ledsB) every frame — pure copy, no audio dependency.
//
// Activation modes (Captain picks one — simplest = compile-time):
//   1. Compile-time: -D ENABLE_M1_FRINGE_COHERENCE -D M1_FIXED_PATTERN=2
//   2. Runtime API:  setPattern(idx) hooked to a serial command or REST endpoint
//   3. Auto-cycle:   call tickAutoCycle(dtMs) each frame; rotates every 15s
//
// All three modes are supported by the generator; choice of dispatch is left
// to the integrating environment (see protocol doc §4 procedure).
// -----------------------------------------------------------------------------
class FringeCoherenceGenerator {
public:
    FringeCoherenceGenerator() noexcept;

    // Initialise LUTs for the default pattern (index 0, Δφ = 0).
    // Cheap one-shot — call once after construction.
    void initialise() noexcept;

    // Switch to a specific pattern index (0..kFringePatternCount-1) or the
    // uniform-brightness control (kPatternUniformControl). Recomputes LUTs.
    // Out-of-range indices are clamped to 0.
    void setPattern(uint8_t patternIndex) noexcept;

    // Override the rendering colour. White is recommended for the photometer
    // measurement; a single non-white colour is acceptable for viewer-panel
    // trials if the panel reports any colour-dependent confounds.
    void setColour(CRGB colour) noexcept;

    // Auto-cycle dispatch — call once per frame with the elapsed milliseconds.
    // Advances to the next pattern every kAutoCycleMs (default 15000). Returns
    // the current pattern index so callers can log/monitor.
    uint8_t tickAutoCycle(uint32_t dtMs) noexcept;

    // Render one frame to both strips. ledsA and ledsB are CRGB buffers of
    // length kFringeStripLength. Static pattern — no dt or audio inputs needed.
    // Per the centre-origin invariant: both strips are sampled identically by
    // index, so the visible fringe is symmetric across the panel centre.
    void render(CRGB* ledsA, CRGB* ledsB) const noexcept;

    // Inspectors (for telemetry / serial debug).
    uint8_t currentPattern() const noexcept { return m_pattern; }
    uint32_t timeInPatternMs() const noexcept { return m_msInPattern; }

private:
    // Precomputed brightness for each strip across the 160-LED span.
    // 8-bit unsigned; populated by recomputeLuts() per pattern.
    uint8_t m_lutA[kFringeStripLength];
    uint8_t m_lutB[kFringeStripLength];

    CRGB     m_colour;
    uint8_t  m_pattern;
    uint32_t m_msInPattern;

    // Auto-cycle dwell time per pattern (≥10s required by protocol; chose 15s
    // to give the viewer panel comfortable judgement headroom).
    static constexpr uint32_t kAutoCycleMs = 15000;

    // Recompute m_lutA / m_lutB from current m_pattern. Called by setPattern().
    void recomputeLuts() noexcept;
};

}  // namespace test_modes
}  // namespace lightwaveos

#endif  // ENABLE_M1_FRINGE_COHERENCE
