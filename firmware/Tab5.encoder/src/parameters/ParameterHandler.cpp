// ============================================================================
// ParameterHandler - Parameter Synchronization for Tab5.encoder
// ============================================================================
// Adapted from K1.8encoderS3 for Tab5's EncoderService interface.
// Extended for 16 parameters across dual M5ROTATE8 units.
// ============================================================================

#include "ParameterHandler.h"
#include "ParameterMap.h"
#include "../input/DualEncoderService.h"
#include "../network/WebSocketClient.h"
#include <Arduino.h>
#include <cstring>

ParameterHandler::ParameterHandler(
    DualEncoderService* encoderService,
    WebSocketClient* wsClient
)
    : m_encoderService(encoderService)
    , m_wsClient(wsClient)
    , m_displayCallback(nullptr)
{
    // Initialize with default values for all 16 parameters
    for (uint8_t i = 0; i < PARAMETER_COUNT; i++) {
        const ParameterDef* param = getParameterByIndex(i);
        if (param) {
            m_values[i] = param->defaultValue;
        } else {
            m_values[i] = 128;  // Fallback default
        }
    }
}

void ParameterHandler::onEncoderChanged(uint8_t index, uint16_t value, bool wasReset) {
    const ParameterDef* param = getParameterByIndex(index);
    if (!param || !m_wsClient) {
        return;
    }

    // Clamp value to valid range
    uint8_t clampedValue = clampValue(param, static_cast<uint8_t>(value));

    // Update local state
    m_values[index] = clampedValue;

    // Notify display (with highlight)
    notifyDisplay(index);

    // Send to LightwaveOS via WebSocket (only if connected)
    if (m_wsClient->isConnected()) {
        sendParameterChange(param, clampedValue);
    }

    // Debug output
    Serial.printf("[Param] %s: %d%s\n",
                  param->statusField,
                  clampedValue,
                  wasReset ? " (reset)" : "");
}

bool ParameterHandler::applyStatus(JsonDocument& doc) {
    // Check type field using ArduinoJson 7 pattern (is<T>() instead of containsKey)
    if (!doc["type"].is<const char*>() || strcmp(doc["type"], "status") != 0) {
        return false;
    }

    bool updated = false;

    // Apply each parameter from status message
    for (uint8_t i = 0; i < getParameterCount(); i++) {
        const ParameterDef* param = getParameterByIndex(i);
        if (!param) continue;

        // Check if this field exists in the status message (ArduinoJson 7 pattern)
        if (doc[param->statusField].is<int>() || doc[param->statusField].is<uint8_t>()) {
            uint8_t newValue = 0;

            // Handle different JSON types (uint8_t, int, etc.)
            if (doc[param->statusField].is<uint8_t>()) {
                newValue = doc[param->statusField].as<uint8_t>();
            } else if (doc[param->statusField].is<int>()) {
                int val = doc[param->statusField].as<int>();
                if (val >= 0 && val <= 255) {
                    newValue = static_cast<uint8_t>(val);
                }
            } else {
                continue;  // Skip invalid types
            }

            // Clamp to valid range
            newValue = clampValue(param, newValue);

            // Only update if value changed (avoid echo loops)
            if (m_values[i] != newValue) {
                m_values[i] = newValue;

                // Update encoder hardware (without triggering callback to avoid echo)
                // Tab5 EncoderService interface: setValue(param, value, triggerCallback)
                if (m_encoderService) {
                    m_encoderService->setValue(i, newValue, false);  // false = don't trigger callback
                }

                updated = true;

                Serial.printf("[Param] Synced %s: %d\n", param->statusField, newValue);
            }
        }
    }

    // Notify display if any parameters changed (no highlight)
    if (updated) {
        notifyDisplay(-1);
    }

    return updated;
}

uint8_t ParameterHandler::getValue(ParameterId id) const {
    uint8_t index = static_cast<uint8_t>(id);
    if (index >= PARAMETER_COUNT) {
        return 0;
    }
    return m_values[index];
}

void ParameterHandler::setValue(ParameterId id, uint8_t value) {
    uint8_t index = static_cast<uint8_t>(id);
    if (index >= PARAMETER_COUNT) {
        return;
    }
    m_values[index] = value;
}

void ParameterHandler::getAllValues(uint8_t values[PARAMETER_COUNT]) const {
    for (uint8_t i = 0; i < PARAMETER_COUNT; i++) {
        values[i] = m_values[i];
    }
}

void ParameterHandler::sendParameterChange(const ParameterDef* param, uint8_t value) {
    if (!param || !m_wsClient || !m_wsClient->isConnected()) {
        return;
    }

    // Send command based on parameter type using existing methods
    switch (param->id) {
        // Unit A (0-7) - Core parameters with dedicated methods
        case ParameterId::EffectId:
            m_wsClient->sendEffectChange(value);
            break;
        case ParameterId::Brightness:
            m_wsClient->sendBrightnessChange(value);
            break;
        case ParameterId::PaletteId:
            m_wsClient->sendPaletteChange(value);
            break;
        case ParameterId::Speed:
            m_wsClient->sendSpeedChange(value);
            break;
        case ParameterId::Intensity:
            m_wsClient->sendIntensityChange(value);
            break;
        case ParameterId::Saturation:
            m_wsClient->sendSaturationChange(value);
            break;
        case ParameterId::Complexity:
            m_wsClient->sendComplexityChange(value);
            break;
        case ParameterId::Variation:
            m_wsClient->sendVariationChange(value);
            break;

        // Unit B (8-15) - Zone parameters
        // These use generic parameter.set with field name
        case ParameterId::Zone0Effect:
            m_wsClient->sendGenericParameter("zone0Effect", value);
            break;
        case ParameterId::Zone0Brightness:
            m_wsClient->sendGenericParameter("zone0Brightness", value);
            break;
        case ParameterId::Zone1Effect:
            m_wsClient->sendGenericParameter("zone1Effect", value);
            break;
        case ParameterId::Zone1Brightness:
            m_wsClient->sendGenericParameter("zone1Brightness", value);
            break;
        case ParameterId::Zone2Effect:
            m_wsClient->sendGenericParameter("zone2Effect", value);
            break;
        case ParameterId::Zone2Brightness:
            m_wsClient->sendGenericParameter("zone2Brightness", value);
            break;
        case ParameterId::Zone3Effect:
            m_wsClient->sendGenericParameter("zone3Effect", value);
            break;
        case ParameterId::Zone3Brightness:
            m_wsClient->sendGenericParameter("zone3Brightness", value);
            break;
    }
}

uint8_t ParameterHandler::clampValue(const ParameterDef* param, uint8_t value) const {
    if (!param) {
        return value;
    }

    if (value < param->min) {
        return param->min;
    }
    if (value > param->max) {
        return param->max;
    }
    return value;
}

void ParameterHandler::notifyDisplay(int index) {
    if (!m_displayCallback) {
        return;
    }

    if (index >= 0 && index < PARAMETER_COUNT) {
        // Single parameter update
        m_displayCallback(static_cast<uint8_t>(index), m_values[index]);
    } else {
        // Bulk refresh: notify all parameters
        for (uint8_t i = 0; i < PARAMETER_COUNT; i++) {
            m_displayCallback(i, m_values[i]);
        }
    }
}
