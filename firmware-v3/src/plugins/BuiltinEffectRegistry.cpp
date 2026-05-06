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
#include <cstdlib>
#include <cstring>
#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

namespace lightwaveos {
namespace plugins {

// Static registry storage is allocated once during effect registration so the
// table can live in PSRAM on K1v2 instead of permanent internal DRAM.
BuiltinEffectRegistry::Entry* BuiltinEffectRegistry::s_entries = nullptr;
uint16_t BuiltinEffectRegistry::s_count = 0;

bool BuiltinEffectRegistry::ensureStorage() {
    if (s_entries) {
        return true;
    }

#if defined(NATIVE_BUILD)
    s_entries = static_cast<Entry*>(std::calloc(MAX_EFFECTS, sizeof(Entry)));
#elif defined(BOARD_HAS_PSRAM)
    s_entries = static_cast<Entry*>(
        heap_caps_calloc(MAX_EFFECTS, sizeof(Entry), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#else
    s_entries = static_cast<Entry*>(
        heap_caps_calloc(MAX_EFFECTS, sizeof(Entry), MALLOC_CAP_8BIT));
#endif
    return s_entries != nullptr;
}

bool BuiltinEffectRegistry::registerBuiltin(EffectId id, IEffect* effect) {
    if (id == INVALID_EFFECT_ID || effect == nullptr) {
        return false;
    }
    if (!ensureStorage()) {
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
    if (!s_entries) {
        return nullptr;
    }
    for (uint16_t i = 0; i < s_count; ++i) {
        if (s_entries[i].id == id) {
            return s_entries[i].effect;
        }
    }
    return nullptr;
}

bool BuiltinEffectRegistry::hasBuiltin(EffectId id) {
    if (!s_entries) {
        return false;
    }
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
    if (s_entries) {
        memset(s_entries, 0, MAX_EFFECTS * sizeof(Entry));
    }
    s_count = 0;
}

} // namespace plugins
} // namespace lightwaveos
