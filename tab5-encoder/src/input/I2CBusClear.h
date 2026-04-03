/**
 * I2C Bus Clear — Recovers from stuck SDA after ESP32 reset.
 *
 * If the ESP32 resets mid-transaction (e.g. firmware flash), a slave
 * device (M5ROTATE8, PCA9548A) may hold SDA low. The I2C master driver
 * cannot initialise when SDA is stuck — every probe returns timeout.
 *
 * Bit-bangs up to 18 SCL clocks while monitoring SDA. Per the I2C
 * specification (NXP UM10204 section 3.1.16), a slave holding SDA
 * low will release it within 9 clocks — but STM32F030 slaves may
 * need up to 18. After SDA releases, a STOP condition returns the
 * bus to idle. If SDA remains stuck after all clocks, STOP is
 * skipped (would cause bus conflict) and pins are released as-is.
 *
 * MUST be called BEFORE Wire.begin() — uses raw GPIO only.
 * If the I2C peripheral has already claimed the pins (e.g. via
 * M5.Ex_I2C.begin()), the pinMode() calls will detach them from the
 * peripheral. Wire.begin() will reclaim them afterwards.
 *
 * Ported from zone-mixer/src/input/I2CBusClear.h
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <Arduino.h>

/// Recover an I2C bus where a slave is holding SDA low.
/// Both pins are returned to INPUT (high-Z) so Wire.begin() can take them.
inline void i2cBusClear(uint8_t sdaPin, uint8_t sclPin) {
    pinMode(sdaPin, INPUT_PULLUP);
    pinMode(sclPin, INPUT_PULLUP);
    delayMicroseconds(50);

    // Check whether recovery is needed
    if (digitalRead(sdaPin) == HIGH) {
        pinMode(sdaPin, INPUT);
        pinMode(sclPin, INPUT);
        Serial.println("[I2C] Bus clear: SDA high — no recovery needed");
        return;
    }

    Serial.println("[I2C] Bus clear: SDA stuck LOW — clocking out slave");

    pinMode(sclPin, OUTPUT);
    digitalWrite(sclPin, HIGH);
    delayMicroseconds(50);

    // Clock up to 18 times — STM32F030 slaves may need more than 9
    bool recovered = false;
    for (uint8_t i = 0; i < 18; i++) {
        digitalWrite(sclPin, LOW);
        delayMicroseconds(50);
        digitalWrite(sclPin, HIGH);
        delayMicroseconds(50);

        if (digitalRead(sdaPin) == HIGH) {
            Serial.printf("[I2C] Bus clear: SDA released after %d clock(s)\n", i + 1);
            recovered = true;
            break;
        }
    }

    if (!recovered) {
        Serial.println("[I2C] Bus clear: WARNING — SDA still low after 18 clocks");
        // Do NOT generate STOP — SDA is still held by slave, STOP would
        // cause bus conflict. Just release pins and let caller escalate.
        pinMode(sdaPin, INPUT);
        pinMode(sclPin, INPUT);
        Serial.println("[I2C] Bus clear: pins released (no STOP — bus conflict risk)");
        return;
    }

    // Generate STOP condition: SDA low-to-high while SCL high
    pinMode(sdaPin, OUTPUT);
    digitalWrite(sdaPin, LOW);
    delayMicroseconds(50);
    digitalWrite(sclPin, HIGH);
    delayMicroseconds(50);
    digitalWrite(sdaPin, HIGH);
    delayMicroseconds(50);

    // Release pins for Wire.begin()
    pinMode(sdaPin, INPUT);
    pinMode(sclPin, INPUT);
    Serial.println("[I2C] Bus clear: STOP generated, pins released");
}
