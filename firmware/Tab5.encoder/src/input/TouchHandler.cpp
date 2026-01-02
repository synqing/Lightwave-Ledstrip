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
        return;
    }

    // Record touch start
    m_touchStartTime = now;
    m_touchX = x;
    m_touchY = y;
    m_longPressTriggered = false;

    // Determine which zone/parameter was touched
    TouchZone zone = hitTestZone(x, y);

    if (zone == TouchZone::PARAMETER_GRID) {
        m_touchedParam = hitTestParameter(x, y);

        if (m_touchedParam >= 0) {
            Serial.printf("[TOUCH] Start on param %d at (%d,%d)\n",
                          m_touchedParam, x, y);
        }
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

    // If long press was triggered, don't process as tap
    if (m_longPressTriggered) {
        m_touchedParam = -1;
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
            if (zone == TouchZone::STATUS_BAR && m_statusBarCallback) {
                m_statusBarCallback(m_touchX, m_touchY);
            }
        }
    }

    // Clear touched param
    m_touchedParam = -1;
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
