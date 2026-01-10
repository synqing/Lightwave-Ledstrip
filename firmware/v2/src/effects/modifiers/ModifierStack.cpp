/**
 * @file ModifierStack.cpp
 * @brief Modifier stack implementation
 */

#include "ModifierStack.h"
#include <Arduino.h>

namespace lightwaveos {
namespace effects {
namespace modifiers {

ModifierStack::ModifierStack()
    : m_count(0)
    , m_mutex(nullptr) {

    // Initialize modifier array
    for (uint8_t i = 0; i < MAX_MODIFIERS; i++) {
        m_modifiers[i] = nullptr;
    }

    // Create mutex for thread safety
    m_mutex = xSemaphoreCreateMutex();
    if (!m_mutex) {
        Serial.println("[ModifierStack] ERROR: Failed to create mutex");
    }
}

ModifierStack::~ModifierStack() {
    // Clear all modifiers (calls unapply())
    clear();

    // Delete mutex
    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
}

bool ModifierStack::lock(uint32_t timeoutMs) {
    if (!m_mutex) return false;
    return xSemaphoreTake(m_mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void ModifierStack::unlock() {
    if (m_mutex) {
        xSemaphoreGive(m_mutex);
    }
}

bool ModifierStack::add(IEffectModifier* modifier, const plugins::EffectContext& ctx) {
    if (!modifier) return false;

    if (!lock()) {
        Serial.println("[ModifierStack] ERROR: Failed to acquire lock for add()");
        return false;
    }

    bool success = false;

    // Check if stack is full
    if (m_count >= MAX_MODIFIERS) {
        Serial.printf("[ModifierStack] ERROR: Stack full (%d/%d)\n", m_count, MAX_MODIFIERS);
        unlock();
        return false;
    }

    // Check for duplicate
    for (uint8_t i = 0; i < m_count; i++) {
        if (m_modifiers[i] == modifier) {
            Serial.printf("[ModifierStack] WARNING: Modifier '%s' already in stack\n",
                          modifier->getName());
            unlock();
            return false;
        }
    }

    // Initialize modifier
    if (!modifier->init(ctx)) {
        Serial.printf("[ModifierStack] ERROR: Modifier '%s' init() failed\n",
                      modifier->getName());
        unlock();
        return false;
    }

    // Add to stack
    m_modifiers[m_count] = modifier;
    m_count++;
    success = true;

    Serial.printf("[ModifierStack] Added '%s' (count: %d/%d)\n",
                  modifier->getName(), m_count, MAX_MODIFIERS);

    unlock();
    return success;
}

bool ModifierStack::remove(IEffectModifier* modifier) {
    if (!modifier) return false;

    if (!lock()) {
        Serial.println("[ModifierStack] ERROR: Failed to acquire lock for remove()");
        return false;
    }

    bool found = false;

    // Find modifier in stack
    for (uint8_t i = 0; i < m_count; i++) {
        if (m_modifiers[i] == modifier) {
            // Call unapply()
            modifier->unapply();

            // Shift remaining modifiers down
            for (uint8_t j = i; j < m_count - 1; j++) {
                m_modifiers[j] = m_modifiers[j + 1];
            }

            m_modifiers[m_count - 1] = nullptr;
            m_count--;
            found = true;

            Serial.printf("[ModifierStack] Removed '%s' (count: %d/%d)\n",
                          modifier->getName(), m_count, MAX_MODIFIERS);
            break;
        }
    }

    if (!found) {
        Serial.printf("[ModifierStack] WARNING: Modifier '%s' not found in stack\n",
                      modifier->getName());
    }

    unlock();
    return found;
}

bool ModifierStack::removeByType(ModifierType type) {
    IEffectModifier* modifier = findByType(type);
    if (modifier) {
        return remove(modifier);
    }
    return false;
}

void ModifierStack::clear() {
    if (!lock()) {
        Serial.println("[ModifierStack] ERROR: Failed to acquire lock for clear()");
        return;
    }

    // Unapply in LIFO order (reverse of application)
    for (int8_t i = m_count - 1; i >= 0; i--) {
        if (m_modifiers[i]) {
            Serial.printf("[ModifierStack] Unapplying '%s'\n", m_modifiers[i]->getName());
            m_modifiers[i]->unapply();
            m_modifiers[i] = nullptr;
        }
    }

    m_count = 0;
    Serial.println("[ModifierStack] Cleared all modifiers");

    unlock();
}

IEffectModifier* ModifierStack::getModifier(uint8_t index) const {
    if (index >= m_count) return nullptr;
    return m_modifiers[index];
}

IEffectModifier* ModifierStack::findByType(ModifierType type) const {
    for (uint8_t i = 0; i < m_count; i++) {
        if (m_modifiers[i] && m_modifiers[i]->getType() == type) {
            return m_modifiers[i];
        }
    }
    return nullptr;
}

void ModifierStack::applyAll(plugins::EffectContext& ctx) {
    // Quick path: no modifiers
    if (m_count == 0) return;

    // Acquire lock with short timeout (render path is time-critical)
    if (!lock(10)) {
        // If we can't get lock quickly, skip modifiers this frame
        // This prevents blocking the render thread
        return;
    }

    // Apply each modifier in FIFO order
    for (uint8_t i = 0; i < m_count; i++) {
        if (m_modifiers[i] && m_modifiers[i]->isEnabled()) {
            m_modifiers[i]->apply(ctx);
        }
    }

    unlock();
}

size_t ModifierStack::getMemoryUsage() const {
    // Base stack overhead
    size_t total = sizeof(ModifierStack);

    // Each modifier pointer
    total += sizeof(IEffectModifier*) * MAX_MODIFIERS;

    // Mutex overhead (approximate)
    total += 64;

    return total;
}

void ModifierStack::printState() const {
    Serial.printf("[ModifierStack] State: %d/%d modifiers\n", m_count, MAX_MODIFIERS);
    for (uint8_t i = 0; i < m_count; i++) {
        if (m_modifiers[i]) {
            Serial.printf("  [%d] %s (%s)\n", i,
                          m_modifiers[i]->getName(),
                          m_modifiers[i]->isEnabled() ? "enabled" : "disabled");
        }
    }
    Serial.printf("[ModifierStack] Memory: %zu bytes\n", getMemoryUsage());
}

} // namespace modifiers
} // namespace effects
} // namespace lightwaveos
