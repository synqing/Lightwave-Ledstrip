// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once
// ============================================================================
// I2CRecovery - I2C Error Counter for Tab5 External Bus
// ============================================================================
// Lightweight error tracking for the external I2C bus (Grove Port.A).
// The full recovery state machine was removed after hardware validation
// proved it unnecessary on healthy bus — single-shot encoder init suffices.
//
// What remains: error/success counting for STATUS line diagnostics.
//
// Usage:
//   I2CRecovery::init(&Wire, sda, scl, freq);
//   // In transport layer:
//   if (transmissionError) I2CRecovery::recordError();
//   else                   I2CRecovery::recordSuccess();
// ============================================================================

#include <Arduino.h>
#include <Wire.h>

class I2CRecovery {
public:
    /**
     * Initialise the error counter.
     * Stores bus parameters for diagnostics.
     */
    static void init(TwoWire* wire, int sda, int scl, uint32_t freq, uint16_t timeout = 50);

    /** Record an I2C error (increments consecutive error count). */
    static void recordError();

    /** Record a successful transaction (resets consecutive error count). */
    static void recordSuccess();

    /** Get current consecutive error count. */
    static uint8_t getErrorCount() { return s_errorCount; }

    /** Get lifetime successful recovery count (legacy — always 0). */
    static uint16_t getRecoverySuccesses() { return s_recoverySuccesses; }

private:
    static bool s_initialized;
    static uint8_t s_errorCount;
    static uint16_t s_recoverySuccesses;
};
