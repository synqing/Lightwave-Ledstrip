// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file AudioDebugConfig.h
 * @brief Runtime-configurable audio debug verbosity system
 *
 * Provides tiered debug levels (0-5) with CLI control via 'adbg' command.
 *
 * IMPORTANT: Levels are NOT additive. Higher levels don't include lower level output.
 * Each level enables specific categories of output.
 *
 * Levels (NEW design - v2.0):
 *   0 = Off      - No audio debug output (complete silence)
 *   1 = Errors   - Only actual errors (capture fail, init error)
 *   2 = Warnings - Errors + warnings (spike correction, stack low)
 *   3 = Info     - Warnings + one-time info (startup, shutdown)
 *   4 = Debug    - Info + periodic condensed status (~30s interval)
 *   5 = Trace    - Debug + DMA, spectrum, all periodic details
 *
 * One-Shot Commands (print regardless of level):
 *   adbg status   - Print health summary (mic level, RMS, AGC, spikes, saliency)
 *   adbg spectrum - Print 8-band + 64-bin spectrum + chroma
 *   adbg beat     - Print BPM, confidence, phase, lock state
 *
 * CLI Commands:
 *   adbg              - Show current level and interval
 *   adbg <0-5>        - Set verbosity level
 *   adbg interval <N> - Set base interval in frames
 */

#pragma once

#include <cstdint>

namespace lightwaveos {
namespace audio {

struct AudioDebugConfig {
    uint8_t verbosity = 2;        // 0-5, default Warnings (was 4)
    uint16_t baseInterval = 62;   // ~1 sec at 62.5 Hz frame rate

    // Derived intervals based on baseInterval
    uint16_t interval8Band() const { return baseInterval; }           // Level 5: ~1s
    uint16_t interval64Bin() const { return baseInterval / 2; }       // Level 5: ~0.5s (was level 4)
    uint16_t intervalDMA() const { return baseInterval * 5; }         // Level 5: ~5s (was level 3)
};

/**
 * @brief Get the global audio debug configuration
 * @return Reference to singleton config instance
 */
AudioDebugConfig& getAudioDebugConfig();

} // namespace audio
} // namespace lightwaveos
