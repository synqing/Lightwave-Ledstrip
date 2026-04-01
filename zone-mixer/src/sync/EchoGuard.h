/**
 * EchoGuard — Per-parameter holdoff to prevent K1 broadcast snapback.
 *
 * When a local encoder change is sent to K1, the next K1 broadcast may
 * carry the OLD value (queued before K1 processed our update). The holdoff
 * timer suppresses incoming values for 1 second after a local change.
 *
 * Ported from Tab5 ParameterHandler holdoff pattern (proven in production).
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <Arduino.h>
#include "../config.h"

struct EchoGuard {
    static constexpr uint32_t HOLDOFF_MS = 1000;
    static constexpr uint8_t PARAM_COUNT = 20;  // InputManager::INPUT_COUNT

    uint32_t lastLocal[PARAM_COUNT] = {};
    bool initialSyncDone = false;

    /// Mark that a local change was sent for this input.
    void markLocal(uint8_t id) {
        if (id < PARAM_COUNT) lastLocal[id] = millis();
    }

    /// Check if incoming broadcast value should be suppressed.
    bool isHeld(uint8_t id, uint32_t now) const {
        if (id >= PARAM_COUNT) return false;
        return (now - lastLocal[id]) < HOLDOFF_MS;
    }

    /// Reset on WS reconnect — next status applies unconditionally.
    void resetSync() { initialSyncDone = false; }
};
