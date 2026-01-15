// ============================================================================
// ParameterHandler - Parameter Synchronization for Tab5.encoder
// ============================================================================
// Adapted from K1.8encoderS3 for Tab5's EncoderService interface.
// Extended for 16 parameters across dual M5ROTATE8 units.
// ============================================================================

#include "ParameterHandler.h"
#include "ParameterMap.h"
#include "../input/DualEncoderService.h"
#include "../input/ButtonHandler.h"
#include "../hub/state/HubState.h"
#include "../config/AnsiColors.h"
#include "../config/Config.h"
#include "../storage/NvsStorage.h"
#include <Arduino.h>
#include <cstring>

#if ENABLE_LEGACY_WIFI_CLIENT
#include "../network/WebSocketClient.h"
#endif


ParameterHandler::ParameterHandler(
    DualEncoderService* encoderService,
    WebSocketClient* wsClient,
    HubState* hubState
)
    : m_encoderService(encoderService)
    , m_wsClient(wsClient)
    , m_hubState(hubState)
    , m_displayCallback(nullptr)
{
    // Sync cache with encoder service values (which have NVS-restored data)
    // This ensures ParameterHandler cache matches hardware state from startup.
    for (uint8_t i = 0; i < PARAMETER_COUNT; i++) {
        if (m_encoderService) {
            m_values[i] = m_encoderService->getValue(i);
        } else {
            // Fallback to defaults if encoder service not available
            const ParameterDef* param = getParameterByIndex(i);
            m_values[i] = param ? param->defaultValue : 128;
        }
    }
}

void ParameterHandler::onEncoderChanged(uint8_t index, uint16_t value, bool wasReset) {
    const ParameterDef* param = getParameterByIndex(index);
    if (!param || (!m_wsClient && !m_hubState)) {
        return;
    }

    // Mark this parameter as locally “authoritative” for a short window to prevent
    // server status echo from snapping the UI/encoder back and forth.
    m_lastLocalChangeMs[index] = millis();

    // Clamp value to valid range
    uint8_t clampedValue = clampValue(param, static_cast<uint8_t>(value));

    // Update local state
    m_values[index] = clampedValue;

    // Persist (debounced) so a reboot restores encoder state.
    NvsStorage::requestSave(index, clampedValue);

    // Notify display (with highlight)
    notifyDisplay(index);

    // Send to HubState (preferred in hub mode), or legacy WebSocket client mode.
    sendParameterChange(param, clampedValue);

    // Debug output
    LOG_PARAM("%s: %d%s", param->statusField, clampedValue, wasReset ? " (reset)" : "");
}

void ParameterHandler::applyLocalValue(uint8_t index, uint16_t value, bool writeEncoder) {
    const ParameterDef* param = getParameterByIndex(index);
    if (!param) return;

    uint8_t clampedValue = clampValue(param, static_cast<uint8_t>(value));
    m_values[index] = clampedValue;
    m_lastLocalChangeMs[index] = millis();

    if (writeEncoder && m_encoderService) {
        m_encoderService->setValue(index, clampedValue, false);
    }

    notifyDisplay(index);
}

bool ParameterHandler::applyStatus(JsonDocument& doc) {
    // Check type field using ArduinoJson 7 pattern (is<T>() instead of containsKey)
    if (!doc["type"].is<const char*>() || strcmp(doc["type"], "status") != 0) {
        return false;
    }

    bool updated = false;
    uint32_t nowMs = millis();

    // Apply each parameter from status message
    for (uint8_t i = 0; i < getParameterCount(); i++) {
        const ParameterDef* param = getParameterByIndex(i);
        if (!param) continue;

        // If this parameter was just changed locally, ignore server status for a short time.
        // This prevents snapback/jitter when LightwaveOS broadcasts status slightly behind.
        if (m_lastLocalChangeMs[i] != 0 && (nowMs - m_lastLocalChangeMs[i] < LOCAL_OVERRIDE_HOLDOFF_MS)) {
            continue;  // Holdoff active - anti-snapback protection
        }

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

                LOG_PARAM("Synced %s: %d", param->statusField, newValue);
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
    if (!param) {
        return;
    }

    // Hub mode: write into HubState and let HubMain batch/broadcast.
    if (m_hubState) {
        switch (param->id) {
            case ParameterId::EffectId:
                m_hubState->setGlobalEffect(value);
                break;
            case ParameterId::PaletteId:
                m_hubState->setGlobalPalette(value);
                break;
            case ParameterId::Speed:
                m_hubState->setGlobalSpeed(value);
                break;
            // Tab5 legacy labels: map to K1 modern global params (intensity/saturation).
            case ParameterId::Mood:
                m_hubState->setGlobalIntensity(value);
                break;
            case ParameterId::FadeAmount:
                m_hubState->setGlobalSaturation(value);
                break;
            case ParameterId::Brightness:
                m_hubState->setGlobalBrightness(value);
                break;
            case ParameterId::Complexity:
                m_hubState->setGlobalComplexity(value);
                break;
            case ParameterId::Variation:
                m_hubState->setGlobalVariation(value);
                break;

            case ParameterId::Zone0Effect:
                m_hubState->setZoneEffectAll(0, value);
                break;
            case ParameterId::Zone0Speed:
                if (m_buttonHandler && m_buttonHandler->getZoneEncoderMode(0) == SpeedPaletteMode::PALETTE) {
                    m_hubState->setZonePaletteAll(0, value);
                } else {
                    m_hubState->setZoneSpeedAll(0, value);
                }
                break;
            case ParameterId::Zone1Effect:
                m_hubState->setZoneEffectAll(1, value);
                break;
            case ParameterId::Zone1Speed:
                if (m_buttonHandler && m_buttonHandler->getZoneEncoderMode(1) == SpeedPaletteMode::PALETTE) {
                    m_hubState->setZonePaletteAll(1, value);
                } else {
                    m_hubState->setZoneSpeedAll(1, value);
                }
                break;
            case ParameterId::Zone2Effect:
                m_hubState->setZoneEffectAll(2, value);
                break;
            case ParameterId::Zone2Speed:
                if (m_buttonHandler && m_buttonHandler->getZoneEncoderMode(2) == SpeedPaletteMode::PALETTE) {
                    m_hubState->setZonePaletteAll(2, value);
                } else {
                    m_hubState->setZoneSpeedAll(2, value);
                }
                break;
            case ParameterId::Zone3Effect:
                m_hubState->setZoneEffectAll(3, value);
                break;
            case ParameterId::Zone3Speed:
                if (m_buttonHandler && m_buttonHandler->getZoneEncoderMode(3) == SpeedPaletteMode::PALETTE) {
                    m_hubState->setZonePaletteAll(3, value);
                } else {
                    m_hubState->setZoneSpeedAll(3, value);
                }
                break;
        }
        return;
    }

    // Legacy client mode: send command based on parameter type using existing methods.
#if !ENABLE_LEGACY_WIFI_CLIENT
    (void)value;
    return;
#else
    if (!m_wsClient || !m_wsClient->isConnected()) {
        return;
    }

    switch (param->id) {
        // Unit A (0-7) - Global parameters with dedicated methods
        case ParameterId::EffectId:
            m_wsClient->sendEffectChange(value);
            break;
        case ParameterId::PaletteId:
            m_wsClient->sendPaletteChange(value);
            break;
        case ParameterId::Speed:
            m_wsClient->sendSpeedChange(value);
            break;
        case ParameterId::Mood:
            m_wsClient->sendMoodChange(value);
            break;
        case ParameterId::FadeAmount:
            m_wsClient->sendFadeAmountChange(value);
            break;
        case ParameterId::Brightness:
            m_wsClient->sendBrightnessChange(value);
            break;
        case ParameterId::Complexity:
            m_wsClient->sendComplexityChange(value);
            break;
        case ParameterId::Variation:
            m_wsClient->sendVariationChange(value);
            break;

        // Unit B (8-15) - Zone parameters
        // Zone effects use zones.setEffect (plural)
        // Zone speed/palette changes use zones.update (can toggle to palette mode via button)
        case ParameterId::Zone0Effect:
            m_wsClient->sendZoneEffect(0, value);
            break;
        case ParameterId::Zone0Speed:
            // Check if button toggled to palette mode
            if (m_buttonHandler && m_buttonHandler->getZoneEncoderMode(0) == SpeedPaletteMode::PALETTE) {
                m_wsClient->sendZonePalette(0, value);
            } else {
                m_wsClient->sendZoneSpeed(0, value);
            }
            break;
        case ParameterId::Zone1Effect:
            m_wsClient->sendZoneEffect(1, value);
            break;
        case ParameterId::Zone1Speed:
            // Check if button toggled to palette mode
            if (m_buttonHandler && m_buttonHandler->getZoneEncoderMode(1) == SpeedPaletteMode::PALETTE) {
                m_wsClient->sendZonePalette(1, value);
            } else {
                m_wsClient->sendZoneSpeed(1, value);
            }
            break;
        case ParameterId::Zone2Effect:
            m_wsClient->sendZoneEffect(2, value);
            break;
        case ParameterId::Zone2Speed:
            // Check if button toggled to palette mode
            if (m_buttonHandler && m_buttonHandler->getZoneEncoderMode(2) == SpeedPaletteMode::PALETTE) {
                m_wsClient->sendZonePalette(2, value);
            } else {
                m_wsClient->sendZoneSpeed(2, value);
            }
            break;
        case ParameterId::Zone3Effect:
            m_wsClient->sendZoneEffect(3, value);
            break;
        case ParameterId::Zone3Speed:
            // Check if button toggled to palette mode
            if (m_buttonHandler && m_buttonHandler->getZoneEncoderMode(3) == SpeedPaletteMode::PALETTE) {
                m_wsClient->sendZonePalette(3, value);
            } else {
                m_wsClient->sendZoneSpeed(3, value);
            }
            break;
    }
#endif
}

uint8_t ParameterHandler::clampValue(const ParameterDef* param, uint8_t value) const {
    if (!param) {
        return value;
    }

    // Use dynamic min/max from ParameterMap (falls back to hardcoded if not available)
    uint8_t min = getParameterMin(param->encoderIndex);
    uint8_t max = getParameterMax(param->encoderIndex);

    // Zone speed/palette encoders use a runtime range based on toggle mode.
    // In palette mode, we want the palette range (0..74) rather than speed (1..100).
    if (m_buttonHandler) {
        uint8_t zoneId = 0xFF;
        switch (param->id) {
            case ParameterId::Zone0Speed: zoneId = 0; break;
            case ParameterId::Zone1Speed: zoneId = 1; break;
            case ParameterId::Zone2Speed: zoneId = 2; break;
            case ParameterId::Zone3Speed: zoneId = 3; break;
            default: break;
        }
        if (zoneId != 0xFF) {
            const bool paletteMode = (m_buttonHandler->getZoneEncoderMode(zoneId) == SpeedPaletteMode::PALETTE);
            if (paletteMode) {
                min = ParamRange::ZONE_PALETTE_MIN;
                max = ParamRange::ZONE_PALETTE_MAX;
            } else {
                min = ParamRange::ZONE_SPEED_MIN;
                max = ParamRange::ZONE_SPEED_MAX;
            }
        }
    }

    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
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
