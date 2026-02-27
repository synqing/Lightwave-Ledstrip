#pragma once

#ifdef NATIVE_BUILD

#include <stdint.h>

namespace lightwaveos {
namespace transitions {

enum class TransitionType : uint8_t {
    FADE = 0,
    WIPE_OUT = 1,
    WIPE_IN = 2,
    DISSOLVE = 3,
    PHASE_SHIFT = 4,
    PULSEWAVE = 5,
    IMPLOSION = 6,
    IRIS = 7,
    NUCLEAR = 8,
    STARGATE = 9,
    KALEIDOSCOPE = 10,
    MANDALA = 11,
    TYPE_COUNT = 12
};

inline const char* getTransitionName(TransitionType type) {
    switch (type) {
        case TransitionType::FADE:        return "Fade";
        case TransitionType::WIPE_OUT:     return "Wipe Out";
        case TransitionType::WIPE_IN:      return "Wipe In";
        case TransitionType::DISSOLVE:    return "Dissolve";
        case TransitionType::PHASE_SHIFT:  return "Phase Shift";
        case TransitionType::PULSEWAVE:   return "Pulsewave";
        case TransitionType::IMPLOSION:   return "Implosion";
        case TransitionType::IRIS:         return "Iris";
        case TransitionType::NUCLEAR:      return "Nuclear";
        case TransitionType::STARGATE:    return "Stargate";
        case TransitionType::KALEIDOSCOPE: return "Kaleidoscope";
        case TransitionType::MANDALA:     return "Mandala";
        default:                          return "Unknown";
    }
}

} // namespace transitions
} // namespace lightwaveos

#endif // NATIVE_BUILD
