#ifndef EFFECTS_H
#define EFFECTS_H

#include <stdint.h>
#include "core/EffectTypes.h"
#include "effects/PatternRegistry.h"

// Forward declaration
struct PatternMetadata;

// Effect function pointer type
typedef void (*EffectFunction)();

// Effect types
enum EffectType {
    EFFECT_TYPE_STANDARD,
    EFFECT_TYPE_WAVE_ENGINE
};

// Effect structure
struct Effect {
    const char* name;
    EffectFunction function;
    EffectType type;
    const PatternMetadata* metadata = nullptr;  // Optional: pattern taxonomy metadata (nullptr = not yet tagged)
};

// External declarations
extern Effect effects[];
extern const uint8_t NUM_EFFECTS;

#endif // EFFECTS_H