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

// ==================== Tier System ====================

/**
 * @brief Three duration tiers for predictable leadTime semantics.
 *
 * Phase 2.2: replaces per-transition magic-number durations with a
 * canonical tier classification. The user can predict how early to arm
 * and how late to fire because every transition in a tier finishes in
 * the same time. Tier durations are calibrated for K1 LGP physics.
 */
enum class TransitionTier : uint8_t {
    QUICK = 0,      // Snappy cuts and dissolves
    MEDIUM = 1,     // Energy shifts
    CINEMATIC = 2,  // Set-piece moments
};

inline constexpr uint16_t kQuickDurationMs     = 500;
inline constexpr uint16_t kMediumDurationMs    = 1500;
inline constexpr uint16_t kCinematicDurationMs = 2500;

inline TransitionTier getTransitionTier(TransitionType type) {
    switch (type) {
        case TransitionType::FADE:
        case TransitionType::WIPE_OUT:
        case TransitionType::WIPE_IN:
        case TransitionType::DISSOLVE:
            return TransitionTier::QUICK;
        case TransitionType::PHASE_SHIFT:
        case TransitionType::PULSEWAVE:
        case TransitionType::IMPLOSION:
        case TransitionType::IRIS:
            return TransitionTier::MEDIUM;
        case TransitionType::NUCLEAR:
        case TransitionType::STARGATE:
        case TransitionType::KALEIDOSCOPE:
        case TransitionType::MANDALA:
            return TransitionTier::CINEMATIC;
        default:
            return TransitionTier::MEDIUM;
    }
}

inline uint16_t getTierDuration(TransitionTier tier) {
    switch (tier) {
        case TransitionTier::QUICK:     return kQuickDurationMs;
        case TransitionTier::MEDIUM:    return kMediumDurationMs;
        case TransitionTier::CINEMATIC: return kCinematicDurationMs;
    }
    return kMediumDurationMs;
}

inline const char* getTierName(TransitionTier tier) {
    switch (tier) {
        case TransitionTier::QUICK:     return "Quick";
        case TransitionTier::MEDIUM:    return "Medium";
        case TransitionTier::CINEMATIC: return "Cinematic";
    }
    return "Unknown";
}

/**
 * @brief Fill outTypes (sized 4) with the transitions belonging to a tier.
 *
 * Caller owns the storage; pass a `TransitionType[4]` buffer. The order
 * matches the enum declaration so CLI cycling remains stable across
 * tier boundaries.
 */
inline void getTransitionsInTier(TransitionTier tier, TransitionType outTypes[4]) {
    switch (tier) {
        case TransitionTier::QUICK:
            outTypes[0] = TransitionType::FADE;
            outTypes[1] = TransitionType::WIPE_OUT;
            outTypes[2] = TransitionType::WIPE_IN;
            outTypes[3] = TransitionType::DISSOLVE;
            return;
        case TransitionTier::MEDIUM:
            outTypes[0] = TransitionType::PHASE_SHIFT;
            outTypes[1] = TransitionType::PULSEWAVE;
            outTypes[2] = TransitionType::IMPLOSION;
            outTypes[3] = TransitionType::IRIS;
            return;
        case TransitionTier::CINEMATIC:
            outTypes[0] = TransitionType::NUCLEAR;
            outTypes[1] = TransitionType::STARGATE;
            outTypes[2] = TransitionType::KALEIDOSCOPE;
            outTypes[3] = TransitionType::MANDALA;
            return;
    }
}

// ==================== Default Durations ====================

/**
 * @brief Recommended duration for a transition type, derived from its tier.
 *
 * Phase 2.2: delegates to the tier system so every transition in a tier
 * shares a canonical duration. Was per-type magic numbers (800/1200/...).
 */
inline uint16_t getDefaultDuration(TransitionType type) {
    return getTierDuration(getTransitionTier(type));
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
