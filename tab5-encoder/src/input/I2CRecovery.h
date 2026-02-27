// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once
// ============================================================================
// I2CRecovery - ESP32-P4 I2C Bus Recovery with Hardware Reset for Tab5
// ============================================================================
// Multi-level I2C bus recovery with hardware peripheral reset capability.
//
// CRITICAL SAFETY NOTES:
// - This module operates ONLY on the EXTERNAL I2C bus (Grove Port.A)
// - Tab5's INTERNAL I2C bus is shared with display/touch/audio - NEVER touch it
// - Uses i2c_ll_reset_register() for P4-native hardware peripheral reset
// - Falls back to software bus clear if hardware reset unavailable
//
// Recovery Levels (escalating):
// Level 1 (Software): SCL toggling + STOP condition + Wire reinit
// Level 2 (Hardware): i2c_ll_reset_register() + Wire reinit
//
// Recovery Sequence:
// 1. Detect bus hang (SDA stuck low)
// 2. Toggle SCL 9-18 times to clock out stuck slave
// 3. Generate STOP condition (SDA low->high while SCL high)
// 4. If still failing: Hardware peripheral reset via i2c_ll_reset_register()
// 5. Reinitialize Wire with original parameters
//
// Usage:
//   I2CRecovery::init(&Wire, I2C::EXT_SDA_PIN, I2C::EXT_SCL_PIN, I2C::FREQ_HZ);
//   // In transport layer:
//   if (transmissionError) {
//       I2CRecovery::recordError();
//   }
//   // In main loop:
//   I2CRecovery::update();
// ============================================================================

#include <Arduino.h>
#include <Wire.h>

class I2CRecovery {
public:
    // ========================================================================
    // Configuration Constants
    // ========================================================================

    // Number of consecutive errors before triggering recovery
    static constexpr uint8_t ERROR_THRESHOLD = 5;

    // Cooldown after failed recovery before retrying (ms)
    static constexpr uint32_t RECOVERY_COOLDOWN_MS = 5000;

    // Minimum time between recovery attempts (ms)
    static constexpr uint32_t RECOVERY_MIN_INTERVAL_MS = 3000;

    // SCL toggle parameters
    static constexpr uint8_t SCL_TOGGLE_MIN = 9;   // I2C spec minimum
    static constexpr uint8_t SCL_TOGGLE_MAX = 18;  // Extra aggressive

    // Recovery step timing (non-blocking state machine)
    static constexpr uint32_t STEP_DELAY_SHORT_MS = 5;
    static constexpr uint32_t STEP_DELAY_MEDIUM_MS = 50;
    static constexpr uint32_t STEP_DELAY_LONG_MS = 100;
    static constexpr uint32_t STEP_DELAY_HW_RESET_MS = 200;  // After hardware reset

    // Escalation: Software recovery attempts before hardware reset
    static constexpr uint8_t SOFTWARE_ATTEMPTS_BEFORE_HW = 2;

    // ========================================================================
    // Initialization
    // ========================================================================

    /**
     * Initialize the I2C recovery module
     * Must be called after Wire.begin()
     *
     * @param wire Pointer to TwoWire instance (typically &Wire)
     * @param sda SDA GPIO pin number
     * @param scl SCL GPIO pin number
     * @param freq I2C clock frequency in Hz (for reinit)
     */
    static void init(TwoWire* wire, int sda, int scl, uint32_t freq);

    // ========================================================================
    // Error Tracking
    // ========================================================================

    /**
     * Record an I2C communication error
     * Call this when Wire.endTransmission() returns non-zero
     * or when device communication fails
     */
    static void recordError();

    /**
     * Record a successful communication
     * Call this on successful device reads/writes
     * Resets consecutive error count
     */
    static void recordSuccess();

    /**
     * Get current consecutive error count
     * @return Number of consecutive errors since last success
     */
    static uint8_t getErrorCount();

    // ========================================================================
    // Bus State Detection
    // ========================================================================

    /**
     * Check if SDA line is stuck low (indicating hung bus)
     * Temporarily sets SDA to INPUT_PULLUP and reads state
     *
     * @return true if SDA is held low (bus potentially hung)
     */
    static bool detectBusHang();

    /**
     * Check if bus appears healthy
     * SDA and SCL should both read HIGH when idle
     *
     * @return true if bus appears idle and healthy
     */
    static bool isBusHealthy();

    // ========================================================================
    // Recovery State Machine
    // ========================================================================

    /**
     * Attempt to trigger recovery if conditions are met
     * - Error count exceeds threshold
     * - Not currently recovering
     * - Cooldown period has elapsed
     *
     * @return true if recovery was triggered
     */
    static bool attemptRecovery();

    /**
     * Update recovery state machine (call from main loop)
     * Non-blocking - advances one step per call when recovering
     * Safe to call every loop iteration
     */
    static void update();

    /**
     * Check if recovery is currently in progress
     * @return true if recovery state machine is active
     */
    static bool isRecovering();

    /**
     * Force immediate recovery attempt
     * Bypasses error threshold check (use for manual recovery)
     */
    static void forceRecovery();

    // ========================================================================
    // Recovery Statistics
    // ========================================================================

    /**
     * Get total number of recovery attempts since boot
     * @return Recovery attempt count
     */
    static uint16_t getRecoveryAttempts();

    /**
     * Get number of successful recoveries since boot
     * @return Successful recovery count
     */
    static uint16_t getRecoverySuccesses();

    /**
     * Reset all statistics and error counts
     */
    static void resetStats();

private:
    // Recovery state machine stages
    enum class RecoveryStage : uint8_t {
        Idle = 0,           // Not recovering
        WireEnd1,           // First Wire.end() call
        WireEnd2,           // Second Wire.end() call (ensure release)
        PinRelease,         // Release pins to INPUT_PULLUP
        CheckSDA,           // Check if SDA is stuck low
        SclToggle,          // Toggle SCL to free stuck slave
        StopCondition1,     // Generate STOP: SDA low, SCL high
        StopCondition2,     // Generate STOP: SDA high (while SCL high)
        WaitAfterStop,      // Wait for bus to settle
        // Hardware reset stages (ESP32-P4 native)
        HwPeriphReset,      // Call i2c_ll_reset_register() inside PERIPH_RCC_ATOMIC()
        HwWaitAfterReset,   // Wait after hardware peripheral reset
        // Reinitialize
        WireBegin,          // Reinitialize Wire
        WaitAfterInit,      // Wait for Wire to stabilize
        Verify,             // Verify bus is healthy
        Complete            // Recovery complete
    };

    // Configuration (set by init())
    static TwoWire* s_wire;
    static int s_sda;
    static int s_scl;
    static uint32_t s_freq;
    static bool s_initialized;

    // Error tracking
    static uint8_t s_errorCount;
    static uint32_t s_lastErrorTime;

    // Recovery state
    static RecoveryStage s_stage;
    static uint32_t s_nextStepTime;
    static uint8_t s_sclToggleCount;
    static uint8_t s_stopAttempt;
    static uint8_t s_softwareAttempts;  // Escalation counter for hardware reset

    // Cooldown tracking
    static uint32_t s_lastRecoveryTime;
    static bool s_recoverySucceeded;

    // Statistics
    static uint16_t s_recoveryAttempts;
    static uint16_t s_recoverySuccesses;

    // ========================================================================
    // Internal Methods
    // ========================================================================

    /**
     * Execute software bus clear sequence (blocking)
     * Used internally by state machine
     * @param pulses Number of SCL pulses to send
     */
    static void executeBusClear(uint8_t pulses);

    /**
     * Generate I2C STOP condition
     */
    static void generateStopCondition();

    /**
     * Execute hardware peripheral reset via i2c_ll_reset_register()
     * ESP32-P4 native API - resets I2C peripheral at SOC level
     * @param port I2C port number (0 or 1)
     */
    static void executeHardwareReset(int port);

    /**
     * Advance to next recovery stage
     * @param stage Next stage
     * @param delayMs Delay before executing next stage
     */
    static void advanceTo(RecoveryStage stage, uint32_t delayMs);

    /**
     * Complete recovery with result
     * @param success true if recovery succeeded
     */
    static void completeRecovery(bool success);
};

// ============================================================================
// Inline Implementations (simple getters)
// ============================================================================

inline uint8_t I2CRecovery::getErrorCount() {
    return s_errorCount;
}

inline bool I2CRecovery::isRecovering() {
    return s_stage != RecoveryStage::Idle;
}

inline uint16_t I2CRecovery::getRecoveryAttempts() {
    return s_recoveryAttempts;
}

inline uint16_t I2CRecovery::getRecoverySuccesses() {
    return s_recoverySuccesses;
}
