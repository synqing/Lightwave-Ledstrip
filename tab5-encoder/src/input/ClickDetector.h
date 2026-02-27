// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once
// ============================================================================
// ClickDetector - Multi-Click Pattern Detection for Encoder Buttons
// ============================================================================
// Detects single-click, double-click, and long-hold patterns on encoder buttons.
//
// Click Patterns:
//   - SINGLE_CLICK: Press and release within CLICK_TIMEOUT, no second click follows
//   - DOUBLE_CLICK: Two presses within DOUBLE_CLICK_WINDOW
//   - LONG_HOLD: Press held for LONG_HOLD_THRESHOLD without release
//
// State Machine:
//   IDLE -> (press) -> PRESSED
//   PRESSED -> (release < LONG_HOLD) -> WAIT_FOR_DOUBLE
//   PRESSED -> (held >= LONG_HOLD) -> emit LONG_HOLD -> COOLDOWN
//   WAIT_FOR_DOUBLE -> (press within window) -> emit DOUBLE_CLICK -> COOLDOWN
//   WAIT_FOR_DOUBLE -> (timeout) -> emit SINGLE_CLICK -> IDLE
//   COOLDOWN -> (release) -> IDLE
//
// Usage:
//   ClickDetector detector;
//   ClickType result = detector.update(isPressed, millis());
//   if (result == ClickType::DOUBLE_CLICK) { savePreset(); }
// ============================================================================

#ifndef CLICK_DETECTOR_H
#define CLICK_DETECTOR_H

#include <Arduino.h>

// Click pattern types
enum class ClickType : uint8_t {
    NONE = 0,       // No click event (update in progress)
    SINGLE_CLICK,   // Single press-release (recall preset)
    DOUBLE_CLICK,   // Two quick presses (save preset)
    LONG_HOLD       // Press held for threshold (delete preset)
};

class ClickDetector {
public:
    // Timing constants (milliseconds)
    static constexpr uint32_t DOUBLE_CLICK_WINDOW_MS = 300;  // Time to wait for second click
    static constexpr uint32_t LONG_HOLD_THRESHOLD_MS = 1000; // Time to detect long hold
    static constexpr uint32_t DEBOUNCE_MS = 20;              // Button debounce time
    static constexpr uint32_t COOLDOWN_MS = 100;             // Post-event cooldown

    /**
     * Update detector with current button state
     * @param isPressed Current button pressed state
     * @param now Current time in milliseconds (millis())
     * @return ClickType if pattern detected, NONE if no event
     */
    ClickType update(bool isPressed, uint32_t now);

    /**
     * Reset detector to idle state
     * Call when switching screens or after handling an event externally
     */
    void reset();

    /**
     * Check if detector is currently tracking a button press
     * @return true if button is being held or waiting for double-click
     */
    bool isActive() const;

    /**
     * Get time remaining before single-click is emitted
     * Useful for UI feedback (e.g., countdown indicator)
     * @return Milliseconds remaining, or 0 if not in wait state
     */
    uint32_t getTimeToSingleClick(uint32_t now) const;

    /**
     * Get time remaining before long-hold is triggered
     * Useful for UI feedback (e.g., progress bar)
     * @param now Current time in milliseconds
     * @return Milliseconds remaining, or 0 if not pressed
     */
    uint32_t getTimeToLongHold(uint32_t now) const;

private:
    enum class State : uint8_t {
        IDLE,             // Waiting for button press
        PRESSED,          // Button is currently pressed
        WAIT_FOR_DOUBLE,  // Released, waiting for potential second click
        COOLDOWN          // Event emitted, waiting for release
    };

    State _state = State::IDLE;
    uint32_t _pressTime = 0;       // When button was pressed
    uint32_t _releaseTime = 0;     // When button was released
    uint32_t _cooldownEnd = 0;     // When cooldown ends
    bool _lastPressed = false;     // Last button state (for edge detection)
};

// ============================================================================
// Inline Implementation
// ============================================================================

inline ClickType ClickDetector::update(bool isPressed, uint32_t now) {
    ClickType result = ClickType::NONE;

    // Detect edges (button down / button up)
    bool justPressed = isPressed && !_lastPressed;
    bool justReleased = !isPressed && _lastPressed;
    _lastPressed = isPressed;

    switch (_state) {
        case State::IDLE:
            if (justPressed) {
                _pressTime = now;
                _state = State::PRESSED;
            }
            break;

        case State::PRESSED:
            if (justReleased) {
                // Released before long-hold threshold
                uint32_t holdDuration = now - _pressTime;
                if (holdDuration >= DEBOUNCE_MS) {
                    // Valid press duration - wait for potential double-click
                    _releaseTime = now;
                    _state = State::WAIT_FOR_DOUBLE;
                } else {
                    // Too short - ignore (debounce)
                    _state = State::IDLE;
                }
            } else if (isPressed) {
                // Still pressed - check for long hold
                uint32_t holdDuration = now - _pressTime;
                if (holdDuration >= LONG_HOLD_THRESHOLD_MS) {
                    result = ClickType::LONG_HOLD;
                    _cooldownEnd = now + COOLDOWN_MS;
                    _state = State::COOLDOWN;
                }
            }
            break;

        case State::WAIT_FOR_DOUBLE:
            if (justPressed) {
                // Second press within window - double click!
                uint32_t gap = now - _releaseTime;
                if (gap <= DOUBLE_CLICK_WINDOW_MS) {
                    result = ClickType::DOUBLE_CLICK;
                    _cooldownEnd = now + COOLDOWN_MS;
                    _state = State::COOLDOWN;
                } else {
                    // Too slow - treat previous as single, this as new press
                    result = ClickType::SINGLE_CLICK;
                    _pressTime = now;
                    _state = State::PRESSED;
                }
            } else {
                // No second press - check timeout
                uint32_t waitDuration = now - _releaseTime;
                if (waitDuration > DOUBLE_CLICK_WINDOW_MS) {
                    // Timeout - emit single click
                    result = ClickType::SINGLE_CLICK;
                    _state = State::IDLE;
                }
            }
            break;

        case State::COOLDOWN:
            // Wait for button release and cooldown to complete
            if (!isPressed && now >= _cooldownEnd) {
                _state = State::IDLE;
            }
            break;
    }

    return result;
}

inline void ClickDetector::reset() {
    _state = State::IDLE;
    _pressTime = 0;
    _releaseTime = 0;
    _cooldownEnd = 0;
    _lastPressed = false;
}

inline bool ClickDetector::isActive() const {
    return _state != State::IDLE;
}

inline uint32_t ClickDetector::getTimeToSingleClick(uint32_t now) const {
    if (_state != State::WAIT_FOR_DOUBLE) return 0;

    uint32_t elapsed = now - _releaseTime;
    if (elapsed >= DOUBLE_CLICK_WINDOW_MS) return 0;

    return DOUBLE_CLICK_WINDOW_MS - elapsed;
}

inline uint32_t ClickDetector::getTimeToLongHold(uint32_t now) const {
    if (_state != State::PRESSED) return 0;

    uint32_t elapsed = now - _pressTime;
    if (elapsed >= LONG_HOLD_THRESHOLD_MS) return 0;

    return LONG_HOLD_THRESHOLD_MS - elapsed;
}

#endif // CLICK_DETECTOR_H
