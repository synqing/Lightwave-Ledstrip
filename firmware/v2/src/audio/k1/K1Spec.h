/**
 * @file K1Spec.h
 * @brief K1 Dual-Bank Goertzel Front-End - Locked Configuration Constants
 *
 * These constants are frozen and must not be changed without updating
 * all dependent code. They define the core timing and structure of the
 * K1 audio processing pipeline.
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#pragma once

#include <cstdint>

namespace lightwaveos {
namespace audio {
namespace k1 {

// ============================================================================
// Sample Rate and Hop Configuration
// ============================================================================

/**
 * @brief Sample rate in Hz
 * 
 * Fixed at 16kHz for K1 architecture.
 */
constexpr uint32_t FS_HZ = 16000;

/**
 * @brief Hop size in samples
 * 
 * Each hop is 128 samples = 8.0 ms at 16kHz.
 */
constexpr uint16_t HOP_SAMPLES = 128;

/**
 * @brief Hop rate in Hz
 * 
 * 125 Hz = 8.0 ms per hop.
 */
constexpr float HOP_HZ = 125.0f;

/**
 * @brief Harmony tick divisor
 * 
 * HarmonyBank computes every 2 hops (every 16.0 ms = 62.5 Hz).
 */
constexpr uint8_t HARMONY_TICK_DIV = 2;

// ============================================================================
// Window Length Constraints
// ============================================================================

/**
 * @brief Minimum window length in samples
 * 
 * All Goertzel bins must use N >= 256 samples.
 */
constexpr uint16_t N_MIN = 256;

/**
 * @brief Maximum window length in samples
 * 
 * All Goertzel bins must use N <= 1536 samples.
 */
constexpr uint16_t N_MAX = 1536;

// ============================================================================
// Bank Configuration
// ============================================================================

/**
 * @brief Number of harmony bins
 * 
 * HarmonyBank: 64 semitone bins from A2 (110 Hz) to C8 (~4186 Hz).
 */
constexpr uint8_t HARMONY_BINS = 64;

/**
 * @brief Number of rhythm bins
 * 
 * RhythmBank: 24 evidence bins for kick/snare/presence/transients.
 */
constexpr uint8_t RHYTHM_BINS = 24;

// ============================================================================
// Derived Timing Constants
// ============================================================================

/**
 * @brief Hop duration in milliseconds
 */
constexpr float HOP_DURATION_MS = (HOP_SAMPLES * 1000.0f) / FS_HZ;  // 8.0 ms

/**
 * @brief Harmony tick interval in milliseconds
 */
constexpr float HARMONY_TICK_MS = HOP_DURATION_MS * HARMONY_TICK_DIV;  // 16.0 ms

/**
 * @brief Harmony tick rate in Hz
 */
constexpr float HARMONY_TICK_HZ = static_cast<float>(FS_HZ) / static_cast<float>(HOP_SAMPLES * HARMONY_TICK_DIV);  // 62.5 Hz

} // namespace k1
} // namespace audio
} // namespace lightwaveos

