// ============================================================================
// TouchHandler.cpp - Touch Screen Integration Implementation
// ============================================================================
// Phase 7 (G.3): Touch-based parameter control for Tab5.encoder
// ============================================================================

#include "TouchHandler.h"
#include "DualEncoderService.h"
#include "../config/Config.h"

// ============================================================================
// Update - Main Touch Polling Loop
// ============================================================================

void TouchHandler::update() {
    // M5.update() must be called before this (handled in main loop)
    auto touch = M5.Touch.getDetail();
    uint32_t now = millis();

    // Get current touch state
    bool isPressed = touch.isPressed();
    int16_t x = touch.x;
    int16_t y = touch.y;

    // ========================================================================
    // State Machine: Detect touch start, hold, and release
    // ========================================================================

    if (isPressed && !m_wasPressed) {
        // Touch just started
        handleTouchStart(x, y);
    }
    else if (isPressed && m_wasPressed) {
        // Touch being held - check for long press
        uint32_t duration = now - m_touchStartTime;
        handleTouchHold(x, y, duration);
    }
    else if (!isPressed && m_wasPressed) {
        // Touch just released
        uint32_t duration = now - m_touchStartTime;
        handleTouchRelease(duration);
    }

    // Update state for next frame
    m_wasPressed = isPressed;
    m_touching = isPressed;

    if (isPressed) {
        m_touchX = x;
        m_touchY = y;
    }
}

// ============================================================================
// Touch Start Handler
// ============================================================================

void TouchHandler::handleTouchStart(int16_t x, int16_t y) {
    uint32_t now = millis();

    // Debounce check - ignore rapid touch events
    if (now - m_lastEventTime < TouchConfig::DEBOUNCE_MS) {
        // #region agent log
        Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H5\",\"location\":\"TouchHandler.cpp:62\",\"message\":\"touch.start.debounced\",\"data\":{\"x\":%d,\"y\":%d,\"now\":%lu,\"lastEvent\":%lu,\"delta\":%lu},\"timestamp\":%lu}\n", x, y, (unsigned long)now, (unsigned long)m_lastEventTime, (unsigned long)(now - m_lastEventTime), (unsigned long)now);
        // #endregion
        return;
    }

    // Record touch start
    m_touchStartTime = now;
    m_touchX = x;
    m_touchY = y;
    m_longPressTriggered = false;
    m_touchedAction = -1;

    // Determine which zone/parameter was touched
    TouchZone zone = hitTestZone(x, y);
    // #region agent log
    Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H3\",\"location\":\"TouchHandler.cpp:74\",\"message\":\"touch.start.zone\",\"data\":{\"x\":%d,\"y\":%d,\"zone\":%d,\"actionRowYStart\":%d,\"actionRowYEnd\":%d},\"timestamp\":%lu}\n", x, y, (int)zone, TouchConfig::ACTION_ROW_Y_START, TouchConfig::ACTION_ROW_Y_END, (unsigned long)now);
    // #endregion

    if (zone == TouchZone::PARAMETER_GRID) {
        m_touchedParam = hitTestParameter(x, y);

        if (m_touchedParam >= 0) {
            Serial.printf("[TOUCH] Start on param %d at (%d,%d)\n",
                          m_touchedParam, x, y);
        }
    }
    else if (zone == TouchZone::ACTION_ROW) {
        m_touchedParam = -1;
        m_touchedAction = hitTestActionButton(x, y);
        // #region agent log
        Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1,H2\",\"location\":\"TouchHandler.cpp:90\",\"message\":\"touch.start.actionRow\",\"data\":{\"x\":%d,\"y\":%d,\"touchedAction\":%d,\"buttonW\":%d,\"screenW\":%d,\"actionRowYStart\":%d,\"actionRowYEnd\":%d,\"calculatedButtonIdx\":%d},\"timestamp\":%lu}\n", x, y, m_touchedAction, TouchConfig::ACTION_BUTTON_W, TouchConfig::SCREEN_WIDTH, TouchConfig::ACTION_ROW_Y_START, TouchConfig::ACTION_ROW_Y_END, (x / TouchConfig::ACTION_BUTTON_W), (unsigned long)now);
        // #endregion
    }
    else if (zone == TouchZone::STATUS_BAR) {
        m_touchedParam = -1;
        Serial.printf("[TOUCH] Start on status bar at (%d,%d)\n", x, y);
    }
    else {
        m_touchedParam = -1;
    }
}

// ============================================================================
// Touch Hold Handler (Long Press Detection)
// ============================================================================

void TouchHandler::handleTouchHold(int16_t x, int16_t y, uint32_t duration) {
    // Only trigger long press once per touch
    if (m_longPressTriggered) {
        return;
    }

    // Check if we've exceeded the long press threshold
    if (duration >= TouchConfig::LONG_PRESS_THRESHOLD_MS) {
        m_longPressTriggered = true;
        m_lastEventTime = millis();

        // Check if we're still on the same parameter cell
        int8_t currentParam = hitTestParameter(x, y);

        if (currentParam >= 0 && currentParam == m_touchedParam) {
            Serial.printf("[TOUCH] Long press on param %d (duration %lu ms)\n",
                          currentParam, duration);

            // Reset parameter to default
            resetParameterToDefault(static_cast<uint8_t>(currentParam));

            // Invoke callback if registered
            if (m_longPressCallback) {
                m_longPressCallback(static_cast<uint8_t>(currentParam));
            }
        }
    }
}

// ============================================================================
// Touch Release Handler (Tap Detection)
// ============================================================================

void TouchHandler::handleTouchRelease(uint32_t duration) {
    uint32_t now = millis();

    // #region agent log
    Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H2\",\"location\":\"TouchHandler.cpp:134\",\"message\":\"touch.release.entry\",\"data\":{\"duration\":%lu,\"longPressTriggered\":%d,\"touchedParam\":%d,\"touchedAction\":%d,\"touchX\":%d,\"touchY\":%d},\"timestamp\":%lu}\n", (unsigned long)duration, m_longPressTriggered ? 1 : 0, m_touchedParam, m_touchedAction, m_touchX, m_touchY, (unsigned long)now);
    // #endregion

    // If long press was triggered, don't process as tap
    if (m_longPressTriggered) {
        m_touchedParam = -1;
        // #region agent log
        Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H5\",\"location\":\"TouchHandler.cpp:140\",\"message\":\"touch.release.longPress\",\"data\":{\"duration\":%lu},\"timestamp\":%lu}\n", (unsigned long)duration, (unsigned long)now);
        // #endregion
        return;
    }

    // Check if this qualifies as a tap (short duration)
    if (duration <= TouchConfig::TAP_MAX_MS) {
        m_lastEventTime = now;

        if (m_touchedParam >= 0) {
            Serial.printf("[TOUCH] Tap on param %d (duration %lu ms)\n",
                          m_touchedParam, duration);

            // Invoke tap callback if registered
            if (m_tapCallback) {
                m_tapCallback(static_cast<uint8_t>(m_touchedParam));
            }
        }
        else {
            // Status bar tap
            TouchZone zone = hitTestZone(m_touchX, m_touchY);
            // #region agent log
            Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H3\",\"location\":\"TouchHandler.cpp:158\",\"message\":\"touch.release.zoneCheck\",\"data\":{\"zone\":%d,\"touchX\":%d,\"touchY\":%d,\"actionRowYStart\":%d,\"actionRowYEnd\":%d},\"timestamp\":%lu}\n", (int)zone, m_touchX, m_touchY, TouchConfig::ACTION_ROW_Y_START, TouchConfig::ACTION_ROW_Y_END, (unsigned long)now);
            // #endregion
            if (zone == TouchZone::STATUS_BAR && m_statusBarCallback) {
                m_statusBarCallback(m_touchX, m_touchY);
            }
            // #region agent log
            Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H4\",\"location\":\"TouchHandler.cpp:181\",\"message\":\"touch.release.actionRowCheck\",\"data\":{\"zone\":%d,\"zoneIsActionRow\":%d,\"callbackSet\":%d,\"touchedAction\":%d,\"touchX\":%d,\"touchY\":%d,\"actionRowYStart\":%d,\"actionRowYEnd\":%d},\"timestamp\":%lu}\n", (int)zone, (zone == TouchZone::ACTION_ROW) ? 1 : 0, (m_actionButtonCallback != nullptr) ? 1 : 0, m_touchedAction, m_touchX, m_touchY, TouchConfig::ACTION_ROW_Y_START, TouchConfig::ACTION_ROW_Y_END, (unsigned long)now);
            // #endregion
            if (zone == TouchZone::ACTION_ROW && m_actionButtonCallback && m_touchedAction >= 0) {
                // #region agent log
                Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H4\",\"location\":\"TouchHandler.cpp:184\",\"message\":\"touch.release.callbackInvoke\",\"data\":{\"buttonIndex\":%d},\"timestamp\":%lu}\n", m_touchedAction, (unsigned long)now);
                // #endregion
                m_actionButtonCallback(static_cast<uint8_t>(m_touchedAction));
            } else {
                // #region agent log
                const char* reason = "unknown";
                if (zone != TouchZone::ACTION_ROW) reason = "zone_mismatch";
                else if (!m_actionButtonCallback) reason = "callback_not_set";
                else if (m_touchedAction < 0) reason = "touchedAction_invalid";
                Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1,H2,H3,H4\",\"location\":\"TouchHandler.cpp:189\",\"message\":\"touch.release.actionRowFailed\",\"data\":{\"zone\":%d,\"zoneIsActionRow\":%d,\"callbackSet\":%d,\"touchedAction\":%d,\"reason\":\"%s\",\"touchX\":%d,\"touchY\":%d},\"timestamp\":%lu}\n", (int)zone, (zone == TouchZone::ACTION_ROW) ? 1 : 0, (m_actionButtonCallback != nullptr) ? 1 : 0, m_touchedAction, reason, m_touchX, m_touchY, (unsigned long)now);
                // #endregion
            }
        }
    } else {
        // #region agent log
        Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H5\",\"location\":\"TouchHandler.cpp:144\",\"message\":\"touch.release.durationTooLong\",\"data\":{\"duration\":%lu,\"tapMax\":%lu},\"timestamp\":%lu}\n", (unsigned long)duration, (unsigned long)TouchConfig::TAP_MAX_MS, (unsigned long)now);
        // #endregion
    }

    // Clear touched param
    m_touchedParam = -1;
    m_touchedAction = -1;
}

// ============================================================================
// Reset Parameter to Default
// ============================================================================

void TouchHandler::resetParameterToDefault(uint8_t paramIndex) {
    if (!m_encoderService) {
        Serial.printf("[TOUCH] No encoder service - cannot reset param %d\n",
                      paramIndex);
        return;
    }

    if (paramIndex >= TouchConfig::TOTAL_CELLS) {
        return;
    }

    // Get default value for this parameter
    Parameter param = static_cast<Parameter>(paramIndex);
    uint16_t defaultValue = getParameterDefault(param);

    // Set the value through the encoder service (triggers callback)
    m_encoderService->setValue(paramIndex, defaultValue, true);

    Serial.printf("[TOUCH] Reset param %d (%s) to default %d\n",
                  paramIndex,
                  getParameterName(param),
                  defaultValue);
}
