/**
 * I2C Bus Clear — Recovers from stuck SDA after ESP32 reset.
 *
 * The PCA9548A (PaHub v2.1) has no reset pin on the M5 breakout.
 * If the ESP32 resets mid-transaction (e.g. firmware flash), the
 * PCA9548A may hold SDA low. The ESP-IDF I2C master driver cannot
 * initialise when SDA is stuck — every probe returns timeout.
 *
 * Bit-bangs up to 9 SCL clocks while monitoring SDA. Per the I2C
 * specification (NXP UM10204 section 3.1.16), a slave holding SDA
 * low will release it within 9 clocks. After SDA releases, a STOP
 * condition returns the bus to idle.
 *
 * MUST be called BEFORE Wire.begin() — uses raw GPIO only.
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
    delayMicroseconds(5);

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
    delayMicroseconds(5);

    // Clock up to 9 times — slave must release SDA within one byte + ACK
    bool recovered = false;
    for (uint8_t i = 0; i < 9; i++) {
        digitalWrite(sclPin, LOW);
        delayMicroseconds(5);
        digitalWrite(sclPin, HIGH);
        delayMicroseconds(5);

        if (digitalRead(sdaPin) == HIGH) {
            Serial.printf("[I2C] Bus clear: SDA released after %d clock(s)\n", i + 1);
            recovered = true;
            break;
        }
    }

    if (!recovered) {
        Serial.println("[I2C] Bus clear: WARNING — SDA still low after 9 clocks");
    }

    // Generate STOP condition: SDA low-to-high while SCL high
    pinMode(sdaPin, OUTPUT);
    digitalWrite(sdaPin, LOW);
    delayMicroseconds(5);
    digitalWrite(sclPin, HIGH);
    delayMicroseconds(5);
    digitalWrite(sdaPin, HIGH);
    delayMicroseconds(5);

    // Release pins for Wire.begin()
    pinMode(sdaPin, INPUT);
    pinMode(sclPin, INPUT);
    Serial.println("[I2C] Bus clear: STOP generated, pins released");
}
