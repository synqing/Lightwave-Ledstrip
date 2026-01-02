#pragma once

#include <M5Unified.h>

/**
 * @file Rotate8Transport.h
 * @brief Bus-safe ROTATE8 transport wrapper
 * 
 * Bus-safe ROTATE8 transport for Tab5 using M5Unified's external I2C (`M5.Ex_I2C`).
 *
 * IMPORTANT:
 * - On Tab5, `M5.Ex_I2C` is `m5::I2C_Class` (not `TwoWire`).
 * - We avoid `Wire`/`TwoWire` here to prevent driver/bus mismatches.
 * - No forbidden low-level I2C reset patterns.
 */

class Rotate8Transport {
public:
    /**
     * @brief Constructor
     * @param bus External I2C bus to use (pass `&M5.Ex_I2C`)
     * @param i2c_address ROTATE8 I2C address (default: 0x41)
     * @param freq_hz I2C frequency (default: 100kHz)
     */
    Rotate8Transport(m5::I2C_Class* bus, uint8_t i2c_address = 0x41, uint32_t freq_hz = 100000);

    /**
     * @brief Initialise transport
     * @return true if successful, false otherwise
     */
    bool begin();

    /**
     * @brief Read encoder relative delta for a channel
     * @param channel Encoder channel (0-7)
     * @return Relative delta (positive/negative)
     */
    int32_t readDelta(uint8_t channel);

    /**
     * @brief Read button state for a channel
     * @param channel Encoder channel (0-7)
     * @return true if button pressed, false otherwise
     */
    bool readButton(uint8_t channel);

    /**
     * @brief Set LED colour for a channel
     * @param index LED index (0-7, or 8 for all)
     * @param r Red value (0-255)
     * @param g Green value (0-255)
     * @param b Blue value (0-255)
     * @return true if successful
     */
    bool setLED(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief Reset encoder counter for a channel
     * @param channel Encoder channel (0-7)
     * @return true if successful
     */
    bool resetCounter(uint8_t channel);

    /**
     * @brief Check if ROTATE8 is connected
     * @return true if connected
     */
    bool isConnected();

    /**
     * @brief Get I2C address
     */
    uint8_t getAddress() const { return _i2c_address; }

private:
    m5::I2C_Class* _bus = nullptr;
    uint8_t _i2c_address = 0x41;
    uint32_t _freq_hz = 100000;

    // M5ROTATE8 register map (matches upstream library v0.4.1)
    static constexpr uint8_t REG_VERSION           = 0xFE;
    static constexpr uint8_t REG_BASE_REL          = 0x20;
    static constexpr uint8_t REG_BASE_RESET        = 0x40;
    static constexpr uint8_t REG_BASE_BUTTON_VALUE = 0x50;
    static constexpr uint8_t REG_RGB               = 0x70;

    static int32_t readI32LE(const uint8_t* b);
};

