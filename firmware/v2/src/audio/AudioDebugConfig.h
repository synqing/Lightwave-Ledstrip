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
#ifndef NATIVE_BUILD
#include <Arduino.h>  // For millis()
#endif

namespace lightwaveos {
namespace audio {

struct AudioDebugConfig {
    uint8_t verbosity = 1;        // 0-5, default Minimal (Errors only)
    uint16_t baseInterval = 62;   // ~1 sec at 62.5 Hz frame rate

    // Derived intervals based on baseInterval
    uint16_t interval8Band() const { return baseInterval; }           // Level 5: ~1s
    uint16_t interval64Bin() const { return baseInterval * 5; }        // Level 4: ~5s (10x slower)
    uint16_t intervalDMA() const { return baseInterval * 5; }           // Level 3: ~5s
    
    // Rate limiting: minimum milliseconds between prints per verbosity level
    // Prevents serial monitor spam
    uint32_t getMinIntervalMs(uint8_t level) const {
        switch (level) {
            case 1: return 4000;  // Errors: 4 seconds (non-time-sensitive)
            case 2: return 2000;   // Status: 2 seconds
            case 3: return 2000;   // Low: 2 seconds
            case 4: return 1500;   // Medium: 1.5 seconds
            case 5: return 1000;   // High: 1 second (most frequent)
            default: return 2000;  // Default: 2 seconds
        }
    }
    
    // Last print timestamps per verbosity level (for rate limiting)
    mutable uint32_t lastPrintMs[6] = {0, 0, 0, 0, 0, 0};
    
    // Check if enough time has passed to allow printing at this verbosity level
    bool shouldPrint(uint8_t level) const {
        if (level == 0 || level > 5) return false;
        uint32_t now = millis();
        uint32_t minInterval = getMinIntervalMs(level);
        if (now - lastPrintMs[level] >= minInterval) {
            lastPrintMs[level] = now;
            return true;
        }
        return false;
    }
};

/**
 * @brief Get the global audio debug configuration
 * @return Reference to singleton config instance
 */
AudioDebugConfig& getAudioDebugConfig();

} // namespace audio
} // namespace lightwaveos
