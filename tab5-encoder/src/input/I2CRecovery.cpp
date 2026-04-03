// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
// ============================================================================
// I2CRecovery - Bus Recovery + Error Counter for Tab5 External I2C
// ============================================================================
// Two recovery levels:
//   resetBus()         — Wire teardown + GPIO bus clear + reinit
//   powerCycleGrove()  — Grove port power cycle via PI4IOE5V6408 + reinit
//
// Both are safe because the external bus (Wire on GPIO 53/54) is
// physically isolated from the internal bus (Wire1).
// ============================================================================

#include "I2CRecovery.h"
#include "I2CBusClear.h"
#include <esp_task_wdt.h>

bool I2CRecovery::s_initialized = false;
TwoWire* I2CRecovery::s_wire = nullptr;
int I2CRecovery::s_sda = 0;
int I2CRecovery::s_scl = 0;
uint32_t I2CRecovery::s_freq = 0;
uint16_t I2CRecovery::s_timeout = 0;
uint8_t I2CRecovery::s_errorCount = 0;
uint16_t I2CRecovery::s_recoverySuccesses = 0;

void I2CRecovery::init(TwoWire* wire, int sda, int scl, uint32_t freq, uint16_t timeout) {
    s_wire = wire;
    s_sda = sda;
    s_scl = scl;
    s_freq = freq;
    s_timeout = timeout;
    s_initialized = true;
    s_errorCount = 0;
    s_recoverySuccesses = 0;
    Serial.printf("[I2C_RECOVERY] Initialised: SDA=%d SCL=%d freq=%luHz timeout=%ums\n",
                  sda, scl, (unsigned long)freq, timeout);
}

// ============================================================================
// Level 1: Wire teardown + GPIO bus clear + reinit
// ============================================================================

bool I2CRecovery::resetBus() {
    if (!s_initialized || !s_wire) {
        Serial.println("[I2C_RECOVERY] resetBus() called before init — aborting");
        return false;
    }

    Serial.printf("[I2C_RECOVERY] === LEVEL 1: BUS RESET (SDA=%d SCL=%d) ===\n",
                  s_sda, s_scl);

    // Step 1: Tear down the ESP-IDF I2C driver.
    // If the driver is in ESP_ERR_INVALID_STATE, all Wire operations fail
    // silently. Wire.end() releases the driver and frees the GPIO pins.
    Serial.println("[I2C_RECOVERY] Wire.end() — tearing down I2C driver");
    s_wire->end();
    delay(150);

    esp_task_wdt_reset();

    // Step 2: GPIO bus clear (up to 18 SCL clocks).
    // Delegates to i2cBusClear() from I2CBusClear.h — the same function
    // used at boot. If a slave is holding SDA low mid-byte, this releases it.
    i2cBusClear(s_sda, s_scl);
    delay(50);

    esp_task_wdt_reset();

    // Step 3: Settle time — let pull-ups charge, slave state machines reset.
    delay(100);

    // Step 4-5: Fresh Wire init with stored parameters.
    return reinitWire();
}

// ============================================================================
// Level 2: Grove port power cycle + full reinit
// ============================================================================

bool I2CRecovery::powerCycleGrove() {
    if (!s_initialized || !s_wire) {
        Serial.println("[I2C_RECOVERY] powerCycleGrove() called before init — aborting");
        return false;
    }

    Serial.printf("[I2C_RECOVERY] === LEVEL 2: GROVE POWER CYCLE (SDA=%d SCL=%d) ===\n",
                  s_sda, s_scl);

    // Step 1: Tear down external I2C driver first.
    Serial.println("[I2C_RECOVERY] Wire.end() — tearing down external I2C");
    s_wire->end();

    // Step 1b: Tri-state I2C pins to prevent back-powering slave through
    // SDA/SCL pull-ups during Grove power cycle.
    pinMode(s_sda, INPUT);  // No pull-up — prevent back-powering slave
    pinMode(s_scl, INPUT);  // during Grove power cycle

    esp_task_wdt_reset();

    // Step 2: Cut power to Grove Port.A via PI4IOE5V6408 (0x43 on Wire1).
    // The expander is on the INTERNAL I2C bus — this does not affect the
    // external bus being recovered.
    Serial.println("[I2C_RECOVERY] EXT5V OFF — cutting Grove port power");
    Wire1.beginTransmission(EXPANDER_ADDR);
    Wire1.write(EXPANDER_OUTPUT_REG);
    Wire1.write(0x00);  // All pins LOW — cuts EXT5V
    uint8_t txErr = Wire1.endTransmission();
    if (txErr != 0) {
        Serial.printf("[I2C_RECOVERY] WARNING: Expander write failed (err=%d) — "
                      "power cycle may not work\n", txErr);
        // Continue anyway — the bus clear below may still help
    }

    // Step 3: Wait for full discharge.
    // Capacitors on the Grove port need time to discharge. The M5ROTATE8's
    // STM32F030 must fully power down to clear its I2C state machine.
    // 800ms ensures full capacitor discharge for reliable reset.
    Serial.println("[I2C_RECOVERY] EXT5V OFF — waiting 800ms for full discharge");
    delay(800);

    esp_task_wdt_reset();

    // Step 4: Restore power.
    Serial.println("[I2C_RECOVERY] EXT5V ON — restoring Grove port power");
    Wire1.beginTransmission(EXPANDER_ADDR);
    Wire1.write(EXPANDER_OUTPUT_REG);
    Wire1.write(EXT5V_EN_BIT);  // Pin 2 HIGH — EXT5V_EN on
    Wire1.endTransmission();

    // Step 5: Wait for STM32F030 boot.
    // Datasheet: 40-100ms from power-on to I2C ready. 500ms for margin.
    Serial.println("[I2C_RECOVERY] Waiting 500ms for STM32F030 boot");
    delay(500);

    esp_task_wdt_reset();

    // Step 6: Bus clear after power restore — the STM32 may have come
    // up in a state where it holds SDA low briefly.
    i2cBusClear(s_sda, s_scl);

    // Step 7-8: Fresh Wire init with stored parameters.
    return reinitWire();
}

// ============================================================================
// Internal: reinitialise Wire with stored parameters
// ============================================================================

bool I2CRecovery::reinitWire() {
    // Verify bus state before Wire.begin() — if SDA or SCL is still stuck,
    // Wire.begin() will fail or produce immediate timeouts.
    pinMode(s_sda, INPUT_PULLUP);
    pinMode(s_scl, INPUT_PULLUP);
    delayMicroseconds(100);
    if (digitalRead(s_sda) == LOW || digitalRead(s_scl) == LOW) {
        Serial.printf("[I2C_RECOVERY] Bus still stuck (SDA=%d SCL=%d) — Wire.begin() will fail\n",
                      digitalRead(s_sda), digitalRead(s_scl));
    }

    Serial.printf("[I2C_RECOVERY] Wire.begin(%d, %d, %lu)\n",
                  s_sda, s_scl, (unsigned long)s_freq);
    bool ok = s_wire->begin(s_sda, s_scl, s_freq);
    if (!ok) {
        Serial.println("[I2C_RECOVERY] ERROR: Wire.begin() failed — bus may be damaged");
        return false;
    }

    s_wire->setTimeOut(s_timeout);
    Serial.println("[I2C_RECOVERY] === RECOVERY COMPLETE ===");
    return true;
}

// ============================================================================
// Error counting
// ============================================================================

void I2CRecovery::recordError() {
    if (!s_initialized) return;
    s_errorCount++;
}

void I2CRecovery::recordSuccess() {
    if (!s_initialized) return;
    if (s_errorCount > 0) s_errorCount = 0;
}

void I2CRecovery::recordRecoverySuccess() {
    if (!s_initialized) return;
    s_recoverySuccesses++;
    s_errorCount = 0;
}
