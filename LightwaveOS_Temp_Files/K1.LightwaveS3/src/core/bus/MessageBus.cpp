/**
 * @file MessageBus.cpp
 * @brief Implementation of the pub/sub MessageBus
 *
 * Key implementation notes:
 * - Subscription table is fixed-size to avoid dynamic allocation
 * - Mutex only protects subscribe/unsubscribe, not publish
 * - publish() is designed to be fast and lock-free
 * - publishFromISR() can be called from interrupt context
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "MessageBus.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <esp_log.h>

static const char* TAG = "MessageBus";
#endif

namespace lightwaveos {
namespace bus {

// ============================================================================
// Singleton Instance
// ============================================================================

MessageBus& MessageBus::instance()
{
    static MessageBus instance;
    return instance;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

MessageBus::MessageBus()
    : m_mutex(nullptr)
    , m_totalPublished(0)
    , m_totalDelivered(0)
    , m_failedDeliveries(0)
{
    // Initialize subscription entries
    for (uint8_t i = 0; i < MAX_TRACKED_TYPES; i++) {
        m_entries[i] = SubscriptionEntry();
    }

    // Create mutex for thread-safe subscribe/unsubscribe
    m_mutex = xSemaphoreCreateMutex();

#ifndef NATIVE_BUILD
    if (m_mutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create mutex");
    } else {
        ESP_LOGI(TAG, "MessageBus initialized (max %d types, %d subs/type)",
                 MAX_TRACKED_TYPES, MAX_SUBSCRIBERS_PER_TYPE);
    }
#endif
}

MessageBus::~MessageBus()
{
    if (m_mutex != nullptr) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
}

// ============================================================================
// Subscription Management
// ============================================================================

bool MessageBus::subscribe(MessageType type, Node* node)
{
    if (node == nullptr) {
        return false;
    }

    // Take mutex
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
#ifndef NATIVE_BUILD
        ESP_LOGW(TAG, "subscribe: Failed to acquire mutex");
#endif
        return false;
    }

    bool result = false;

    // Find or create entry for this type
    SubscriptionEntry* entry = findOrCreateEntry(type);
    if (entry != nullptr) {
        // Check if node is already subscribed
        bool alreadySubscribed = false;
        for (uint8_t i = 0; i < entry->count; i++) {
            if (entry->subscribers[i] == node) {
                alreadySubscribed = true;
                break;
            }
        }

        if (!alreadySubscribed && entry->count < MAX_SUBSCRIBERS_PER_TYPE) {
            entry->subscribers[entry->count] = node;
            entry->count++;
            result = true;

#ifndef NATIVE_BUILD
            ESP_LOGD(TAG, "Node '%s' subscribed to type 0x%02X (now %d subs)",
                     node->getName(), static_cast<uint8_t>(type), entry->count);
#endif
        }
    }

    xSemaphoreGive(m_mutex);
    return result;
}

bool MessageBus::unsubscribe(MessageType type, Node* node)
{
    if (node == nullptr) {
        return false;
    }

    // Take mutex
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }

    bool result = false;

    SubscriptionEntry* entry = findEntry(type);
    if (entry != nullptr) {
        // Find and remove the node
        for (uint8_t i = 0; i < entry->count; i++) {
            if (entry->subscribers[i] == node) {
                // Shift remaining subscribers down
                for (uint8_t j = i; j < entry->count - 1; j++) {
                    entry->subscribers[j] = entry->subscribers[j + 1];
                }
                entry->subscribers[entry->count - 1] = nullptr;
                entry->count--;
                result = true;

                // Deactivate entry if no subscribers
                if (entry->count == 0) {
                    entry->active = false;
                }

#ifndef NATIVE_BUILD
                ESP_LOGD(TAG, "Node '%s' unsubscribed from type 0x%02X",
                         node->getName(), static_cast<uint8_t>(type));
#endif
                break;
            }
        }
    }

    xSemaphoreGive(m_mutex);
    return result;
}

void MessageBus::unsubscribeAll(Node* node)
{
    if (node == nullptr) {
        return;
    }

    // Take mutex
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }

    // Remove node from all entries
    for (uint8_t e = 0; e < MAX_TRACKED_TYPES; e++) {
        if (m_entries[e].active) {
            for (uint8_t i = 0; i < m_entries[e].count; i++) {
                if (m_entries[e].subscribers[i] == node) {
                    // Shift remaining subscribers down
                    for (uint8_t j = i; j < m_entries[e].count - 1; j++) {
                        m_entries[e].subscribers[j] = m_entries[e].subscribers[j + 1];
                    }
                    m_entries[e].subscribers[m_entries[e].count - 1] = nullptr;
                    m_entries[e].count--;

                    // Deactivate entry if no subscribers
                    if (m_entries[e].count == 0) {
                        m_entries[e].active = false;
                    }
                    break; // Node can only appear once per entry
                }
            }
        }
    }

#ifndef NATIVE_BUILD
    ESP_LOGD(TAG, "Node '%s' unsubscribed from all types", node->getName());
#endif

    xSemaphoreGive(m_mutex);
}

// ============================================================================
// Publishing
// ============================================================================

uint8_t MessageBus::publish(const Message& msg, TickType_t timeout)
{
    m_totalPublished++;

    // Find subscribers for this message type
    // Note: We read the subscription table without locking for performance.
    // This is safe because:
    // 1. Entries are only added/removed, not modified in place
    // 2. count is read atomically
    // 3. Worst case: we miss a just-added subscriber or try to send to
    //    a just-removed subscriber (which will fail gracefully)

    const SubscriptionEntry* entry = nullptr;
    for (uint8_t i = 0; i < MAX_TRACKED_TYPES; i++) {
        if (m_entries[i].active && m_entries[i].type == msg.type) {
            entry = &m_entries[i];
            break;
        }
    }

    if (entry == nullptr || entry->count == 0) {
        return 0;
    }

    uint8_t delivered = 0;
    uint8_t count = entry->count; // Snapshot count

    for (uint8_t i = 0; i < count && i < MAX_SUBSCRIBERS_PER_TYPE; i++) {
        Node* node = entry->subscribers[i];
        if (node != nullptr && node->isRunning()) {
            if (node->send(msg, timeout)) {
                delivered++;
                m_totalDelivered++;
            } else {
                m_failedDeliveries++;
#ifndef NATIVE_BUILD
                ESP_LOGD(TAG, "Failed to deliver to '%s' (queue full)",
                         node->getName());
#endif
            }
        }
    }

    return delivered;
}

uint8_t MessageBus::publishFromISR(const Message& msg)
{
    m_totalPublished++;

    // Same logic as publish(), but use sendFromISR()

    const SubscriptionEntry* entry = nullptr;
    for (uint8_t i = 0; i < MAX_TRACKED_TYPES; i++) {
        if (m_entries[i].active && m_entries[i].type == msg.type) {
            entry = &m_entries[i];
            break;
        }
    }

    if (entry == nullptr || entry->count == 0) {
        return 0;
    }

    uint8_t delivered = 0;
    uint8_t count = entry->count;

    for (uint8_t i = 0; i < count && i < MAX_SUBSCRIBERS_PER_TYPE; i++) {
        Node* node = entry->subscribers[i];
        if (node != nullptr && node->isRunning()) {
            if (node->sendFromISR(msg)) {
                delivered++;
                m_totalDelivered++;
            } else {
                m_failedDeliveries++;
            }
        }
    }

    return delivered;
}

// ============================================================================
// Diagnostics
// ============================================================================

uint8_t MessageBus::getSubscriberCount(MessageType type) const
{
    for (uint8_t i = 0; i < MAX_TRACKED_TYPES; i++) {
        if (m_entries[i].active && m_entries[i].type == type) {
            return m_entries[i].count;
        }
    }
    return 0;
}

uint8_t MessageBus::getActiveEntryCount() const
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_TRACKED_TYPES; i++) {
        if (m_entries[i].active) {
            count++;
        }
    }
    return count;
}

void MessageBus::resetStats()
{
    m_totalPublished = 0;
    m_totalDelivered = 0;
    m_failedDeliveries = 0;
}

void MessageBus::dumpSubscriptions()
{
#ifndef NATIVE_BUILD
    Serial.println(F("\n=== MessageBus Subscriptions ==="));
    Serial.printf("Active entries: %d/%d\n", getActiveEntryCount(), MAX_TRACKED_TYPES);
    Serial.printf("Published: %lu, Delivered: %lu, Failed: %lu\n",
                  m_totalPublished, m_totalDelivered, m_failedDeliveries);
    Serial.println();

    for (uint8_t i = 0; i < MAX_TRACKED_TYPES; i++) {
        if (m_entries[i].active) {
            Serial.printf("Type 0x%02X: %d subscribers [ ",
                          static_cast<uint8_t>(m_entries[i].type),
                          m_entries[i].count);

            for (uint8_t j = 0; j < m_entries[i].count; j++) {
                if (m_entries[i].subscribers[j] != nullptr) {
                    Serial.printf("%s ", m_entries[i].subscribers[j]->getName());
                }
            }
            Serial.println("]");
        }
    }
    Serial.println(F("================================\n"));
#endif
}

// ============================================================================
// Private Helpers
// ============================================================================

SubscriptionEntry* MessageBus::findEntry(MessageType type)
{
    for (uint8_t i = 0; i < MAX_TRACKED_TYPES; i++) {
        if (m_entries[i].active && m_entries[i].type == type) {
            return &m_entries[i];
        }
    }
    return nullptr;
}

SubscriptionEntry* MessageBus::findOrCreateEntry(MessageType type)
{
    // First, look for existing entry
    SubscriptionEntry* existing = findEntry(type);
    if (existing != nullptr) {
        return existing;
    }

    // Find an empty slot
    for (uint8_t i = 0; i < MAX_TRACKED_TYPES; i++) {
        if (!m_entries[i].active) {
            m_entries[i].type = type;
            m_entries[i].count = 0;
            m_entries[i].active = true;
            return &m_entries[i];
        }
    }

#ifndef NATIVE_BUILD
    ESP_LOGW(TAG, "Subscription table full! Cannot add type 0x%02X",
             static_cast<uint8_t>(type));
#endif
    return nullptr;
}

} // namespace bus
} // namespace lightwaveos
