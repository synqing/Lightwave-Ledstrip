// ============================================================================
// CoarseModeManager - Per-Parameter Coarse Mode with Acceleration
// ============================================================================

#include "CoarseModeManager.h"
#include "../parameters/ParameterMap.h"
#include <cmath>

CoarseModeManager::CoarseModeManager()
    : _switchState(0)
{
    // Initialize acceleration state for all encoders
    for (uint8_t i = 0; i < 8; i++) {
        _accelerationState[i].lastDirection = 0;
        _accelerationState[i].consecutiveDetents = 0;
        _accelerationState[i].lastActivityTime = 0;
        _accelerationState[i].baseMultiplier = calculateBaseMultiplier(i);
    }
}

void CoarseModeManager::updateSwitchState(uint8_t switchState) {
    if (switchState > 1) {
        switchState = 1;  // Clamp to valid range
    }
    
    // If switching from enabled to disabled, reset all acceleration
    if (_switchState == 1 && switchState == 0) {
        resetAllAcceleration();
    }
    
    _switchState = switchState;
}

int32_t CoarseModeManager::applyCoarseMode(uint8_t encoderIndex, int32_t normalizedDelta, uint32_t now) {
    // Only apply to ENC-A (indices 0-7)
    if (encoderIndex >= 8) {
        return normalizedDelta;
    }

    // If coarse mode is disabled, return delta unchanged
    if (!isCoarseModeEnabled()) {
        // Reset acceleration when disabled
        resetAcceleration(encoderIndex);
        return normalizedDelta;
    }

    AccelerationState& state = _accelerationState[encoderIndex];
    int8_t currentDirection = (normalizedDelta > 0) ? 1 : ((normalizedDelta < 0) ? -1 : 0);

    // Check for direction change
    if (state.lastDirection != 0 && currentDirection != 0 && 
        state.lastDirection != currentDirection) {
        // Direction changed - reset acceleration
        state.consecutiveDetents = 0;
        state.lastDirection = currentDirection;
    } else if (currentDirection != 0) {
        // Same direction or first movement
        state.lastDirection = currentDirection;
    }

    // Check for pause (no activity for >500ms)
    if (state.lastActivityTime > 0 && (now - state.lastActivityTime) > PAUSE_THRESHOLD_MS) {
        // Pause detected - reset acceleration
        state.consecutiveDetents = 0;
    }

    // Update activity timestamp
    if (normalizedDelta != 0) {
        state.lastActivityTime = now;
    }

    // Increment consecutive detents if moving in same direction (before calculating multiplier)
    if (normalizedDelta != 0) {
        if (currentDirection == state.lastDirection && state.lastDirection != 0) {
            // Same direction - increment
            state.consecutiveDetents++;
        } else {
            // First detent in this direction or direction changed
            state.consecutiveDetents = 1;
        }
    }

    // Calculate multiplier (after updating consecutive detents)
    uint16_t multiplier = calculateCurrentMultiplier(state);

    // Apply multiplier to delta
    int32_t result = normalizedDelta * static_cast<int32_t>(multiplier);

    return result;
}

void CoarseModeManager::resetAcceleration(uint8_t encoderIndex) {
    if (encoderIndex >= 8) return;

    AccelerationState& state = _accelerationState[encoderIndex];
    state.lastDirection = 0;
    state.consecutiveDetents = 0;
    state.lastActivityTime = 0;
    // Keep baseMultiplier as it's based on parameter range
}

void CoarseModeManager::resetAllAcceleration() {
    for (uint8_t i = 0; i < 8; i++) {
        resetAcceleration(i);
    }
}

uint16_t CoarseModeManager::calculateBaseMultiplier(uint8_t encoderIndex) const {
    if (encoderIndex >= 8) {
        return BASE_MULTIPLIER_SMALL;  // Fallback
    }

    // Get parameter range
    uint8_t min = getParameterMin(encoderIndex);
    uint8_t max = getParameterMax(encoderIndex);
    uint16_t range = (max - min) + 1;

    // Calculate base multiplier based on range
    if (range <= 88) {
        // Small range (Effect: 88, Palette: 75)
        return BASE_MULTIPLIER_SMALL;  // 5x
    } else if (range <= 100) {
        // Medium range (Speed: 100)
        return BASE_MULTIPLIER_MEDIUM;  // 7x
    } else {
        // Large range (Mood, Fade, Complexity, Variation, Brightness: 256)
        return BASE_MULTIPLIER_LARGE;  // 12x
    }
}

uint16_t CoarseModeManager::calculateCurrentMultiplier(const AccelerationState& state) const {
    uint16_t base = state.baseMultiplier;

    // Calculate acceleration factor: 2^(floor(consecutive_detents / 3))
    // This doubles the multiplier every 3 consecutive detents
    uint8_t accelerationLevel = state.consecutiveDetents / ACCELERATION_DETENTS;
    
    // Calculate 2^accelerationLevel (exponential acceleration)
    // Limit to prevent overflow: 2^4 = 16, so max would be 12 * 16 = 192x
    // We cap at MAX_MULTIPLIER (50x) instead
    uint32_t accelerationFactor = 1;
    for (uint8_t i = 0; i < accelerationLevel && i < 4; i++) {  // Cap at 2^4 = 16x acceleration
        accelerationFactor *= 2;
    }

    uint32_t calculatedMultiplier = base * accelerationFactor;

    // Cap at maximum multiplier
    if (calculatedMultiplier > MAX_MULTIPLIER) {
        return MAX_MULTIPLIER;
    }

    return static_cast<uint16_t>(calculatedMultiplier);
}

