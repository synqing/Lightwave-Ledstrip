/**
 * @file RhythmBank.cpp
 * @brief Implementation of rhythm-focused Goertzel bank
 *
 * @author LightwaveOS Team
 * @version 2.0.0 - Phase 2 Integration
 */

#include "RhythmBank.h"
#include "K1_GoertzelTables_16k.h"  // For kRhythmBins_16k_24
#include <cmath>
#include <algorithm>

namespace lightwaveos {
namespace audio {

// ============================================================================
// Configuration
// ============================================================================

namespace {
    constexpr uint16_t SAMPLE_RATE = 16000;
    constexpr float HOP_DURATION = 0.016f;  // 128 samples @ 16kHz = 16ms

    // AGC configuration (fast attack, slow release)
    constexpr float AGC_ATTACK_TIME = 0.010f;   // 10ms attack
    constexpr float AGC_RELEASE_TIME = 0.500f;  // 500ms release
    constexpr float AGC_TARGET_LEVEL = 0.7f;
    constexpr float AGC_MAX_GAIN = 1.0f;        // No boost (attenuation-only)

    // Noise floor configuration
    constexpr float NOISE_TIME_CONSTANT = 1.0f;  // 1 second
}

// ============================================================================
// Helper function to convert GoertzelBinSpec to GoertzelConfig
// ============================================================================

static void convertToGoertzelConfigs(const GoertzelBinSpec* specs, GoertzelConfig* configs, uint8_t numBins) {
    for (uint8_t i = 0; i < numBins; i++) {
        configs[i].freq_hz = specs[i].freq_hz;
        configs[i].windowSize = specs[i].N;
        configs[i].coeff_q14 = specs[i].coeff_q14;
    }
}

// ============================================================================
// Constructor
// ============================================================================

RhythmBank::RhythmBank()
    : m_goertzel(NUM_BINS, nullptr)  // Will init properly below
    , m_noiseFloor(NOISE_TIME_CONSTANT, NUM_BINS)
    , m_agc(AGC_ATTACK_TIME, AGC_RELEASE_TIME, AGC_TARGET_LEVEL)
    , m_noveltyFlux(NUM_BINS)
    , m_flux(0.0f)
{
    // Convert kRhythmBins_16k_24 to GoertzelConfig array
    static GoertzelConfig configs[NUM_BINS];
    convertToGoertzelConfigs(kRhythmBins_16k_24, configs, NUM_BINS);

    // Reconstruct GoertzelBank with proper config
    m_goertzel.~GoertzelBank();
    new (&m_goertzel) GoertzelBank(NUM_BINS, configs);

    // Set AGC max gain (attenuation-only for rhythm)
    m_agc.setMaxGain(AGC_MAX_GAIN);

    // Clear output buffers
    memset(m_magnitudes, 0, sizeof(m_magnitudes));
}

// ============================================================================
// Processing
// ============================================================================

void RhythmBank::process(const AudioRingBuffer<float, 2048>& ringBuffer) {
    // Step 1: Run Goertzel bank on ring buffer
    m_goertzel.compute(ringBuffer, m_magnitudes);

    // Step 2: Update noise floor estimates (per-bin)
    for (uint8_t i = 0; i < NUM_BINS; i++) {
        m_noiseFloor.update(i, m_magnitudes[i]);
    }

    // Step 3: Subtract noise floor from magnitudes
    for (uint8_t i = 0; i < NUM_BINS; i++) {
        float floor = m_noiseFloor.getFloor(i);
        m_magnitudes[i] = std::max(0.0f, m_magnitudes[i] - floor);
    }

    // Step 4: Apply AGC (fast attack, slow release)
    // Find peak magnitude
    float peak = 0.0f;
    for (uint8_t i = 0; i < NUM_BINS; i++) {
        if (m_magnitudes[i] > peak) {
            peak = m_magnitudes[i];
        }
    }

    // Update AGC with peak
    m_agc.update(peak, HOP_DURATION);

    // Apply gain to all magnitudes
    float gain = m_agc.getGain();
    for (uint8_t i = 0; i < NUM_BINS; i++) {
        m_magnitudes[i] *= gain;
    }

    // Step 5: Compute spectral flux (onset detection)
    m_flux = m_noveltyFlux.compute(m_magnitudes, m_noiseFloor);
}

} // namespace audio
} // namespace lightwaveos
