#pragma once

#include <Wire.h>
#include <M5Unified.h>

/**
 * @file I2CScan.h
 * @brief I2C bus scanning utility
 * 
 * Scans an I2C bus and prints found addresses to Serial.
 * Supports both TwoWire and m5::I2C_Class instances.
 */

namespace I2CScan {
    /**
     * @brief Scan I2C bus and print found addresses (TwoWire version)
     * @param bus TwoWire instance to scan
     * @return Number of devices found
     */
    uint8_t scanI2C(TwoWire& bus);
    
    /**
     * @brief Scan I2C bus and print found addresses (m5::I2C_Class version)
     * @param bus m5::I2C_Class instance to scan (e.g., M5.Ex_I2C)
     * @return Number of devices found
     */
    uint8_t scanI2C(m5::I2C_Class& bus);
}

