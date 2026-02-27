#include "StateStore.h"
#include <esp_timer.h>
#include <string.h>

namespace lightwaveos {
namespace state {

// ==================== Constructor / Destructor ====================

StateStore::StateStore()
    : m_activeIndex(0)
    , m_writeMutex(nullptr)
    , m_subscriberCount(0)
    , m_commandCount(0)
    , m_lastCommandDuration(0)
{
    // Initialize both state copies with defaults
    m_states[0] = SystemState();
    m_states[1] = SystemState();

    // Create mutex for write protection
    m_writeMutex = xSemaphoreCreateMutex();

    // Initialize subscriber array
    memset(m_subscribers, 0, sizeof(m_subscribers));
}

StateStore::~StateStore() {
    // Clean up mutex
    if (m_writeMutex != nullptr) {
        vSemaphoreDelete(m_writeMutex);
        m_writeMutex = nullptr;
    }
}

// ==================== Query Methods (Lock-Free) ====================

const SystemState& StateStore::getState() const {
    // Lock-free read of active state
    // Validate index to prevent out-of-bounds access if corrupted
    uint8_t safeIndex = validateActiveIndex();
    return m_states[safeIndex];
}

uint32_t StateStore::getVersion() const {
    return getState().version;
}

uint8_t StateStore::getCurrentEffect() const {
    return getState().currentEffectId;
}

uint8_t StateStore::getCurrentPalette() const {
    return getState().currentPaletteId;
}

uint8_t StateStore::getBrightness() const {
    return getState().brightness;
}

uint8_t StateStore::getSpeed() const {
    return getState().speed;
}

bool StateStore::isZoneModeEnabled() const {
    return getState().zoneModeEnabled;
}

uint8_t StateStore::getActiveZoneCount() const {
    return getState().activeZoneCount;
}

ZoneState StateStore::getZoneConfig(uint8_t zoneId) const {
    if (zoneId >= MAX_ZONES) {
        return ZoneState();  // Return default for invalid zone
    }
    return getState().zones[zoneId];
}

bool StateStore::isTransitionActive() const {
    return getState().transitionActive;
}

// ==================== Command Methods (Thread-Safe) ====================

bool StateStore::dispatch(const ICommand& command) {
    // Sanity check
    if (m_writeMutex == nullptr) {
        return false;
    }

    // Start timing
    uint64_t startTime = esp_timer_get_time();

    // Acquire write lock
    if (xSemaphoreTake(m_writeMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;  // Failed to acquire lock within timeout
    }

    bool success = false;

    // Get current active state (validate index to prevent corruption issues)
    uint8_t safeIndex = validateActiveIndex();
    const SystemState& currentState = m_states[safeIndex];

    // Validate command
    if (command.validate(currentState)) {
        // Get inactive state index for writing
        uint8_t writeIndex = getInactiveIndex();

        // Apply command to create new state
        m_states[writeIndex] = command.apply(currentState);

        // Atomically swap active index
        // This makes the new state visible to readers
        swapActiveIndex();

        // Notify subscribers with new state (use validated index)
        uint8_t activeIdx = validateActiveIndex();
        notifySubscribers(m_states[activeIdx]);

        // Update statistics
        m_commandCount++;
        success = true;
    }

    // Release write lock
    xSemaphoreGive(m_writeMutex);

    // Update command duration
    m_lastCommandDuration = static_cast<uint32_t>(esp_timer_get_time() - startTime);

    return success;
}

bool StateStore::dispatchBatch(const ICommand* const* commands, uint8_t count) {
    // Sanity checks
    if (m_writeMutex == nullptr || commands == nullptr || count == 0) {
        return false;
    }

    // Start timing
    uint64_t startTime = esp_timer_get_time();

    // Acquire write lock
    if (xSemaphoreTake(m_writeMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }

    bool success = true;

    // Get current active state (validate index to prevent corruption issues)
    uint8_t safeIndex = validateActiveIndex();
    const SystemState* currentState = &m_states[safeIndex];

    // Validate all commands first
    for (uint8_t i = 0; i < count; i++) {
        if (commands[i] == nullptr || !commands[i]->validate(*currentState)) {
            success = false;
            break;
        }
    }

    // If all valid, apply all commands
    if (success) {
        // Get inactive state index for writing
        uint8_t writeIndex = getInactiveIndex();

        // Start with current state
        m_states[writeIndex] = *currentState;

        // Apply each command sequentially
        for (uint8_t i = 0; i < count; i++) {
            m_states[writeIndex] = commands[i]->apply(m_states[writeIndex]);
        }

        // Atomically swap active index
        swapActiveIndex();

        // Notify subscribers with new state (use validated index)
        uint8_t activeIdx = validateActiveIndex();
        notifySubscribers(m_states[activeIdx]);

        // Update statistics
        m_commandCount += count;
    }

    // Release write lock
    xSemaphoreGive(m_writeMutex);

    // Update command duration
    m_lastCommandDuration = static_cast<uint32_t>(esp_timer_get_time() - startTime);

    return success;
}

// ==================== Subscription Methods ====================

bool StateStore::subscribe(StateChangeCallback callback) {
    // Sanity checks
    if (callback == nullptr || m_subscriberCount >= MAX_SUBSCRIBERS) {
        return false;
    }

    // Check if already subscribed
    for (uint8_t i = 0; i < m_subscriberCount; i++) {
        if (m_subscribers[i] == callback) {
            return false;  // Already subscribed
        }
    }

    // Add to subscriber list
    m_subscribers[m_subscriberCount] = callback;
    m_subscriberCount++;

    return true;
}

bool StateStore::unsubscribe(StateChangeCallback callback) {
    // Sanity check
    if (callback == nullptr) {
        return false;
    }

    // Find and remove subscriber
    for (uint8_t i = 0; i < m_subscriberCount; i++) {
        if (m_subscribers[i] == callback) {
            // Shift remaining subscribers down
            for (uint8_t j = i; j < m_subscriberCount - 1; j++) {
                m_subscribers[j] = m_subscribers[j + 1];
            }
            m_subscriberCount--;
            m_subscribers[m_subscriberCount] = nullptr;
            return true;
        }
    }

    return false;  // Not found
}

uint8_t StateStore::getSubscriberCount() const {
    return m_subscriberCount;
}

// ==================== Utility Methods ====================

void StateStore::reset() {
    // Create default state
    SystemState defaultState;

    // Dispatch command to reset
    class ResetCommand : public ICommand {
    public:
        ResetCommand(const SystemState& defaultState)
            : m_defaultState(defaultState) {}

        SystemState apply(const SystemState& current) const override {
            (void)current;
            return m_defaultState;
        }

        const char* getName() const override {
            return "Reset";
        }

    private:
        SystemState m_defaultState;
    };

    ResetCommand cmd(defaultState);
    dispatch(cmd);
}

void StateStore::getStats(uint32_t& outCommandCount, uint32_t& outLastCommandDuration) const {
    outCommandCount = m_commandCount;
    outLastCommandDuration = m_lastCommandDuration;
}

// ==================== Private Methods ====================

void StateStore::notifySubscribers(const SystemState& newState) {
    // Call all subscribers
    // This is called within the write lock, so subscribers should be FAST
    for (uint8_t i = 0; i < m_subscriberCount; i++) {
        if (m_subscribers[i] != nullptr) {
            m_subscribers[i](newState);
        }
    }
}

uint8_t StateStore::getInactiveIndex() const {
    // DEFENSIVE CHECK: Validate active index first to ensure calculation is safe
    // Prevents accessing m_states[2] with invalid index if m_activeIndex is corrupted
    uint8_t active = validateActiveIndex();
    uint8_t inactive = 1 - active;
    // Ensure result is 0 or 1 (defensive check - should never be >1, but protects against corruption)
    if (inactive > 1) {
        inactive = 0;
    }
    return inactive;
}

void StateStore::swapActiveIndex() {
    // Atomic swap using volatile member
    // DEFENSIVE CHECK: Validate before swap to ensure we're swapping to a valid index
    // This prevents corrupted m_activeIndex from causing out-of-bounds access to m_states[2]
    uint8_t newIndex = getInactiveIndex();
    m_activeIndex = newIndex;

    // Memory barrier to ensure writes complete before readers see new index
    // On ESP32, this is handled by the volatile keyword, but we can be explicit
    __asm__ __volatile__ ("" ::: "memory");
}

/**
 * @brief Validate and clamp m_activeIndex to safe range [0, 1]
 * 
 * DEFENSIVE CHECK: Prevents LoadProhibited crashes from corrupted double-buffer index.
 * 
 * StateStore uses double-buffering with m_states[2] array. m_activeIndex must be
 * 0 or 1 to access valid array elements. If corrupted (e.g., by memory corruption
 * or race condition), accessing m_states[m_activeIndex] would cause out-of-bounds
 * access and crash.
 * 
 * This validation ensures we always access valid array indices, returning safe
 * default (0) if corruption is detected.
 * 
 * @return Valid index (0 or 1), defaults to 0 if corrupted
 */
uint8_t StateStore::validateActiveIndex() const {
    // Validate m_activeIndex is 0 or 1 (only valid values for double buffer)
    if (m_activeIndex > 1) {
        // Corrupted - return safe default (0)
        // Note: We can't modify m_activeIndex here (const method), but we can
        // return a safe value for reading. The corruption should be fixed at
        // the source (swapActiveIndex, constructor, etc.)
        return 0;
    }
    return m_activeIndex;
}

} // namespace state
} // namespace lightwaveos
