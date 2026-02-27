/**
 * @file EffectValidationMacros.cpp
 * @brief Global validation ring buffer instance definition
 *
 * Only compiled when FEATURE_EFFECT_VALIDATION is enabled.
 * Provides the global ring buffer for effect validation samples.
 */

#include "EffectValidationMacros.h"

#if FEATURE_EFFECT_VALIDATION

#include "EffectValidationMetrics.h"

namespace lightwaveos {
namespace validation {

// Global validation ring buffer pointer (lazy initialized)
// Allocated on first call to initValidationRing() to avoid
// stack overflow during ESP32 static initialization
EffectValidationRing<32>* g_validationRing = nullptr;

void initValidationRing() {
    if (g_validationRing == nullptr) {
        g_validationRing = new EffectValidationRing<32>();
    }
}

} // namespace validation
} // namespace lightwaveos

#endif // FEATURE_EFFECT_VALIDATION
