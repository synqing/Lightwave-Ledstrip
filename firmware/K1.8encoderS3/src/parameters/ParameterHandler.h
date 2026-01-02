#pragma once

#include <ArduinoJson.h>
#include "ParameterMap.h"
#include <cstdint>

// Forward declarations
class EncoderController;
class WebSocketClient;
class DisplayUI;

/**
 * @file ParameterHandler.h
 * @brief Business logic for parameter synchronization
 * 
 * Handles:
 * - Encoder changes → WebSocket commands
 * - WebSocket status messages → local state updates
 * - Validation and clamping
 */

class ParameterHandler {
public:
    ParameterHandler(
        EncoderController* encoderCtrl,
        WebSocketClient* wsClient,
        DisplayUI* displayUI
    );

    /**
     * @brief Handle encoder value change
     * @param index Encoder index (0-7)
     * @param value New value
     * @param wasReset True if value was reset via button press
     */
    void onEncoderChanged(uint8_t index, uint16_t value, bool wasReset);

    /**
     * @brief Apply status message from LightwaveOS
     * @param doc JSON document with "type": "status" and parameter fields
     * @return true if any parameters were updated
     */
    bool applyStatus(StaticJsonDocument<512>& doc);

    /**
     * @brief Get current parameter value
     * @param id Parameter ID
     * @return Current value, or 0 if invalid
     */
    uint8_t getValue(ParameterId id) const;

    /**
     * @brief Set parameter value (for UI state tracking)
     * @param id Parameter ID
     * @param value New value
     */
    void setValue(ParameterId id, uint8_t value);

private:
    EncoderController* m_encoderCtrl;
    WebSocketClient* m_wsClient;
    DisplayUI* m_displayUI;
    
    // Local state cache (for UI updates)
    uint8_t m_values[8];
    
    /**
     * @brief Send parameter change via WebSocket
     * @param param Parameter definition
     * @param value Parameter value
     */
    void sendParameterChange(const ParameterDef* param, uint8_t value);
    
    /**
     * @brief Clamp value to parameter range
     * @param param Parameter definition
     * @param value Value to clamp
     * @return Clamped value
     */
    uint8_t clampValue(const ParameterDef* param, uint8_t value) const;
};

