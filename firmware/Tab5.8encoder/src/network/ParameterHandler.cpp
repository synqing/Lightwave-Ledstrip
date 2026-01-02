#include "ParameterHandler.h"
#include "WebSocketClient.h"
#include "../processing/EncoderProcessing.h"
#include <Arduino.h>
#include <cstring>

ParameterHandler::ParameterHandler(
    EncoderProcessing* processing,
    WebSocketClient* wsClient
)
    : m_processing(processing)
    , m_wsClient(wsClient)
{
    // Initialize with default values
    for (uint8_t i = 0; i < 8; i++) {
        const ParameterDef* param = getParameterByIndex(i);
        if (param) {
            m_values[i] = param->defaultValue;
        } else {
            m_values[i] = 0;
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

    // Send to LightwaveOS via WebSocket (only if connected)
    if (m_wsClient->isConnected()) {
        sendParameterChange(param, clampedValue);
    }
}

bool ParameterHandler::applyStatus(StaticJsonDocument<512>& doc) {
    if (!doc.containsKey("type") || strcmp(doc["type"], "status") != 0) {
        return false;
    }

    bool updated = false;

    // Apply each parameter from status message
    for (uint8_t i = 0; i < getParameterCount(); i++) {
        const ParameterDef* param = getParameterByIndex(i);
        if (!param) continue;

        // Check if this field exists in the status message
        if (doc.containsKey(param->statusField)) {
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
                continue; // Skip invalid types
            }

            // Clamp to valid range
            newValue = clampValue(param, newValue);

            // Only update if value changed (avoid echo loops)
            if (m_values[i] != newValue) {
                m_values[i] = newValue;

                // Update encoder processing (without triggering callback to avoid echo)
                if (m_processing) {
                    m_processing->setValue(i, newValue, false); // false = don't trigger callback
                }

                updated = true;
            }
        }
    }

    return updated;
}

uint8_t ParameterHandler::getValue(ParameterId id) const {
    uint8_t index = static_cast<uint8_t>(id);
    if (index >= 8) {
        return 0;
    }
    return m_values[index];
}

void ParameterHandler::setValue(ParameterId id, uint8_t value) {
    uint8_t index = static_cast<uint8_t>(id);
    if (index >= 8) {
        return;
    }
    m_values[index] = value;
}

void ParameterHandler::sendParameterChange(const ParameterDef* param, uint8_t value) {
    if (!param || !m_wsClient || !m_wsClient->isConnected()) {
        return;
    }

    // Send command based on parameter type
    switch (param->id) {
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

