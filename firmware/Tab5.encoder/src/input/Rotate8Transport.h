#pragma once
// ============================================================================
// Rotate8Transport - Clean M5ROTATE8 I2C Transport Layer
// ============================================================================
// A simple wrapper around M5ROTATE8 that provides:
// - Custom TwoWire instance support (for Tab5's M5.Ex_I2C)
// - Basic connection state tracking
// - Integration with I2CRecovery for software-level bus recovery
//
// This layer handles only I2C transport. Processing logic (debounce, wrap,
// clamp) is in EncoderProcessing.h.
//
// RECOVERY INTEGRATION:
// This transport layer optionally integrates with I2CRecovery to track I2C
// errors and trigger software-level bus recovery when errors exceed threshold.
// Recovery is SAFE for Tab5 - uses only Wire.end()/begin() and SCL toggling.
// ============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <M5ROTATE8.h>
#include "I2CRecovery.h"

class Rotate8Transport {
public:
    /**
     * Constructor
     * @param wire Pointer to TwoWire instance (e.g., &Wire after Wire.begin())
     * @param address M5ROTATE8 I2C address (default: 0x41)
     */
    Rotate8Transport(TwoWire* wire, uint8_t address = 0x41)
        : _encoder(address, wire)
        , _address(address)
        , _available(false)
    {}

    /**
     * Initialize connection to M5ROTATE8
     * @return true if device responds on I2C bus
     */
    bool begin() {
        _available = _encoder.begin() && _encoder.isConnected();

        if (_available) {
            // Clear all LEDs on successful init
            // M5ROTATE8 has 9 LEDs: 0-7 for encoders, 8 for status
            for (uint8_t i = 0; i < 9; i++) {
                _encoder.writeRGB(i, 0, 0, 0);
            }
        }

        return _available;
    }

    /**
     * Check if M5ROTATE8 is available
     * @return true if device was successfully initialized
     */
    bool isAvailable() const { return _available; }

    /**
     * Check if M5ROTATE8 is currently responding
     * This performs an actual I2C transaction
     * Records error if device is not responding
     * @return true if device responds
     */
    bool isConnected() {
        if (!_available) return false;
        bool connected = _encoder.isConnected();
        if (!connected) {
            I2CRecovery::recordError();
        }
        return connected;
    }

    /**
     * Get firmware version from M5ROTATE8
     * @return Version number (typically 2 for V2 firmware)
     */
    uint8_t getVersion() {
        if (!_available) return 0;
        return _encoder.getVersion();
    }

    // ========================================================================
    // Encoder Reading
    // ========================================================================

    /**
     * Get relative counter delta for an encoder channel
     * The counter is automatically reset after reading
     * Tracks I2C errors for recovery integration
     * @param channel Encoder channel (0-7)
     * @return Delta value since last read (can be negative)
     */
    int32_t getRelCounter(uint8_t channel) {
        if (!_available || channel > 7) return 0;

        // Read with error tracking
        int32_t value = _encoder.getRelCounter(channel);

        // Sanity check for wild values (indicates I2C corruption)
        if (value > 100 || value < -100) {
            I2CRecovery::recordError();
            return 0;  // Discard corrupt value
        }

        // Record success for non-zero valid reads
        if (value != 0) {
            I2CRecovery::recordSuccess();
            _encoder.resetCounter(channel);
        }

        return value;
    }

    /**
     * Get absolute counter value for an encoder channel
     * @param channel Encoder channel (0-7)
     * @return Absolute counter value
     */
    int32_t getAbsCounter(uint8_t channel) {
        if (!_available || channel > 7) return 0;
        return _encoder.getAbsCounter(channel);
    }

    /**
     * Check if encoder button is pressed
     * @param channel Encoder channel (0-7)
     * @return true if button is pressed
     */
    bool getKeyPressed(uint8_t channel) {
        if (!_available || channel > 7) return false;
        return _encoder.getKeyPressed(channel);
    }

    /**
     * Reset counter for a specific channel
     * @param channel Encoder channel (0-7)
     */
    void resetCounter(uint8_t channel) {
        if (!_available || channel > 7) return;
        _encoder.resetCounter(channel);
    }

    /**
     * Reset all encoder counters
     */
    void resetAll() {
        if (!_available) return;
        _encoder.resetAll();
    }

    // ========================================================================
    // LED Control
    // ========================================================================

    /**
     * Set RGB color for an LED
     * @param channel LED channel (0-7 for encoders, 8 for status LED)
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     */
    void setLED(uint8_t channel, uint8_t r, uint8_t g, uint8_t b) {
        // #region agent log - DISABLED by default (enable with ENABLE_VERBOSE_DEBUG)
        #ifdef ENABLE_VERBOSE_DEBUG
        static uint32_t s_lastLogTime = 0;
        static uint32_t s_writeCount = 0;
        s_writeCount++;
        uint32_t now = millis();
        if (now - s_lastLogTime >= 100) {  // Log every 100ms
            Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"C,F\",\"location\":\"Rotate8Transport.h:165\",\"message\":\"setLED.write\",\"data\":{\"channel\":%u,\"writesSinceLastLog\":%lu,\"timestamp\":%lu}\n",
                channel, static_cast<unsigned long>(s_writeCount), static_cast<unsigned long>(now));
            s_writeCount = 0;
            s_lastLogTime = now;
        }
        #endif // ENABLE_VERBOSE_DEBUG
        // #endregion
        
        if (!_available || channel > 8) {
            // #region agent log
            if (channel > 8) {
                Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"F\",\"location\":\"Rotate8Transport.h:167\",\"message\":\"setLED.invalidChannel\",\"data\":{\"channel\":%u,\"timestamp\":%lu}\n",
                    channel, static_cast<unsigned long>(millis()));
            }
            // #endregion
            return;
        }
        _encoder.writeRGB(channel, r, g, b);
    }

    /**
     * Set all LEDs to the same color
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     */
    void setAllLEDs(uint8_t r, uint8_t g, uint8_t b) {
        if (!_available) return;
        // #region agent log
        Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"boot\",\"hypothesisId\":\"H2\",\"location\":\"Rotate8Transport.h:setAllLEDs\",\"message\":\"allLEDs.set\",\"data\":{\"addr\":%u,\"r\":%u,\"g\":%u,\"b\":%u},\"timestamp\":%lu}\n",
                      _address, r, g, b, static_cast<unsigned long>(millis()));
        // #endregion
        _encoder.setAll(r, g, b);
    }

    /**
     * Turn off all LEDs
     */
    void allLEDsOff() {
        if (!_available) return;
        // #region agent log
        Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"boot\",\"hypothesisId\":\"H2\",\"location\":\"Rotate8Transport.h:allLEDsOff\",\"message\":\"allLEDs.off\",\"data\":{\"addr\":%u},\"timestamp\":%lu}\n",
                      _address, static_cast<unsigned long>(millis()));
        // #endregion
        _encoder.allOff();
    }

    // ========================================================================
    // V2 Firmware Features
    // ========================================================================

    /**
     * Get encoder change mask (V2 firmware)
     * Bit N is set if encoder N has changed since last read
     * @return Bitmask of changed encoders
     */
    uint8_t getEncoderChangeMask() {
        if (!_available) return 0;
        return _encoder.getEncoderChangeMask();
    }

    /**
     * Get button change mask (V2 firmware)
     * Bit N is set if button N is pressed
     * @return Bitmask of pressed buttons
     */
    uint8_t getButtonChangeMask() {
        if (!_available) return 0;
        return _encoder.getButtonChangeMask();
    }

    /**
     * Get input switch state
     * @return Switch position (0 or 1)
     */
    uint8_t getInputSwitch() {
        if (!_available) return 0;
        return _encoder.inputSwitch();
    }

    // ========================================================================
    // Direct Access (for advanced use)
    // ========================================================================

    /**
     * Get reference to underlying M5ROTATE8 instance
     * Use with caution - bypasses transport layer checks
     * @return Reference to M5ROTATE8
     */
    M5ROTATE8& encoder() { return _encoder; }

    /**
     * Get I2C address
     * @return I2C address (typically 0x41)
     */
    uint8_t getAddress() const { return _address; }

    /**
     * Mark the transport as unavailable (called during recovery)
     * Recovery will re-probe after bus reset
     */
    void markUnavailable() {
        _available = false;
    }

    /**
     * Attempt to reinitialize after recovery
     * @return true if device is now available
     */
    bool reinit() {
        _available = _encoder.begin() && _encoder.isConnected();
        if (_available) {
            // Clear all LEDs on successful reinit
            for (uint8_t i = 0; i < 9; i++) {
                _encoder.writeRGB(i, 0, 0, 0);
            }
            Serial.printf("[Rotate8Transport] Reinit successful at 0x%02X\n", _address);
        }
        return _available;
    }

private:
    M5ROTATE8 _encoder;
    uint8_t _address;
    bool _available;

    // NOTE: Recovery is now handled by I2CRecovery module.
    // Uses software-level bus clear (SCL toggling) which is safe for Tab5.
    // No hardware peripheral resets are performed.
};
