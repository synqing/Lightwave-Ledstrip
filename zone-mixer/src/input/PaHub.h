/**
 * PaHub v2.1 (PCA9548A) I2C Multiplexer — Channel Selection
 *
 * Provides init with probe, channel selection with settle delay,
 * and error-checked disable. All functions take TwoWire& to avoid
 * coupling to the global Wire instance.
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <Wire.h>
#include "../config.h"

namespace pahub {

/// Probe the PCA9548A at addr 0x70, disable all channels, settle.
/// Returns true if the multiplexer responds.
inline bool init(TwoWire& wire) {
    wire.beginTransmission(hw::PAHUB_ADDR);
    uint8_t err = wire.endTransmission();
    if (err != 0) {
        Serial.printf("[PaHub] FAIL: 0x%02X not responding (err=%d)\n",
                      hw::PAHUB_ADDR, err);
        return false;
    }

    // Disable all channels on startup
    wire.beginTransmission(hw::PAHUB_ADDR);
    wire.write(0x00);
    wire.endTransmission();

    delay(10);  // Settle after init
    Serial.println("[PaHub] OK: PCA9548A at 0x70");
    return true;
}

/// Select a PaHub channel (0-5). Returns true on success.
/// Includes 100us settle delay for the STM32F030 inside the PaHub.
inline bool selectChannel(TwoWire& wire, uint8_t channel) {
    if (channel > 5) return false;
    wire.beginTransmission(hw::PAHUB_ADDR);
    wire.write(1 << channel);
    uint8_t err = wire.endTransmission();
    delayMicroseconds(hw::INTER_TXN_DELAY_US);  // STM32F030 settle
    if (err != 0) {
        Serial.printf("[PaHub] ERR: channel %d select failed (err=%d)\n",
                      channel, err);
        return false;
    }
    return true;
}

/// Disable all PaHub channels. Returns true on success.
inline bool disableAll(TwoWire& wire) {
    wire.beginTransmission(hw::PAHUB_ADDR);
    wire.write(0x00);
    uint8_t err = wire.endTransmission();
    return (err == 0);
}

}  // namespace pahub
