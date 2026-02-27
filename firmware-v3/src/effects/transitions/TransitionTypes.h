/**
 * @file TransitionTypes.h
 * @brief 12 CENTER ORIGIN-compliant transition types
 *
 * LightwaveOS v2 - Transition System
 *
 * All transitions radiate from LED 79/80 (center point) to respect
 * the Light Guide Plate physics model.
 */

#pragma once

#include <Arduino.h>

namespace lightwaveos {
namespace transitions {

// ==================== Transition Types ====================

/**
 * @brief 12 CENTER ORIGIN-compliant transition effects
 *
 * Each transition uses distance-from-center to modulate progress,
 * creating outward-radiating or inward-collapsing animations.
 */
enum class TransitionType : uint8_t {
    FADE = 0,           // Crossfade radiates from center outward
    WIPE_OUT = 1,       // Circular wipe expanding center → edges
    WIPE_IN = 2,        // Circular wipe collapsing edges → center
    DISSOLVE = 3,       // Random pixel transition (shuffled order)
    PHASE_SHIFT = 4,    // Frequency-based wave morphing
    PULSEWAVE = 5,      // Concentric energy rings from center
    IMPLOSION = 6,      // Particles converge to center
    IRIS = 7,           // Aperture open/close from center
    NUCLEAR = 8,        // Chain reaction explosion from center
    STARGATE = 9,       // Wormhole portal at center
    KALEIDOSCOPE = 10,  // Symmetric patterns radiating
    MANDALA = 11,       // Sacred geometry concentric rings
    TYPE_COUNT = 12
};

// ==================== Transition Names ====================

inline const char* getTransitionName(TransitionType type) {
    switch (type) {
        case TransitionType::FADE:        return "Fade";
        case TransitionType::WIPE_OUT:    return "Wipe Out";
        case TransitionType::WIPE_IN:     return "Wipe In";
        case TransitionType::DISSOLVE:    return "Dissolve";
        case TransitionType::PHASE_SHIFT: return "Phase Shift";
        case TransitionType::PULSEWAVE:   return "Pulsewave";
        case TransitionType::IMPLOSION:   return "Implosion";
        case TransitionType::IRIS:        return "Iris";
        case TransitionType::NUCLEAR:     return "Nuclear";
        case TransitionType::STARGATE:    return "Stargate";
        case TransitionType::KALEIDOSCOPE: return "Kaleidoscope";
        case TransitionType::MANDALA:     return "Mandala";
        default: return "Unknown";
    }
}

// ==================== Default Durations ====================

/**
 * @brief Get recommended duration for transition type (ms)
 */
inline uint16_t getDefaultDuration(TransitionType type) {
    switch (type) {
        case TransitionType::FADE:        return 800;
        case TransitionType::WIPE_OUT:    return 1200;
        case TransitionType::WIPE_IN:     return 1200;
        case TransitionType::DISSOLVE:    return 1500;
        case TransitionType::PHASE_SHIFT: return 1400;
        case TransitionType::PULSEWAVE:   return 2000;
        case TransitionType::IMPLOSION:   return 1500;
        case TransitionType::IRIS:        return 1200;
        case TransitionType::NUCLEAR:     return 2500;
        case TransitionType::STARGATE:    return 3000;
        case TransitionType::KALEIDOSCOPE: return 1800;
        case TransitionType::MANDALA:     return 2200;
        default: return 1000;
    }
}

// ==================== Default Easing Curves ====================

// Forward declare EasingCurve (defined in Easing.h)
enum class EasingCurve : uint8_t;

/**
 * @brief Get recommended easing curve for transition type
 */
inline uint8_t getDefaultEasing(TransitionType type) {
    // Returns the uint8_t value to avoid circular include
    // Maps to EasingCurve enum
    switch (type) {
        case TransitionType::FADE:        return 3;  // IN_OUT_QUAD
        case TransitionType::WIPE_OUT:    return 5;  // OUT_CUBIC
        case TransitionType::WIPE_IN:     return 4;  // IN_CUBIC
        case TransitionType::DISSOLVE:    return 0;  // LINEAR
        case TransitionType::PHASE_SHIFT: return 6;  // IN_OUT_CUBIC
        case TransitionType::PULSEWAVE:   return 2;  // OUT_QUAD
        case TransitionType::IMPLOSION:   return 4;  // IN_CUBIC
        case TransitionType::IRIS:        return 3;  // IN_OUT_QUAD
        case TransitionType::NUCLEAR:     return 8;  // OUT_ELASTIC
        case TransitionType::STARGATE:    return 14; // IN_OUT_BACK
        case TransitionType::KALEIDOSCOPE: return 6; // IN_OUT_CUBIC
        case TransitionType::MANDALA:     return 9;  // IN_OUT_ELASTIC
        default: return 0;
    }
}

} // namespace transitions
} // namespace lightwaveos
