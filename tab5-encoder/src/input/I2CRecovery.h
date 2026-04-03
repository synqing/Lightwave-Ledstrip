// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once
// ============================================================================
// I2CRecovery - I2C Bus Recovery + Error Counter for Tab5 External Bus
// ============================================================================
// Error tracking and runtime bus recovery for the external I2C bus
// (Grove Port.A, GPIO 53/54). The external bus is ISOLATED from the
// Tab5's internal I2C (display/touch/audio on Wire1) — resetting Wire
// does not affect internal peripherals.
//
// Three recovery levels (escalating):
//   Level 1 (resetBus):
//     1. Wire.end()          — tear down corrupted ESP-IDF I2C driver
//     2. 150ms settle        — let driver release cleanly
//     3. i2cBusClear()       — up to 18 SCL clocks to release stuck slave
//     4. 50ms settle         — let bus stabilise after clocking
//     5. 100ms settle        — let bus capacitance discharge
//     6. Verify bus state    — warn if SDA/SCL still stuck
//     7. Wire.begin()        — fresh I2C driver initialisation
//     8. Wire.setTimeOut()   — restore timeout
//
//   Level 2 (powerCycleGrove):
//     1. Wire.end()
//     1b. Tri-state SDA/SCL  — prevent back-powering slave via pull-ups
//     2. PI4IOE5V6408 EXT5V_EN LOW via Wire1 — cuts Grove port power
//     3. 800ms discharge     — capacitors + STM32F030 full power-down
//     4. PI4IOE5V6408 EXT5V_EN HIGH          — restore power
//     5. 500ms boot          — STM32F030 needs 40-100ms; generous margin
//     6. i2cBusClear()       — clear any stuck state from power-on
//     7. Verify bus state    — warn if SDA/SCL still stuck
//     8. Wire.begin()        — fresh I2C driver initialisation
//     9. Wire.setTimeOut()   — restore timeout
//
// Usage:
//   I2CRecovery::init(&Wire, sda, scl, freq);
//   // In REPROBE failure path:
//   I2CRecovery::resetBus();           // Level 1
//   I2CRecovery::powerCycleGrove();    // Level 2 (if Level 1 failed)
//   // In transport layer:
//   if (transmissionError) I2CRecovery::recordError();
//   else                   I2CRecovery::recordSuccess();
// ============================================================================

#include <Arduino.h>
#include <Wire.h>

class I2CRecovery {
public:
    /**
     * Initialise the recovery module.
     * Stores bus parameters for later resetBus()/powerCycleGrove() calls.
     */
    static void init(TwoWire* wire, int sda, int scl, uint32_t freq, uint16_t timeout = 50);

    /**
     * Level 1 recovery: Wire teardown + GPIO bus clear + reinit.
     *
     * Clears ESP_ERR_INVALID_STATE by destroying and recreating the
     * I2C driver, with a GPIO-level bus clear in between.
     *
     * SAFETY: Only affects the external bus (Wire on GPIO 53/54).
     * The internal bus (Wire1 — display, touch, audio) is untouched.
     *
     * @return true if Wire.begin() succeeded (bus is ready for probe)
     */
    static bool resetBus();

    /**
     * Level 2 recovery: Grove port power cycle + full reinit.
     *
     * Cuts power to the Grove port via PI4IOE5V6408 I/O expander
     * (address 0x43 on Wire1), waits for full discharge and STM32F030
     * reboot, then reinitialises the bus.
     *
     * SAFETY: Wire1 commands to the expander do not affect the external
     * bus being recovered. The expander is on the internal I2C bus.
     *
     * @return true if Wire.begin() succeeded (bus is ready for probe)
     */
    static bool powerCycleGrove();

    /** Record an I2C error (increments consecutive error count). */
    static void recordError();

    /** Record a successful transaction (resets consecutive error count). */
    static void recordSuccess();

    /** Record a successful bus recovery (increments lifetime counter). */
    static void recordRecoverySuccess();

    /** Get current consecutive error count. */
    static uint8_t getErrorCount() { return s_errorCount; }

    /** Get lifetime successful recovery count. */
    static uint16_t getRecoverySuccesses() { return s_recoverySuccesses; }

private:
    static bool s_initialized;
    static TwoWire* s_wire;
    static int s_sda;
    static int s_scl;
    static uint32_t s_freq;
    static uint16_t s_timeout;
    static uint8_t s_errorCount;
    static uint16_t s_recoverySuccesses;

    // PI4IOE5V6408 I/O expander on internal I2C bus
    static constexpr uint8_t EXPANDER_ADDR = 0x43;
    static constexpr uint8_t EXPANDER_OUTPUT_REG = 0x01;
    static constexpr uint8_t EXT5V_EN_BIT = 0x04;  // Pin 2 = EXT5V_EN

    /**
     * Reinitialise Wire with stored parameters.
     * @return true if Wire.begin() succeeded
     */
    static bool reinitWire();
};
