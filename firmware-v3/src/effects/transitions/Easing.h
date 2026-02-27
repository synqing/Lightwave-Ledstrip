/**
 * @file Easing.h
 * @brief 15 easing curves for smooth animation transitions
 *
 * LightwaveOS v2 - Transition System
 *
 * Easing functions transform linear progress (0-1) into curved motion.
 * Used by TransitionEngine for natural-feeling effect transitions.
 */

#pragma once

#include <Arduino.h>
#include <math.h>

namespace lightwaveos {
namespace transitions {

// ==================== Easing Curve Types ====================

enum class EasingCurve : uint8_t {
    LINEAR = 0,
    IN_QUAD = 1,
    OUT_QUAD = 2,
    IN_OUT_QUAD = 3,
    IN_CUBIC = 4,
    OUT_CUBIC = 5,
    IN_OUT_CUBIC = 6,
    IN_ELASTIC = 7,
    OUT_ELASTIC = 8,
    IN_OUT_ELASTIC = 9,
    IN_BOUNCE = 10,
    OUT_BOUNCE = 11,
    IN_BACK = 12,
    OUT_BACK = 13,
    IN_OUT_BACK = 14,
    CURVE_COUNT = 15
};

// ==================== Easing Function Names ====================

inline const char* getEasingName(EasingCurve curve) {
    switch (curve) {
        case EasingCurve::LINEAR:       return "Linear";
        case EasingCurve::IN_QUAD:      return "In Quad";
        case EasingCurve::OUT_QUAD:     return "Out Quad";
        case EasingCurve::IN_OUT_QUAD:  return "InOut Quad";
        case EasingCurve::IN_CUBIC:     return "In Cubic";
        case EasingCurve::OUT_CUBIC:    return "Out Cubic";
        case EasingCurve::IN_OUT_CUBIC: return "InOut Cubic";
        case EasingCurve::IN_ELASTIC:   return "In Elastic";
        case EasingCurve::OUT_ELASTIC:  return "Out Elastic";
        case EasingCurve::IN_OUT_ELASTIC: return "InOut Elastic";
        case EasingCurve::IN_BOUNCE:    return "In Bounce";
        case EasingCurve::OUT_BOUNCE:   return "Out Bounce";
        case EasingCurve::IN_BACK:      return "In Back";
        case EasingCurve::OUT_BACK:     return "Out Back";
        case EasingCurve::IN_OUT_BACK:  return "InOut Back";
        default: return "Unknown";
    }
}

// ==================== Easing Implementation ====================

/**
 * @brief Apply easing curve to linear progress
 * @param t Linear progress (0.0 to 1.0)
 * @param curve Easing curve type
 * @return Eased progress (0.0 to 1.0, may overshoot for elastic/back)
 */
inline float ease(float t, EasingCurve curve) {
    // Clamp input to valid range
    t = constrain(t, 0.0f, 1.0f);

    switch (curve) {
        case EasingCurve::LINEAR:
            return t;

        case EasingCurve::IN_QUAD:
            return t * t;

        case EasingCurve::OUT_QUAD:
            return t * (2.0f - t);

        case EasingCurve::IN_OUT_QUAD:
            return (t < 0.5f) ? (2.0f * t * t) : (-1.0f + (4.0f - 2.0f * t) * t);

        case EasingCurve::IN_CUBIC:
            return t * t * t;

        case EasingCurve::OUT_CUBIC: {
            float f = t - 1.0f;
            return f * f * f + 1.0f;
        }

        case EasingCurve::IN_OUT_CUBIC:
            return (t < 0.5f)
                ? (4.0f * t * t * t)
                : ((t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f);

        case EasingCurve::IN_ELASTIC: {
            if (t == 0.0f || t == 1.0f) return t;
            float p = 0.3f;
            return -powf(2.0f, 10.0f * (t - 1.0f)) * sinf((t - 1.1f) * 2.0f * PI / p);
        }

        case EasingCurve::OUT_ELASTIC: {
            if (t == 0.0f || t == 1.0f) return t;
            float p = 0.3f;
            return powf(2.0f, -10.0f * t) * sinf((t - 0.1f) * 2.0f * PI / p) + 1.0f;
        }

        case EasingCurve::IN_OUT_ELASTIC: {
            if (t == 0.0f || t == 1.0f) return t;
            float p = 0.45f;
            float s = p / 4.0f;
            if (t < 0.5f) {
                return -0.5f * powf(2.0f, 20.0f * t - 10.0f) *
                       sinf((20.0f * t - 11.125f) * 2.0f * PI / p);
            }
            return powf(2.0f, -20.0f * t + 10.0f) *
                   sinf((20.0f * t - 11.125f) * 2.0f * PI / p) * 0.5f + 1.0f;
        }

        case EasingCurve::OUT_BOUNCE: {
            if (t < 1.0f / 2.75f) {
                return 7.5625f * t * t;
            } else if (t < 2.0f / 2.75f) {
                t -= 1.5f / 2.75f;
                return 7.5625f * t * t + 0.75f;
            } else if (t < 2.5f / 2.75f) {
                t -= 2.25f / 2.75f;
                return 7.5625f * t * t + 0.9375f;
            } else {
                t -= 2.625f / 2.75f;
                return 7.5625f * t * t + 0.984375f;
            }
        }

        case EasingCurve::IN_BOUNCE:
            return 1.0f - ease(1.0f - t, EasingCurve::OUT_BOUNCE);

        case EasingCurve::IN_BACK: {
            float s = 1.70158f;
            return t * t * ((s + 1.0f) * t - s);
        }

        case EasingCurve::OUT_BACK: {
            float s = 1.70158f;
            float f = t - 1.0f;
            return f * f * ((s + 1.0f) * f + s) + 1.0f;
        }

        case EasingCurve::IN_OUT_BACK: {
            float s = 1.70158f * 1.525f;
            if (t < 0.5f) {
                return 0.5f * (4.0f * t * t * ((s + 1.0f) * 2.0f * t - s));
            }
            float f = 2.0f * t - 2.0f;
            return 0.5f * (f * f * ((s + 1.0f) * f + s) + 2.0f);
        }

        default:
            return t;
    }
}

} // namespace transitions
} // namespace lightwaveos
