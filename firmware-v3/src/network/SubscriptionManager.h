#pragma once

#include <stdint.h>
#include <stddef.h>

namespace lightwaveos {
namespace network {

/**
 * @brief Manages a fixed-size list of subscriber IDs (e.g., WebSocket client IDs).
 * 
 * Logic is isolated here for easier unit testing and separation of concerns.
 * Thread safety must be managed by the caller.
 * 
 * @tparam MAX_CLIENTS Maximum number of subscribers
 */
template <size_t MAX_CLIENTS>
class SubscriptionManager {
public:
    SubscriptionManager() : m_count(0) {
        for (size_t i = 0; i < MAX_CLIENTS; i++) {
            m_ids[i] = 0;
        }
    }

    /**
     * @brief Add a subscriber ID.
     * @param id The ID to add.
     * @return true if added or already exists, false if full.
     */
    bool add(uint32_t id) {
        if (contains(id)) return true;
        if (m_count >= MAX_CLIENTS) return false;
        m_ids[m_count++] = id;
        return true;
    }

    /**
     * @brief Remove a subscriber ID.
     * @param id The ID to remove.
     * @return true if removed, false if not found.
     */
    bool remove(uint32_t id) {
        for (size_t i = 0; i < m_count; i++) {
            if (m_ids[i] == id) {
                // Swap with last element for O(1) removal
                // Decrement count first to get the index of the last element
                size_t lastIdx = m_count - 1;
                m_ids[i] = m_ids[lastIdx];
                m_ids[lastIdx] = 0; // Clear the slot (optional but good for debugging)
                m_count--;
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Check if an ID is subscribed.
     */
    bool contains(uint32_t id) const {
        for (size_t i = 0; i < m_count; i++) {
            if (m_ids[i] == id) return true;
        }
        return false;
    }

    /**
     * @brief Get current number of subscribers.
     */
    size_t count() const { return m_count; }

    /**
     * @brief Get ID at index.
     * @param index Index (0 to count-1)
     * @return ID or 0 if out of bounds.
     */
    uint32_t get(size_t index) const {
        if (index >= m_count) return 0;
        return m_ids[index];
    }

    /**
     * @brief Clear all subscribers.
     */
    void clear() {
        m_count = 0;
        for (size_t i = 0; i < MAX_CLIENTS; i++) {
            m_ids[i] = 0;
        }
    }

private:
    uint32_t m_ids[MAX_CLIENTS];
    size_t m_count;
};

} // namespace network
} // namespace lightwaveos
