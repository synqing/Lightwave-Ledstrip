// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file BuiltinEffectRegistry.cpp
 * @brief Static registry implementation for built-in effects
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "BuiltinEffectRegistry.h"
#include <cstring>

namespace lightwaveos {
namespace plugins {

// Static storage
IEffect* BuiltinEffectRegistry::s_effects[MAX_EFFECTS] = {nullptr};
uint8_t BuiltinEffectRegistry::s_count = 0;

bool BuiltinEffectRegistry::registerBuiltin(uint8_t id, IEffect* effect) {
    if (id >= MAX_EFFECTS || effect == nullptr) {
        return false;
    }

    if (s_effects[id] == nullptr) {
        s_count++;
    }
    s_effects[id] = effect;
    return true;
}

IEffect* BuiltinEffectRegistry::getBuiltin(uint8_t id) {
    if (id >= MAX_EFFECTS) {
        return nullptr;
    }
    return s_effects[id];
}

bool BuiltinEffectRegistry::hasBuiltin(uint8_t id) {
    if (id >= MAX_EFFECTS) {
        return false;
    }
    return s_effects[id] != nullptr;
}

uint8_t BuiltinEffectRegistry::getBuiltinCount() {
    return s_count;
}

void BuiltinEffectRegistry::clear() {
    memset(s_effects, 0, sizeof(s_effects));
    s_count = 0;
}

} // namespace plugins
} // namespace lightwaveos
