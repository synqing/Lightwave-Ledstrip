/**
 * @file BuiltinEffectRegistry.cpp
 * @brief Static registry implementation for built-in effects
 *
 * Append-only linear registry with linear scan lookup.
 * For 162 effects on ESP32 at 240 MHz, scan time is <1us.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "BuiltinEffectRegistry.h"
#include <cstring>

namespace lightwaveos {
namespace plugins {

// Static storage
BuiltinEffectRegistry::Entry BuiltinEffectRegistry::s_entries[MAX_EFFECTS] = {};
uint16_t BuiltinEffectRegistry::s_count = 0;

bool BuiltinEffectRegistry::registerBuiltin(EffectId id, IEffect* effect) {
    if (id == INVALID_EFFECT_ID || effect == nullptr) {
        return false;
    }

    // Check for existing registration (update in place)
    for (uint16_t i = 0; i < s_count; ++i) {
        if (s_entries[i].id == id) {
            s_entries[i].effect = effect;
            return true;
        }
    }

    // Append new entry
    if (s_count >= MAX_EFFECTS) {
        return false;
    }
    s_entries[s_count].id = id;
    s_entries[s_count].effect = effect;
    s_count++;
    return true;
}

IEffect* BuiltinEffectRegistry::getBuiltin(EffectId id) {
    for (uint16_t i = 0; i < s_count; ++i) {
        if (s_entries[i].id == id) {
            return s_entries[i].effect;
        }
    }
    return nullptr;
}

bool BuiltinEffectRegistry::hasBuiltin(EffectId id) {
    for (uint16_t i = 0; i < s_count; ++i) {
        if (s_entries[i].id == id) {
            return true;
        }
    }
    return false;
}

uint16_t BuiltinEffectRegistry::getBuiltinCount() {
    return s_count;
}

void BuiltinEffectRegistry::clear() {
    memset(s_entries, 0, sizeof(s_entries));
    s_count = 0;
}

} // namespace plugins
} // namespace lightwaveos
