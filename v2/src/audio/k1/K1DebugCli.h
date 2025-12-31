/**
 * @file K1DebugCli.h
 * @brief CLI commands for K1-Lightwave beat tracker debugging
 *
 * Provides Serial CLI commands and print functions for debugging the K1 beat
 * tracker. Gated by FEATURE_K1_DEBUG, which defaults to FEATURE_AUDIO_SYNC.
 *
 * Commands:
 *   k1      - Show BPM, confidence, phase, lock state, top-3 candidates
 *   k1s     - Full stats summary
 *   k1spec  - ASCII resonator spectrum (121 bins)
 *   k1nov   - Recent novelty z-scores
 *   k1reset - Reset K1 pipeline
 */

#pragma once

#include "../../config/features.h"

#if FEATURE_K1_DEBUG

#include <Arduino.h>
#include "K1Pipeline.h"
#include "K1Types.h"

namespace lightwaveos {
namespace audio {
namespace k1 {

// ============================================================================
// Print Functions (Tab5.DSP format)
// ============================================================================

/**
 * @brief Print compact beat line
 * Output: "BPM: 138.2 | Conf: 0.85 | Phase: 0.42 | LOCKED"
 */
void k1_print_compact(Print& out, const K1Pipeline& pipeline);

/**
 * @brief Print full diagnostic line with Top-3 candidates
 * Output: "BPM: 138.2 | Conf: 0.85 | Phase: 0.42 | LOCKED | Top3: 138(0.92) 69(0.45) 276(0.32)"
 */
void k1_print_full(Print& out, const K1Pipeline& pipeline);

/**
 * @brief Print beat tick line (call when beat_tick is true)
 * Output: ">>> BEAT <<< BPM: 138.2 Conf: 0.85"
 */
void k1_print_beat_tick(Print& out, const K1Pipeline& pipeline);

/**
 * @brief Print ASCII resonator spectrum (every 5 BPM)
 * Shows normalized magnitude bars for all 121 BPM bins
 */
void k1_print_spectrum(Print& out, const K1ResonatorFrame& rf);

/**
 * @brief Print detailed Goertzel bins around a target BPM
 * Shows ±10 BPM of the target with magnitudes and markers for top candidates
 * @param center_bpm The BPM to center the view on (default: 128)
 */
void k1_print_bins(Print& out, const K1ResonatorFrame& rf, int center_bpm = 128);

/**
 * @brief Print recent novelty z-scores with visual bars
 * @param count Number of recent frames to show (default: 20)
 */
void k1_print_novelty(Print& out, const K1ResonatorBank& resonators, int count = 20);

/**
 * @brief Print K1 stats summary
 */
void k1_print_stats(Print& out, const K1Pipeline& pipeline);

// ============================================================================
// CLI Command Handler
// ============================================================================

/**
 * @brief Handle K1 CLI commands
 * @param cmd Command string (lowercase, trimmed)
 * @param pipeline Reference to K1Pipeline
 * @return true if command was handled
 */
bool k1_handle_command(const String& cmd, K1Pipeline& pipeline);

} // namespace k1
} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_K1_DEBUG
