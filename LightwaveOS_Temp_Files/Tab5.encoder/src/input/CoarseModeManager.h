#pragma once
// ============================================================================
// CoarseModeManager - Per-Parameter Coarse Mode with Acceleration
// ============================================================================
// Manages coarse mode for ENC-A (Unit A, encoders 0-7) that activates when
// the physical switch on Unit A moves from position 0 to 1.
//
// Features:
// - Per-parameter base multipliers based on parameter range
// - Exponential acceleration (doubles every 3 consecutive detents)
// - Resets on direction change or pause (>500ms)
// - Maximum multiplier cap (50x) to prevent overflow
// ============================================================================

#include <Arduino.h>
#include <cstdint>

class CoarseModeManager {
public:
    /**
     * Constructor
     */
    CoarseModeManager();

    /**
     * Check if coarse mode is currently enabled
     * @return true if switch is in position 1, false if position 0
     */
    bool isCoarseModeEnabled() const { return _switchState == 1; }

    /**
     * Update switch state (called from main loop)
     * @param switchState Switch position (0 or 1)
     */
    void updateSwitchState(uint8_t switchState);

    /**
     * Apply coarse mode multiplier to normalized delta
     * @param encoderIndex Encoder index (0-7 for ENC-A only)
     * @param normalizedDelta Normalized delta from DetentDebounce (-1, 0, or +1)
     * @param now Current time in milliseconds
     * @return Multiplied delta value
     */
    int32_t applyCoarseMode(uint8_t encoderIndex, int32_t normalizedDelta, uint32_t now);

    /**
     * Reset acceleration state for a specific encoder
     * @param encoderIndex Encoder index (0-7)
     */
    void resetAcceleration(uint8_t encoderIndex);

    /**
     * Reset acceleration state for all encoders
     */
    void resetAllAcceleration();

private:
    /**
     * Acceleration state per encoder
     */
    struct AccelerationState {
        int8_t lastDirection;        // Last delta sign (-1, 0, or +1)
        uint8_t consecutiveDetents;  // Consecutive detents in same direction
        uint32_t lastActivityTime;   // Last activity timestamp
        uint16_t baseMultiplier;     // Base multiplier for this parameter
    };

    /**
     * Calculate base multiplier based on parameter range
     * @param encoderIndex Encoder index (0-7)
     * @return Base multiplier (5-12x based on range)
     */
    uint16_t calculateBaseMultiplier(uint8_t encoderIndex) const;

    /**
     * Calculate current multiplier with acceleration
     * @param state Acceleration state for encoder
     * @return Current multiplier (base × acceleration factor, capped at MAX_MULTIPLIER)
     */
    uint16_t calculateCurrentMultiplier(const AccelerationState& state) const;

    // Configuration constants
    static constexpr uint32_t PAUSE_THRESHOLD_MS = 500;  // Reset acceleration after 500ms pause
    static constexpr uint8_t ACCELERATION_DETENTS = 3;   // Double multiplier every 3 detents
    static constexpr uint16_t MAX_MULTIPLIER = 50;        // Maximum multiplier cap

    // Base multipliers based on parameter range
    static constexpr uint16_t BASE_MULTIPLIER_SMALL = 5;   // For ranges 75-88
    static constexpr uint16_t BASE_MULTIPLIER_MEDIUM = 7;  // For range ~100
    static constexpr uint16_t BASE_MULTIPLIER_LARGE = 12;  // For range 255

    uint8_t _switchState;  // Current switch state (0 or 1)
    AccelerationState _accelerationState[8];  // One per ENC-A encoder (0-7)
};

