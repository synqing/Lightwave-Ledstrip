#pragma once
// ============================================================================
// ParameterHandler - Parameter Synchronization for Tab5.encoder
// ============================================================================
// Business logic for parameter synchronization between:
// - EncoderService (local encoder input)
// - WebSocketClient (LightwaveOS server)
// - Display (optional, via callback interface)
//
// Adapted from K1.8encoderS3 for Tab5's EncoderService interface.
// Extended for 16 parameters across dual M5ROTATE8 units.
//
// Key differences from K1.8encoderS3:
// - Uses EncoderService instead of EncoderController
// - Display updates via abstract callback (display built by another agent)
// - No direct DisplayUI dependency
// - Supports 16 parameters (dual M5ROTATE8)
// ============================================================================

#include <ArduinoJson.h>
#include "ParameterMap.h"
#include <cstdint>
#include <functional>

// Forward declarations
class EncoderService;
class WebSocketClient;

/**
 * Display update callback type
 * @param values Pointer to 16-element array of parameter values
 * @param highlightIndex Index of parameter to highlight (-1 for none)
 */
using DisplayCallback = std::function<void(uint8_t* values, int highlightIndex)>;

class ParameterHandler {
public:
    /**
     * Constructor
     * @param encoderService Pointer to EncoderService (Tab5 encoder interface)
     * @param wsClient Pointer to WebSocketClient
     */
    ParameterHandler(
        EncoderService* encoderService,
        WebSocketClient* wsClient
    );

    /**
     * Handle encoder value change
     * Called by EncoderService callback when encoder is rotated or reset
     * @param index Encoder index (0-15)
     * @param value New value
     * @param wasReset True if value was reset via button press
     */
    void onEncoderChanged(uint8_t index, uint16_t value, bool wasReset);

    /**
     * Apply status message from LightwaveOS
     * Updates local state and encoder values without triggering callbacks
     * @param doc JSON document with "type": "status" and parameter fields
     * @return true if any parameters were updated
     */
    bool applyStatus(StaticJsonDocument<1024>& doc);

    /**
     * Get current parameter value
     * @param id Parameter ID
     * @return Current value, or 0 if invalid
     */
    uint8_t getValue(ParameterId id) const;

    /**
     * Set parameter value (for UI state tracking)
     * @param id Parameter ID
     * @param value New value
     */
    void setValue(ParameterId id, uint8_t value);

    /**
     * Get all current values
     * @param values Array to fill (must have 16 elements)
     */
    void getAllValues(uint8_t values[PARAMETER_COUNT]) const;

    /**
     * Set display update callback
     * Called when parameters change and display needs updating
     * @param callback Display update function
     */
    void setDisplayCallback(DisplayCallback callback) { m_displayCallback = callback; }

private:
    EncoderService* m_encoderService;
    WebSocketClient* m_wsClient;
    DisplayCallback m_displayCallback;

    // Local state cache (for UI updates) - 16 parameters
    uint8_t m_values[PARAMETER_COUNT];

    /**
     * Send parameter change via WebSocket
     * @param param Parameter definition
     * @param value Parameter value
     */
    void sendParameterChange(const ParameterDef* param, uint8_t value);

    /**
     * Clamp value to parameter range
     * @param param Parameter definition
     * @param value Value to clamp
     * @return Clamped value
     */
    uint8_t clampValue(const ParameterDef* param, uint8_t value) const;

    /**
     * Notify display of parameter change
     * @param highlightIndex Index of parameter to highlight (-1 for none)
     */
    void notifyDisplay(int highlightIndex);
};
