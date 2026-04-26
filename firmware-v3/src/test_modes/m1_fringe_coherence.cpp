// =============================================================================
// M1 LGP Fringe-Coherence Measurement Pattern Generator — Implementation
// =============================================================================
//
// See m1_fringe_coherence.h for full documentation, gates, and constraints.
//
// Build inclusion: guarded by ENABLE_M1_FRINGE_COHERENCE. Without that flag
// this translation unit compiles to nothing and contributes zero footprint.
// =============================================================================

#include "m1_fringe_coherence.h"

#ifdef ENABLE_M1_FRINGE_COHERENCE

#include <math.h>

namespace lightwaveos {
namespace test_modes {

namespace {

// Two-pi as a float (matches std::sinf domain). Avoids dragging M_PI's macro
// surface and keeps the constant local to this TU.
constexpr float kTwoPi = 6.28318530717958647692f;

// Phase offsets for Δφ = φ_B - φ_A across the 5-pattern set.
// φ_A is held at 0 throughout; φ_B steps through {0, π/4, π/2, 3π/4, π}.
constexpr float kPhaseOffsetsB[kFringePatternCount] = {
    0.0f,                       // Δφ = 0     — constructive
    kTwoPi * 0.125f,            // Δφ = π/4   — slight diagonal
    kTwoPi * 0.25f,             // Δφ = π/2   — quadrature
    kTwoPi * 0.375f,            // Δφ = 3π/4  — near-destructive
    kTwoPi * 0.5f,              // Δφ = π     — destructive (alternating)
};

// Compute brightness[i] = base + amp * sin(2π · i / λ + phase).
// Clamps to [0, 255] for safety though the formula is bounded by construction.
inline uint8_t fringeSampleAt(uint8_t ledIndex, float phaseRad) noexcept {
    const float k = kTwoPi / static_cast<float>(kFringeWavelengthLeds);
    const float angle = k * static_cast<float>(ledIndex) + phaseRad;
    const float s = sinf(angle);
    const float v = static_cast<float>(kFringeBaseBrightness)
                  + static_cast<float>(kFringeAmplitude) * s;
    if (v < 0.0f)   return 0;
    if (v > 255.0f) return 255;
    return static_cast<uint8_t>(v + 0.5f);
}

}  // namespace

FringeCoherenceGenerator::FringeCoherenceGenerator() noexcept
    : m_colour(kFringeDefaultColour),
      m_pattern(0),
      m_msInPattern(0) {
    // Zero the LUTs deterministically. Will be filled by initialise().
    for (uint8_t i = 0; i < kFringeStripLength; ++i) {
        m_lutA[i] = 0;
        m_lutB[i] = 0;
    }
}

void FringeCoherenceGenerator::initialise() noexcept {
    m_pattern = 0;
    m_msInPattern = 0;
    recomputeLuts();
}

void FringeCoherenceGenerator::setPattern(uint8_t patternIndex) noexcept {
    if (patternIndex == kPatternUniformControl) {
        m_pattern = kPatternUniformControl;
    } else if (patternIndex >= kFringePatternCount) {
        m_pattern = 0;
    } else {
        m_pattern = patternIndex;
    }
    m_msInPattern = 0;
    recomputeLuts();
}

void FringeCoherenceGenerator::setColour(CRGB colour) noexcept {
    m_colour = colour;
}

uint8_t FringeCoherenceGenerator::tickAutoCycle(uint32_t dtMs) noexcept {
    m_msInPattern += dtMs;
    if (m_msInPattern >= kAutoCycleMs) {
        m_msInPattern = 0;
        // Auto-cycle skips the uniform-control sentinel — that pattern is
        // intended for explicit A/B trial dispatch, not the rotating exhibition.
        const uint8_t next = (m_pattern == kPatternUniformControl)
                                ? 0
                                : static_cast<uint8_t>((m_pattern + 1) % kFringePatternCount);
        m_pattern = next;
        recomputeLuts();
    }
    return m_pattern;
}

void FringeCoherenceGenerator::render(CRGB* ledsA, CRGB* ledsB) const noexcept {
    if (ledsA == nullptr || ledsB == nullptr) return;

    // Pure scalar multiply: per-LED brightness × colour. No heap, no audio,
    // no dt. Static pattern — well within the 2.0 ms render ceiling.
    for (uint8_t i = 0; i < kFringeStripLength; ++i) {
        const uint8_t bA = m_lutA[i];
        const uint8_t bB = m_lutB[i];
        ledsA[i] = m_colour;
        ledsA[i].nscale8(bA);
        ledsB[i] = m_colour;
        ledsB[i].nscale8(bB);
    }
}

void FringeCoherenceGenerator::recomputeLuts() noexcept {
    if (m_pattern == kPatternUniformControl) {
        // Uniform-brightness control: both strips at base brightness, no
        // modulation. This is the "indistinguishable target" for the A/B
        // forced-choice protocol.
        for (uint8_t i = 0; i < kFringeStripLength; ++i) {
            m_lutA[i] = kFringeBaseBrightness;
            m_lutB[i] = kFringeBaseBrightness;
        }
        return;
    }

    // φ_A is anchored at 0 across all patterns; only φ_B varies. This keeps
    // strip A as the reference and makes Δφ entirely attributable to strip B.
    const uint8_t idx = (m_pattern < kFringePatternCount) ? m_pattern : 0;
    const float phaseA = 0.0f;
    const float phaseB = kPhaseOffsetsB[idx];

    for (uint8_t i = 0; i < kFringeStripLength; ++i) {
        m_lutA[i] = fringeSampleAt(i, phaseA);
        m_lutB[i] = fringeSampleAt(i, phaseB);
    }
}

}  // namespace test_modes
}  // namespace lightwaveos

#endif  // ENABLE_M1_FRINGE_COHERENCE
