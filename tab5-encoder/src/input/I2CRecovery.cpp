// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
// ============================================================================
// I2CRecovery - ESP32-P4 I2C Bus Recovery with Hardware Reset
// ============================================================================
// Multi-level I2C bus recovery for Tab5's external I2C bus.
//
// This implementation uses BOTH software and hardware recovery:
// - Level 1: SCL toggling + STOP condition + Wire reinit (software)
// - Level 2: i2c_ll_reset_register() hardware peripheral reset (P4 native)
//
// ESP32-P4 uses new RCC API instead of deprecated periph_module_reset():
// - periph_module_reset() is NOT functional on P4
// - i2c_ll_reset_register() provides direct hardware reset
// - PERIPH_RCC_ATOMIC() macro ensures thread-safe register access
// ============================================================================

#include "I2CRecovery.h"

// ESP32-P4 hardware reset includes
#include "hal/i2c_ll.h"
#include "esp_private/periph_ctrl.h"

// ============================================================================
// Static Member Definitions
// ============================================================================

TwoWire* I2CRecovery::s_wire = nullptr;
int I2CRecovery::s_sda = -1;
int I2CRecovery::s_scl = -1;
uint32_t I2CRecovery::s_freq = 100000;
bool I2CRecovery::s_initialized = false;

uint8_t I2CRecovery::s_errorCount = 0;
uint32_t I2CRecovery::s_lastErrorTime = 0;

I2CRecovery::RecoveryStage I2CRecovery::s_stage = RecoveryStage::Idle;
uint32_t I2CRecovery::s_nextStepTime = 0;
uint8_t I2CRecovery::s_sclToggleCount = 0;
uint8_t I2CRecovery::s_stopAttempt = 0;
uint8_t I2CRecovery::s_softwareAttempts = 0;

uint32_t I2CRecovery::s_lastRecoveryTime = 0;
bool I2CRecovery::s_recoverySucceeded = false;

uint16_t I2CRecovery::s_recoveryAttempts = 0;
uint16_t I2CRecovery::s_recoverySuccesses = 0;

// ============================================================================
// Initialization
// ============================================================================

void I2CRecovery::init(TwoWire* wire, int sda, int scl, uint32_t freq) {
    s_wire = wire;
    s_sda = sda;
    s_scl = scl;
    s_freq = freq;
    s_initialized = true;

    // Reset all state
    s_errorCount = 0;
    s_lastErrorTime = 0;
    s_stage = RecoveryStage::Idle;
    s_nextStepTime = 0;
    s_sclToggleCount = 0;
    s_stopAttempt = 0;
    s_softwareAttempts = 0;
    s_lastRecoveryTime = 0;
    s_recoverySucceeded = false;
    s_recoveryAttempts = 0;
    s_recoverySuccesses = 0;

    Serial.printf("[I2C_RECOVERY] Initialized: SDA=%d SCL=%d freq=%luHz\n",
                  sda, scl, (unsigned long)freq);
}

// ============================================================================
// Error Tracking
// ============================================================================

void I2CRecovery::recordError() {
    if (!s_initialized) return;

    s_errorCount++;
    s_lastErrorTime = millis();

    // Log when approaching threshold
    if (s_errorCount >= ERROR_THRESHOLD - 1) {
        Serial.printf("[I2C_RECOVERY] Error count: %d/%d\n",
                      s_errorCount, ERROR_THRESHOLD);
    }
}

void I2CRecovery::recordSuccess() {
    if (!s_initialized) return;

    // Reset error count on successful communication
    if (s_errorCount > 0) {
        s_errorCount = 0;
    }
}

// ============================================================================
// Bus State Detection
// ============================================================================

bool I2CRecovery::detectBusHang() {
    if (!s_initialized) return false;

    // Temporarily release the bus and check SDA state
    // A healthy idle bus has both SDA and SCL high (pull-ups)
    // A hung bus typically has SDA stuck low

    // Save current state and release pins
    pinMode(s_sda, INPUT_PULLUP);
    pinMode(s_scl, INPUT_PULLUP);
    delayMicroseconds(10);  // Allow lines to settle

    // Read SDA state
    bool sdaLow = (digitalRead(s_sda) == LOW);

    // If SDA is stuck low, the bus is hung
    if (sdaLow) {
        Serial.println("[I2C_RECOVERY] Bus hang detected: SDA stuck LOW");
    }

    return sdaLow;
}

bool I2CRecovery::isBusHealthy() {
    if (!s_initialized) return true;  // Assume healthy if not initialized

    // Release pins and check both lines
    pinMode(s_sda, INPUT_PULLUP);
    pinMode(s_scl, INPUT_PULLUP);
    delayMicroseconds(10);

    bool sdaHigh = (digitalRead(s_sda) == HIGH);
    bool sclHigh = (digitalRead(s_scl) == HIGH);

    return sdaHigh && sclHigh;
}

// ============================================================================
// Recovery Triggering
// ============================================================================

bool I2CRecovery::attemptRecovery() {
    if (!s_initialized) return false;

    // Don't start if already recovering
    if (s_stage != RecoveryStage::Idle) {
        return false;
    }

    // Check error threshold
    if (s_errorCount < ERROR_THRESHOLD) {
        return false;
    }

    // Check cooldown
    uint32_t now = millis();
    if (s_lastRecoveryTime != 0 &&
        (now - s_lastRecoveryTime) < RECOVERY_COOLDOWN_MS) {
        return false;
    }

    // Check minimum interval
    if (s_lastRecoveryTime != 0 &&
        (now - s_lastRecoveryTime) < RECOVERY_MIN_INTERVAL_MS) {
        return false;
    }

    // Start recovery
    forceRecovery();
    return true;
}

void I2CRecovery::forceRecovery() {
    if (!s_initialized) return;

    // Don't restart if already recovering
    if (s_stage != RecoveryStage::Idle) {
        return;
    }

    Serial.println("[I2C_RECOVERY] Starting recovery sequence...");

    s_recoveryAttempts++;
    s_lastRecoveryTime = millis();
    s_recoverySucceeded = false;
    s_sclToggleCount = 0;
    s_stopAttempt = 0;

    // Start state machine
    advanceTo(RecoveryStage::WireEnd1, 0);
}

void I2CRecovery::resetStats() {
    s_errorCount = 0;
    s_lastErrorTime = 0;
    s_recoveryAttempts = 0;
    s_recoverySuccesses = 0;
    s_lastRecoveryTime = 0;
}

// ============================================================================
// Recovery State Machine
// ============================================================================

void I2CRecovery::update() {
    if (!s_initialized) return;

    // Check if we should auto-trigger recovery
    if (s_stage == RecoveryStage::Idle) {
        attemptRecovery();
        return;
    }

    // Non-blocking: Check if it's time for next step
    uint32_t now = millis();
    if (now < s_nextStepTime) {
        return;  // Not yet time
    }

    // Execute current stage
    switch (s_stage) {
        case RecoveryStage::WireEnd1:
            // First Wire.end() to release bus
            if (s_wire) {
                s_wire->end();
            }
            advanceTo(RecoveryStage::WireEnd2, STEP_DELAY_MEDIUM_MS);
            break;

        case RecoveryStage::WireEnd2:
            // Second Wire.end() for thorough release (paranoia)
            if (s_wire) {
                s_wire->end();
            }
            advanceTo(RecoveryStage::PinRelease, STEP_DELAY_SHORT_MS);
            break;

        case RecoveryStage::PinRelease:
            // Set pins to input with pull-up
            pinMode(s_sda, INPUT_PULLUP);
            pinMode(s_scl, INPUT_PULLUP);
            advanceTo(RecoveryStage::CheckSDA, STEP_DELAY_SHORT_MS);
            break;

        case RecoveryStage::CheckSDA:
            // Check if SDA is stuck low
            if (digitalRead(s_sda) == LOW) {
                Serial.println("[I2C_RECOVERY] SDA stuck low - toggling SCL");
                s_sclToggleCount = 0;
                advanceTo(RecoveryStage::SclToggle, STEP_DELAY_SHORT_MS);
            } else {
                // SDA is already high, just send STOP and reinit
                Serial.println("[I2C_RECOVERY] SDA not stuck - sending STOP");
                advanceTo(RecoveryStage::StopCondition1, STEP_DELAY_SHORT_MS);
            }
            break;

        case RecoveryStage::SclToggle:
            // Toggle SCL to clock out stuck slave
            // Do this in batches to stay non-blocking
            {
                // Configure SCL as output
                pinMode(s_scl, OUTPUT_OPEN_DRAIN);
                digitalWrite(s_scl, HIGH);
                delayMicroseconds(5);

                // Toggle SCL up to 3 times per update call
                for (uint8_t i = 0; i < 3 && s_sclToggleCount < SCL_TOGGLE_MAX; i++) {
                    digitalWrite(s_scl, LOW);
                    delayMicroseconds(5);
                    digitalWrite(s_scl, HIGH);
                    delayMicroseconds(5);
                    s_sclToggleCount++;

                    // Check if SDA is released
                    pinMode(s_sda, INPUT_PULLUP);
                    delayMicroseconds(2);
                    if (digitalRead(s_sda) == HIGH) {
                        Serial.printf("[I2C_RECOVERY] SDA released after %d SCL pulses\n",
                                      s_sclToggleCount);
                        advanceTo(RecoveryStage::StopCondition1, STEP_DELAY_SHORT_MS);
                        return;
                    }
                }

                // Check if we've done enough toggles
                if (s_sclToggleCount >= SCL_TOGGLE_MAX) {
                    Serial.println("[I2C_RECOVERY] Max SCL toggles reached");
                    advanceTo(RecoveryStage::StopCondition1, STEP_DELAY_SHORT_MS);
                } else {
                    // Continue toggling next update
                    advanceTo(RecoveryStage::SclToggle, STEP_DELAY_SHORT_MS);
                }
            }
            break;

        case RecoveryStage::StopCondition1:
            // Generate STOP condition: First set SDA low while SCL high
            pinMode(s_sda, OUTPUT_OPEN_DRAIN);
            pinMode(s_scl, OUTPUT_OPEN_DRAIN);
            digitalWrite(s_scl, HIGH);
            delayMicroseconds(2);
            digitalWrite(s_sda, LOW);  // SDA low while SCL high
            delayMicroseconds(5);
            advanceTo(RecoveryStage::StopCondition2, STEP_DELAY_SHORT_MS);
            break;

        case RecoveryStage::StopCondition2:
            // Generate STOP condition: Then set SDA high while SCL high
            digitalWrite(s_sda, HIGH);  // SDA low->high transition = STOP
            delayMicroseconds(5);

            // Release both lines
            pinMode(s_sda, INPUT_PULLUP);
            pinMode(s_scl, INPUT_PULLUP);

            s_stopAttempt++;
            if (s_stopAttempt < 3) {
                // Send multiple STOP conditions
                advanceTo(RecoveryStage::StopCondition1, STEP_DELAY_SHORT_MS);
            } else {
                advanceTo(RecoveryStage::WaitAfterStop, STEP_DELAY_LONG_MS);
            }
            break;

        case RecoveryStage::WaitAfterStop:
            // Wait for bus to settle, then decide on escalation
            // If we've tried software recovery multiple times, escalate to hardware reset
            if (s_softwareAttempts >= SOFTWARE_ATTEMPTS_BEFORE_HW) {
                Serial.println("[I2C_RECOVERY] Escalating to hardware peripheral reset");
                advanceTo(RecoveryStage::HwPeriphReset, STEP_DELAY_SHORT_MS);
            } else {
                advanceTo(RecoveryStage::WireBegin, STEP_DELAY_SHORT_MS);
            }
            break;

        case RecoveryStage::HwPeriphReset:
            // Execute hardware peripheral reset (ESP32-P4 native)
            Serial.println("[I2C_RECOVERY] Executing i2c_ll_reset_register(0)");
            executeHardwareReset(0);  // Reset I2C0 (Wire)
            advanceTo(RecoveryStage::HwWaitAfterReset, STEP_DELAY_HW_RESET_MS);
            break;

        case RecoveryStage::HwWaitAfterReset:
            // Wait for hardware to stabilize after peripheral reset
            Serial.println("[I2C_RECOVERY] Hardware reset complete, reinitializing Wire");
            advanceTo(RecoveryStage::WireBegin, STEP_DELAY_SHORT_MS);
            break;

        case RecoveryStage::WireBegin:
            // Reinitialize Wire
            if (s_wire) {
                s_wire->begin(s_sda, s_scl, s_freq);
                s_wire->setTimeOut(200);  // 200ms timeout
                Serial.printf("[I2C_RECOVERY] Wire reinitialized at %luHz\n",
                              (unsigned long)s_freq);
            }
            advanceTo(RecoveryStage::WaitAfterInit, STEP_DELAY_LONG_MS);
            break;

        case RecoveryStage::WaitAfterInit:
            // Wait for Wire to stabilize
            advanceTo(RecoveryStage::Verify, STEP_DELAY_SHORT_MS);
            break;

        case RecoveryStage::Verify:
            // Verify bus is healthy
            {
                bool healthy = isBusHealthy();
                if (healthy) {
                    Serial.println("[I2C_RECOVERY] Bus appears healthy");
                    completeRecovery(true);
                } else {
                    Serial.println("[I2C_RECOVERY] Bus still unhealthy after recovery");
                    completeRecovery(false);
                }
            }
            break;

        case RecoveryStage::Complete:
            // Already completed, should not reach here
            s_stage = RecoveryStage::Idle;
            break;

        case RecoveryStage::Idle:
        default:
            // Nothing to do
            break;
    }
}

// ============================================================================
// Internal Helpers
// ============================================================================

void I2CRecovery::executeBusClear(uint8_t pulses) {
    // Configure SCL as open-drain output
    pinMode(s_scl, OUTPUT_OPEN_DRAIN);
    digitalWrite(s_scl, HIGH);
    delayMicroseconds(5);

    // Toggle SCL
    for (uint8_t i = 0; i < pulses; i++) {
        digitalWrite(s_scl, LOW);
        delayMicroseconds(5);
        digitalWrite(s_scl, HIGH);
        delayMicroseconds(5);
    }

    // Release SCL
    pinMode(s_scl, INPUT_PULLUP);
}

void I2CRecovery::generateStopCondition() {
    // STOP condition: SDA transitions LOW->HIGH while SCL is HIGH
    pinMode(s_sda, OUTPUT_OPEN_DRAIN);
    pinMode(s_scl, OUTPUT_OPEN_DRAIN);

    // Ensure SDA is low first
    digitalWrite(s_sda, LOW);
    delayMicroseconds(2);

    // Set SCL high
    digitalWrite(s_scl, HIGH);
    delayMicroseconds(5);

    // Transition SDA low->high (STOP)
    digitalWrite(s_sda, HIGH);
    delayMicroseconds(5);

    // Release both
    pinMode(s_sda, INPUT_PULLUP);
    pinMode(s_scl, INPUT_PULLUP);
}

void I2CRecovery::executeHardwareReset(int port) {
    // ESP32-P4 native hardware peripheral reset
    // Uses i2c_ll_reset_register() which directly manipulates HP_SYS_CLKRST registers
    // Must be called inside PERIPH_RCC_ATOMIC() for thread safety
    //
    // This is the P4 equivalent of periph_module_reset(PERIPH_I2C0_MODULE) on S3
    // The legacy API is deprecated and non-functional on P4

    PERIPH_RCC_ATOMIC() {
        i2c_ll_reset_register(port);
    }

    Serial.printf("[I2C_RECOVERY] Hardware reset executed for I2C%d\n", port);
}

void I2CRecovery::advanceTo(RecoveryStage stage, uint32_t delayMs) {
    s_stage = stage;
    s_nextStepTime = millis() + delayMs;
}

void I2CRecovery::completeRecovery(bool success) {
    s_recoverySucceeded = success;
    s_stage = RecoveryStage::Idle;
    s_nextStepTime = 0;

    if (success) {
        s_recoverySuccesses++;
        s_errorCount = 0;  // Reset error count on success
        s_softwareAttempts = 0;  // Reset escalation counter
        Serial.printf("[I2C_RECOVERY] Recovery SUCCESSFUL (%d/%d total)\n",
                      s_recoverySuccesses, s_recoveryAttempts);
    } else {
        s_softwareAttempts++;  // Increment for escalation on next attempt
        Serial.printf("[I2C_RECOVERY] Recovery FAILED (attempt %d, escalation level %d/%d)\n",
                      s_recoveryAttempts, s_softwareAttempts, SOFTWARE_ATTEMPTS_BEFORE_HW);
        // Keep error count - will trigger retry after cooldown
    }
}
