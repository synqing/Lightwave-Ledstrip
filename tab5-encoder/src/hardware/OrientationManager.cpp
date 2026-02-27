// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
// ============================================================================
// OrientationManager - Automatic Screen Rotation via IMU
// ============================================================================
// Implementation of IMU-based automatic screen rotation for Tab5.
//
// Algorithm:
// 1. Read accelerometer to detect gravity vector
// 2. Determine dominant axis (X or Y) excluding flat detection (Z)
// 3. Apply hysteresis threshold to prevent jitter
// 4. Debounce to ensure stable orientation before switching
//
// Tab5 BMI270 Axis Orientation (when USB port faces right, screen up):
//   +X = USB side (right)
//   +Y = bottom of device
//   +Z = screen facing up
//
// ============================================================================

#include "OrientationManager.h"
#include <M5Unified.h>
#include <cmath>

// ============================================================================
// Constants
// ============================================================================

// Default configuration values
static constexpr float DEFAULT_THRESHOLD_DEGREES = 30.0f;
static constexpr uint32_t DEFAULT_DEBOUNCE_MS = 500;
static constexpr float DEFAULT_FLAT_THRESHOLD = 0.7f;

// Default rotation (landscape, USB on right)
static constexpr uint8_t DEFAULT_ROTATION = 1;

// ============================================================================
// Constructor
// ============================================================================

OrientationManager::OrientationManager()
    : _enabled(true)
    , _locked(false)
    , _currentRotation(DEFAULT_ROTATION)
    , _pendingRotation(DEFAULT_ROTATION)
    , _pendingStartTime(0)
    , _thresholdDegrees(DEFAULT_THRESHOLD_DEGREES)
    , _thresholdTan(0.0f)
    , _debounceMs(DEFAULT_DEBOUNCE_MS)
    , _flatThreshold(DEFAULT_FLAT_THRESHOLD)
    , _lastAx(0.0f)
    , _lastAy(0.0f)
    , _lastAz(1.0f)  // Default to face-up
    , _callback(nullptr)
{
    updateThresholdTan();
}

// ============================================================================
// Initialization
// ============================================================================

void OrientationManager::begin() {
    // M5.Imu is already initialized by M5.begin()
    // We just ensure our state is consistent

    // Read initial orientation
    float ax, ay, az;
    if (M5.Imu.getAccel(&ax, &ay, &az)) {
        _lastAx = ax;
        _lastAy = ay;
        _lastAz = az;

        // Set initial rotation based on current orientation
        uint8_t detected = detectOrientation(ax, ay, az);
        _currentRotation = detected;
        _pendingRotation = detected;
    }

    Serial.printf("[OrientationMgr] Initialized, rotation=%d, threshold=%.1f deg, debounce=%lu ms\n",
                  _currentRotation, _thresholdDegrees, _debounceMs);
}

// ============================================================================
// Update Loop
// ============================================================================

void OrientationManager::update() {
    // Skip if disabled or locked
    if (!_enabled || _locked) {
        return;
    }

    uint32_t now = millis();

    // Read accelerometer
    float ax, ay, az;
    if (!M5.Imu.getAccel(&ax, &ay, &az)) {
        // IMU read failed - skip this update
        return;
    }

    // Store for debugging
    _lastAx = ax;
    _lastAy = ay;
    _lastAz = az;

    // Detect orientation from gravity vector
    uint8_t detected = detectOrientation(ax, ay, az);

    // Hysteresis logic
    if (detected != _currentRotation) {
        // Different from current - check if stable
        if (detected != _pendingRotation) {
            // New pending orientation - start timer
            _pendingRotation = detected;
            _pendingStartTime = now;
        } else if (now - _pendingStartTime >= _debounceMs) {
            // Stable for debounce period - apply change
            _currentRotation = detected;

            Serial.printf("[OrientationMgr] Rotation changed: %d (ax=%.2f, ay=%.2f, az=%.2f)\n",
                          _currentRotation, ax, ay, az);

            // Invoke callback
            if (_callback) {
                _callback(_currentRotation);
            }
        }
        // else: still waiting for debounce
    } else {
        // Same as current - reset pending state
        _pendingRotation = _currentRotation;
        _pendingStartTime = now;
    }
}

// ============================================================================
// Orientation Detection
// ============================================================================

uint8_t OrientationManager::detectOrientation(float ax, float ay, float az) {
    // If device is flat (Z dominant), don't change rotation
    // This prevents accidental rotation when device is on a table
    if (fabsf(az) > _flatThreshold) {
        return _currentRotation;  // Keep current
    }

    // Determine dominant axis
    float absX = fabsf(ax);
    float absY = fabsf(ay);

    // Rotation mapping based on dominant axis:
    //
    // Tab5 axis orientation (when in rotation 1, landscape, USB right):
    //   +X points right (toward USB)
    //   +Y points down
    //   +Z points up (toward screen)
    //
    // When tilted, gravity pulls toward the "down" side of the device.
    //
    // USB Position | Gravity Direction | Dominant Axis | Rotation
    // -------------|-------------------|---------------|----------
    // USB Right    | +X (tilt right)   | +X            | 1 (landscape)
    // USB Down     | +Y (tilt down)    | +Y            | 0 (portrait)
    // USB Left     | -X (tilt left)    | -X            | 3 (landscape flipped)
    // USB Up       | -Y (tilt up)      | -Y            | 2 (portrait flipped)

    if (absX > absY) {
        // X axis is dominant - landscape orientation
        if (ax > _thresholdTan) {
            return 1;  // USB right (normal landscape)
        } else if (ax < -_thresholdTan) {
            return 3;  // USB left (flipped landscape)
        }
    } else {
        // Y axis is dominant - portrait orientation
        if (ay > _thresholdTan) {
            return 0;  // USB down (portrait)
        } else if (ay < -_thresholdTan) {
            return 2;  // USB up (flipped portrait)
        }
    }

    // No clear orientation (within threshold dead zone)
    return _currentRotation;
}

// ============================================================================
// Callback Registration
// ============================================================================

void OrientationManager::setCallback(RotationCallback callback) {
    _callback = callback;
}

// ============================================================================
// Lock/Unlock
// ============================================================================

void OrientationManager::lockRotation(uint8_t rotation) {
    if (rotation > 3) rotation = 3;

    _locked = true;
    _currentRotation = rotation;
    _pendingRotation = rotation;

    Serial.printf("[OrientationMgr] Rotation locked to %d\n", rotation);

    // Invoke callback for the locked rotation
    if (_callback) {
        _callback(_currentRotation);
    }
}

void OrientationManager::unlockRotation() {
    _locked = false;
    _pendingStartTime = millis();

    Serial.println("[OrientationMgr] Rotation unlocked");
}

// ============================================================================
// Configuration
// ============================================================================

void OrientationManager::setThresholdDegrees(float degrees) {
    // Clamp to reasonable range
    if (degrees < 5.0f) degrees = 5.0f;
    if (degrees > 80.0f) degrees = 80.0f;

    _thresholdDegrees = degrees;
    updateThresholdTan();
}

void OrientationManager::updateThresholdTan() {
    // Convert degrees to radians and compute tangent
    // tan(angle) gives us the ratio of acceleration components
    // at the tilt threshold
    float radians = _thresholdDegrees * M_PI / 180.0f;
    _thresholdTan = tanf(radians);
}

// ============================================================================
// Debug Accessors
// ============================================================================

void OrientationManager::getLastAccel(float* ax, float* ay, float* az) const {
    if (ax) *ax = _lastAx;
    if (ay) *ay = _lastAy;
    if (az) *az = _lastAz;
}

uint32_t OrientationManager::getTimeUntilChange() const {
    if (_pendingRotation == _currentRotation) {
        return 0;  // No change pending
    }

    uint32_t elapsed = millis() - _pendingStartTime;
    if (elapsed >= _debounceMs) {
        return 0;  // Change imminent
    }

    return _debounceMs - elapsed;
}
