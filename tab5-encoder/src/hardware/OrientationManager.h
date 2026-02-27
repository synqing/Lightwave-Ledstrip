#pragma once
// ============================================================================
// OrientationManager - Automatic Screen Rotation via IMU
// ============================================================================
// Uses the Tab5's BMI270 IMU accelerometer to detect device orientation
// and automatically rotate the display.
//
// Features:
// - Hysteresis to prevent jitter near rotation boundaries
// - Debounce to ensure stable orientation before switching
// - Dead zone when device is flat (Z-axis dominant)
// - Callback system for display updates
//
// Rotation Mapping (based on USB port position):
//   USB Right  -> Rotation 1 (Landscape, normal)
//   USB Down   -> Rotation 0 (Portrait)
//   USB Left   -> Rotation 3 (Landscape, flipped)
//   USB Up     -> Rotation 2 (Portrait, flipped)
//
// Usage:
//   OrientationManager orientMgr;
//   orientMgr.begin();
//   orientMgr.setCallback([](uint8_t rotation) {
//       M5.Display.setRotation(rotation);
//   });
//   // In loop (call at ~10Hz):
//   orientMgr.update();
//
// ============================================================================

#include <Arduino.h>
#include <functional>

class OrientationManager {
public:
    // Callback signature: (new_rotation 0-3)
    using RotationCallback = std::function<void(uint8_t)>;

    /**
     * Constructor - sets default values
     */
    OrientationManager();

    /**
     * Initialize the orientation manager
     * Note: M5.Imu should already be initialized by M5.begin()
     */
    void begin();

    /**
     * Poll IMU and update orientation
     * Call this at approximately 10Hz (every 100ms) in the main loop
     */
    void update();

    // ========================================================================
    // Rotation Access
    // ========================================================================

    /**
     * Get the current display rotation
     * @return Rotation value 0-3
     */
    uint8_t getRotation() const { return _currentRotation; }

    /**
     * Register callback for rotation changes
     * @param callback Function to call when rotation changes
     */
    void setCallback(RotationCallback callback);

    // ========================================================================
    // Enable/Disable
    // ========================================================================

    /**
     * Enable or disable auto-rotation
     * Useful during calibration or when user locks orientation
     * @param enabled true to enable, false to disable
     */
    void setEnabled(bool enabled) { _enabled = enabled; }

    /**
     * Check if auto-rotation is enabled
     * @return true if enabled
     */
    bool isEnabled() const { return _enabled; }

    /**
     * Force a specific rotation (disables auto-rotation)
     * @param rotation Rotation value 0-3
     */
    void lockRotation(uint8_t rotation);

    /**
     * Unlock rotation and resume auto-rotation
     */
    void unlockRotation();

    /**
     * Check if rotation is locked
     * @return true if locked to a specific rotation
     */
    bool isLocked() const { return _locked; }

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * Set the tilt threshold in degrees
     * Device must tilt beyond this angle to trigger rotation
     * @param degrees Threshold angle (default: 30)
     */
    void setThresholdDegrees(float degrees);

    /**
     * Get the current threshold in degrees
     * @return Threshold angle
     */
    float getThresholdDegrees() const { return _thresholdDegrees; }

    /**
     * Set the debounce time in milliseconds
     * Orientation must be stable for this duration before switching
     * @param ms Debounce time (default: 500)
     */
    void setDebounceMs(uint32_t ms) { _debounceMs = ms; }

    /**
     * Get the current debounce time
     * @return Debounce time in milliseconds
     */
    uint32_t getDebounceMs() const { return _debounceMs; }

    /**
     * Set the flat detection threshold
     * When abs(Z) exceeds this, device is considered flat (no rotation change)
     * @param threshold Z-axis threshold (default: 0.7)
     */
    void setFlatThreshold(float threshold) { _flatThreshold = threshold; }

    /**
     * Get the flat detection threshold
     * @return Z-axis threshold
     */
    float getFlatThreshold() const { return _flatThreshold; }

    // ========================================================================
    // Debug
    // ========================================================================

    /**
     * Get the last read accelerometer values (for debugging)
     * @param ax Pointer to store X acceleration
     * @param ay Pointer to store Y acceleration
     * @param az Pointer to store Z acceleration
     */
    void getLastAccel(float* ax, float* ay, float* az) const;

    /**
     * Get the pending rotation (for debugging hysteresis)
     * @return Pending rotation value, or current if none pending
     */
    uint8_t getPendingRotation() const { return _pendingRotation; }

    /**
     * Get time remaining until rotation change (for debugging)
     * @return Milliseconds until rotation changes, 0 if stable
     */
    uint32_t getTimeUntilChange() const;

private:
    // State
    bool _enabled;
    bool _locked;
    uint8_t _currentRotation;
    uint8_t _pendingRotation;
    uint32_t _pendingStartTime;

    // Configuration
    float _thresholdDegrees;
    float _thresholdTan;          // Cached tan(_thresholdDegrees)
    uint32_t _debounceMs;
    float _flatThreshold;

    // Last accelerometer readings (for debugging)
    float _lastAx;
    float _lastAy;
    float _lastAz;

    // Callback
    RotationCallback _callback;

    /**
     * Detect orientation from accelerometer readings
     * @param ax X acceleration (g)
     * @param ay Y acceleration (g)
     * @param az Z acceleration (g)
     * @return Detected rotation 0-3, or current rotation if unclear
     */
    uint8_t detectOrientation(float ax, float ay, float az);

    /**
     * Check if the new rotation has been stable long enough
     * @param newRotation Detected rotation
     * @param now Current timestamp (millis)
     * @return true if rotation should change
     */
    bool isStable(uint8_t newRotation, uint32_t now);

    /**
     * Update the cached tangent value when threshold changes
     */
    void updateThresholdTan();
};
