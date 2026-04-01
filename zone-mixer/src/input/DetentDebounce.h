/**
 * DetentDebounce — Normalises M5Rotate8 encoder steps.
 * Ported from Tab5 EncoderProcessing.h.
 *
 * M5Rotate8 reports ±2 per mechanical detent, but occasionally ±1 due to
 * polling timing. This normalises all output to clean ±1 per detent.
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <cstdint>
#include <cstdlib>

struct DetentDebounce {
    static constexpr uint32_t INTERVAL_MS = 60;  // Min 60ms between emits

    int32_t pending = 0;
    uint32_t lastEmit = 0;
    bool expectingPair = false;

    /// Process a raw encoder delta. Returns true if a normalised step is ready.
    bool process(int32_t raw, uint32_t now) {
        if (raw == 0) return false;

        if (abs(raw) >= 2) {
            // Standard detent (±2) → normalise to ±1
            pending = (raw > 0) ? 1 : -1;
            expectingPair = false;
            return (now - lastEmit >= INTERVAL_MS);
        }

        // Raw ±1: half-detent. Wait for matching pair.
        if (!expectingPair) {
            pending = raw;
            expectingPair = true;
            return false;
        }

        // Second half arrived
        if ((pending > 0 && raw > 0) || (pending < 0 && raw < 0)) {
            pending = (pending > 0) ? 1 : -1;
            expectingPair = false;
            return (now - lastEmit >= INTERVAL_MS);
        }

        // Direction changed — restart
        pending = raw;
        return false;
    }

    /// Consume the normalised delta (call after process() returns true).
    int32_t consume() {
        lastEmit = millis();
        int32_t d = pending;
        pending = 0;
        return d;
    }
};
