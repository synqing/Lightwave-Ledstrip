#ifndef EFFECTS_H
#define EFFECTS_H

#include <stdint.h>
#include "core/EffectTypes.h"

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
};

// External declarations
extern Effect effects[];
extern const uint8_t NUM_EFFECTS;

#endif // EFFECTS_H