/**
 * @file AudioDebugConfig.h
 * @brief Runtime-configurable audio debug verbosity system
 *
 * Provides tiered debug levels (0-5) with CLI control via 'adbg' command.
 *
 * Levels:
 *   0 = Off      - No audio debug output
 *   1 = Minimal  - Errors only
 *   2 = Status   - 10-second health (Audio alive, mic dB, Spike stats)
 *   3 = Low      - + DMA diagnostics (~5s interval)
 *   4 = Medium   - + 64-bin Goertzel (~2s interval)
 *   5 = High     - + 8-band Goertzel with full detail (~1s interval)
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
    uint8_t verbosity = 4;        // 0-5, default Medium
    uint16_t baseInterval = 62;   // ~1 sec at 62.5 Hz frame rate

    // Derived intervals based on baseInterval
    uint16_t interval8Band() const { return baseInterval; }           // Level 5: ~1s
    uint16_t interval64Bin() const { return baseInterval * 5; }        // Level 4: ~5s (10x slower)
    uint16_t intervalDMA() const { return baseInterval * 5; }           // Level 3: ~5s
};

/**
 * @brief Get the global audio debug configuration
 * @return Reference to singleton config instance
 */
AudioDebugConfig& getAudioDebugConfig();

} // namespace audio
} // namespace lightwaveos
