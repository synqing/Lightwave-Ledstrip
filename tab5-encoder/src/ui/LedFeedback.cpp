// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
// ============================================================================
// LedFeedback - Connection Status LED Feedback for Tab5.encoder
// ============================================================================
// Implementation: Non-blocking breathing LED animations for connection status.
// Uses sine wave for smooth breathing effect on Unit A LED 8.
// Note: Unit B LED 8 is now used for palette display.
// ============================================================================

#include "LedFeedback.h"
#include "../input/DualEncoderService.h"
#include <math.h>

// ============================================================================
// Constructors
// ============================================================================

LedFeedback::LedFeedback()
    : m_encoders(nullptr)
    , m_state(ConnectionState::WIFI_DISCONNECTED)
    , m_animationStartTime(0)
    , m_isBreathing(false)
    , m_baseColor(255, 0, 0)  // Default to red (disconnected)
{
}

LedFeedback::LedFeedback(DualEncoderService* encoders)
    : m_encoders(encoders)
    , m_state(ConnectionState::WIFI_DISCONNECTED)
    , m_animationStartTime(0)
    , m_isBreathing(false)
    , m_baseColor(255, 0, 0)  // Default to red (disconnected)
{
}

// ============================================================================
// Initialization
// ============================================================================

void LedFeedback::setEncoders(DualEncoderService* encoders) {
    m_encoders = encoders;
}

void LedFeedback::begin() {
    // Initialize to disconnected state (solid red)
    setState(ConnectionState::WIFI_DISCONNECTED);
}

// ============================================================================
// State Management
// ============================================================================

void LedFeedback::setState(ConnectionState state) {
    // Skip if same state (avoid animation reset)
    if (state == m_state) return;

    m_state = state;
    m_baseColor = getColorForState(state);
    m_isBreathing = stateRequiresBreathing(state);
    m_animationStartTime = millis();

    // For non-breathing states, apply color immediately
    if (!m_isBreathing) {
        applyColorToBothUnits(m_baseColor);
    }
}

const char* LedFeedback::getStateString() const {
    switch (m_state) {
        case ConnectionState::WIFI_DISCONNECTED: return "WIFI_DISC";
        case ConnectionState::WIFI_CONNECTING:   return "WIFI_CONN";
        case ConnectionState::WIFI_CONNECTED:    return "WIFI_OK";
        case ConnectionState::WS_CONNECTING:     return "WS_CONN";
        case ConnectionState::WS_CONNECTED:      return "WS_OK";
        case ConnectionState::WS_RECONNECTING:   return "WS_RECON";
        default:                                 return "UNKNOWN";
    }
}

// ============================================================================
// Color Mapping
// ============================================================================

StatusLedColor LedFeedback::getColorForState(ConnectionState state) {
    switch (state) {
        case ConnectionState::WIFI_DISCONNECTED:
            // Solid red
            return StatusLedColor(255, 0, 0);

        case ConnectionState::WIFI_CONNECTING:
            // Blue (breathing)
            return StatusLedColor(0, 0, 255);

        case ConnectionState::WIFI_CONNECTED:
            // Solid blue (brief state before WS)
            return StatusLedColor(0, 0, 255);

        case ConnectionState::WS_CONNECTING:
            // Yellow (breathing)
            return StatusLedColor(255, 200, 0);

        case ConnectionState::WS_CONNECTED:
            // Solid green
            return StatusLedColor(0, 255, 0);

        case ConnectionState::WS_RECONNECTING:
            // Orange (breathing)
            return StatusLedColor(255, 100, 0);

        default:
            // Default to red (error)
            return StatusLedColor(255, 0, 0);
    }
}

bool LedFeedback::stateRequiresBreathing(ConnectionState state) {
    switch (state) {
        case ConnectionState::WIFI_CONNECTING:
        case ConnectionState::WS_CONNECTING:
        case ConnectionState::WS_RECONNECTING:
            return true;
        default:
            return false;
    }
}

// ============================================================================
// Animation Update
// ============================================================================

void LedFeedback::update() {
    if (!m_encoders) return;

    if (!m_isBreathing) {
        // No animation needed for solid states
        return;
    }

    // Calculate breathing factor (0.30 to 1.0)
    float factor = calculateBreathingFactor();

    // Apply breathing to base color
    StatusLedColor breathColor;
    breathColor.r = static_cast<uint8_t>(m_baseColor.r * factor);
    breathColor.g = static_cast<uint8_t>(m_baseColor.g * factor);
    breathColor.b = static_cast<uint8_t>(m_baseColor.b * factor);

    applyColorToBothUnits(breathColor);
}

float LedFeedback::calculateBreathingFactor() const {
    // Calculate phase within breathing cycle (0.0 to 1.0)
    uint32_t elapsed = millis() - m_animationStartTime;
    float phase = static_cast<float>(elapsed % BREATHING_PERIOD_MS) / static_cast<float>(BREATHING_PERIOD_MS);

    // Use sine wave for smooth breathing
    // sin(2*PI*phase) maps [0,1] phase to full sine cycle
    // Result is [-1, 1], we map to [0, 1]
    float sineValue = (sinf(phase * 2.0f * M_PI) + 1.0f) / 2.0f;

    // Map to brightness range [MIN_PERCENT, MAX_PERCENT]
    float minFactor = static_cast<float>(BREATHING_MIN_PERCENT) / 100.0f;
    float maxFactor = static_cast<float>(BREATHING_MAX_PERCENT) / 100.0f;

    return minFactor + sineValue * (maxFactor - minFactor);
}

// ============================================================================
// LED Control
// ============================================================================

void LedFeedback::applyColorToBothUnits(const StatusLedColor& color) {
    if (!m_encoders) return;

    // Set LED 8 (status LED) on Unit A (unit index 0)
    // Note: Unit B LED 8 is now used for palette display, so we only set Unit A
    m_encoders->setStatusLed(0, color.r, color.g, color.b);
}

void LedFeedback::allOff() {
    if (!m_encoders) return;

    // Turn off status LED on Unit A
    // Note: Unit B LED 8 is now used for palette display, so we only turn off Unit A
    m_encoders->setStatusLed(0, 0, 0, 0);
}
