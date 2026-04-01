/**
 * PaHub v2.1 (PCA9548A) I2C Multiplexer — Channel Selection
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <Wire.h>
#include "../config.h"

namespace pahub {

/// Select a PaHub channel (0-5). Returns true on success.
inline bool selectChannel(uint8_t channel) {
    if (channel > 5) return false;
    Wire.beginTransmission(hw::PAHUB_ADDR);
    Wire.write(1 << channel);
    return (Wire.endTransmission() == 0);
}

/// Disable all PaHub channels.
inline void disableAll() {
    Wire.beginTransmission(hw::PAHUB_ADDR);
    Wire.write(0x00);
    Wire.endTransmission();
}

}  // namespace pahub
