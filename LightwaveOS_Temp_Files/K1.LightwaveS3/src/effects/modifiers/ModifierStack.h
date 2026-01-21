/**
 * @file ModifierStack.h
 * @brief Modifier orchestration and stack management
 *
 * The ModifierStack manages a collection of IEffectModifiers that are
 * applied in order to the LED buffer after effect rendering.
 *
 * Stack Behavior:
 * - Modifiers are applied in FIFO order (first added = first applied)
 * - Maximum 8 modifiers in stack (memory constraint)
 * - Each modifier receives output of previous modifier
 * - Modifiers are unapplied in LIFO order (last added = first removed)
 *
 * Thread Safety:
 * - applyAll() is called from Core 1 render task
 * - add()/remove() called from Core 0 (Web/Serial commands)
 * - Mutex protection for stack modifications
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "IEffectModifier.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace lightwaveos {
namespace effects {
namespace modifiers {

/**
 * @brief Modifier stack manager
 *
 * Manages a stack of modifiers applied to the LED buffer.
 * Thread-safe for concurrent add/remove from Core 0 and apply from Core 1.
 */
class ModifierStack {
public:
    /**
     * @brief Maximum number of modifiers in stack
     */
    static constexpr uint8_t MAX_MODIFIERS = 8;

    /**
     * @brief Constructor
     */
    ModifierStack();

    /**
     * @brief Destructor
     */
    ~ModifierStack();

    //--------------------------------------------------------------------------
    // Stack Management
    //--------------------------------------------------------------------------

    /**
     * @brief Add a modifier to the stack
     * @param modifier Modifier instance (ownership NOT transferred)
     * @param ctx Effect context for init()
     * @return true if added successfully, false if stack full or init failed
     *
     * Thread-safe. Can be called from Core 0 (Web/Serial).
     * Modifier is initialized before being added to active stack.
     */
    bool add(IEffectModifier* modifier, const plugins::EffectContext& ctx);

    /**
     * @brief Remove a modifier from the stack
     * @param modifier Modifier instance to remove
     * @return true if removed, false if not found
     *
     * Thread-safe. Can be called from Core 0 (Web/Serial).
     * Modifier's unapply() is called before removal.
     */
    bool remove(IEffectModifier* modifier);

    /**
     * @brief Remove modifier by type
     * @param type Modifier type to remove
     * @return true if removed, false if not found
     */
    bool removeByType(ModifierType type);

    /**
     * @brief Clear all modifiers from stack
     *
     * Thread-safe. Calls unapply() on all modifiers in LIFO order.
     */
    void clear();

    /**
     * @brief Get number of active modifiers
     */
    uint8_t getCount() const { return m_count; }

    /**
     * @brief Check if stack is empty
     */
    bool isEmpty() const { return m_count == 0; }

    /**
     * @brief Check if stack is full
     */
    bool isFull() const { return m_count >= MAX_MODIFIERS; }

    /**
     * @brief Get modifier at index
     * @param index Index in stack (0 to getCount()-1)
     * @return Modifier pointer or nullptr if invalid index
     */
    IEffectModifier* getModifier(uint8_t index) const;

    /**
     * @brief Find modifier by type
     * @param type Modifier type to find
     * @return Modifier pointer or nullptr if not found
     */
    IEffectModifier* findByType(ModifierType type) const;

    //--------------------------------------------------------------------------
    // Rendering
    //--------------------------------------------------------------------------

    /**
     * @brief Apply all modifiers in stack order
     * @param ctx Effect context with LED buffer
     *
     * Called from Core 1 render task at 120 FPS.
     * Applies each modifier in FIFO order.
     *
     * Thread-safe: uses read lock to prevent concurrent modifications.
     */
    void applyAll(plugins::EffectContext& ctx);

    //--------------------------------------------------------------------------
    // Diagnostics
    //--------------------------------------------------------------------------

    /**
     * @brief Get total memory usage of stack
     * @return Estimated bytes used by modifiers
     */
    size_t getMemoryUsage() const;

    /**
     * @brief Print stack state to serial (debug)
     */
    void printState() const;

private:
    // Modifier storage (pointers, ownership NOT held)
    IEffectModifier* m_modifiers[MAX_MODIFIERS];
    uint8_t m_count;

    // Thread synchronization
    SemaphoreHandle_t m_mutex;

    // Helper: acquire mutex with timeout
    bool lock(uint32_t timeoutMs = 100);
    void unlock();
};

} // namespace modifiers
} // namespace effects
} // namespace lightwaveos
