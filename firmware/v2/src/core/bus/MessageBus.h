/**
 * @file MessageBus.h
 * @brief Pub/Sub message bus for Actor communication
 *
 * The MessageBus provides a publish/subscribe pattern on top of the Actor
 * system. Actors can subscribe to specific message types and receive
 * broadcasts without knowing about other Actors.
 *
 * Usage:
 *   // In NetworkActor
 *   bus.subscribe(MessageType::EFFECT_CHANGED, this);
 *
 *   // In RendererActor
 *   bus.publish(Message(MessageType::EFFECT_CHANGED, newEffectId));
 *
 * Thread Safety:
 * - subscribe/unsubscribe are protected by a mutex
 * - publish is lock-free (reads subscription table atomically)
 * - publishFromISR is interrupt-safe
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "../actors/Actor.h"
#include <atomic>
#include <cstdint>
#include <cstring>

// FreeRTOS headers are included by Actor.h based on NATIVE_BUILD

namespace lightwaveos {
namespace bus {

// Forward declaration
using actors::Actor;
using actors::Message;
using actors::MessageType;

// ============================================================================
// Configuration
// ============================================================================

/**
 * @brief Maximum number of subscribers per message type
 *
 * Keep this small to minimize memory and iteration overhead.
 * 8 subscribers is enough for most use cases.
 */
constexpr uint8_t MAX_SUBSCRIBERS_PER_TYPE = 8;

/**
 * @brief Number of unique message types we track subscriptions for
 *
 * We don't need to track all 256 possible types - just the commonly
 * published events. This saves memory (256 * 8 * 4 = 8KB otherwise).
 */
constexpr uint8_t MAX_TRACKED_TYPES = 32;

// ============================================================================
// Subscription Entry
// ============================================================================

/**
 * @brief Entry in the subscription table
 */
struct SubscriptionEntry {
    MessageType type;                               // Message type to match
    Actor* subscribers[MAX_SUBSCRIBERS_PER_TYPE];   // Subscribed actors
    uint8_t count;                                  // Number of subscribers
    bool active;                                    // Entry is in use

    SubscriptionEntry()
        : type(MessageType::HEALTH_CHECK)
        , count(0)
        , active(false)
    {
        memset(subscribers, 0, sizeof(subscribers));
    }
};

// ============================================================================
// MessageBus Class
// ============================================================================

/**
 * @brief Singleton message bus for Actor pub/sub communication
 *
 * The MessageBus maintains a subscription table mapping message types
 * to interested Actors. When a message is published, it's forwarded
 * to all subscribed Actors' queues.
 *
 * Design decisions:
 * - Singleton pattern for global access
 * - Fixed-size subscription table (no dynamic allocation)
 * - Lock-free publish for performance
 * - Mutex-protected subscribe/unsubscribe for safety
 */
class MessageBus {
public:
    /**
     * @brief Get the singleton instance
     */
    static MessageBus& instance();

    // Prevent copying
    MessageBus(const MessageBus&) = delete;
    MessageBus& operator=(const MessageBus&) = delete;

    // ========================================================================
    // Subscription Management
    // ========================================================================

    /**
     * @brief Subscribe an Actor to a message type
     *
     * Thread-safe - protected by mutex.
     *
     * @param type Message type to subscribe to
     * @param actor Actor to receive messages
     * @return true if subscription was added
     */
    bool subscribe(MessageType type, Actor* actor);

    /**
     * @brief Unsubscribe an Actor from a message type
     *
     * Thread-safe - protected by mutex.
     *
     * @param type Message type to unsubscribe from
     * @param actor Actor to remove
     * @return true if subscription was removed
     */
    bool unsubscribe(MessageType type, Actor* actor);

    /**
     * @brief Unsubscribe an Actor from all message types
     *
     * Call this when an Actor is being destroyed.
     *
     * @param actor Actor to remove from all subscriptions
     */
    void unsubscribeAll(Actor* actor);

    // ========================================================================
    // Publishing
    // ========================================================================

    /**
     * @brief Publish a message to all subscribers
     *
     * Lock-free for performance. Messages are sent to each subscriber's
     * queue with zero wait time (non-blocking).
     *
     * @param msg Message to publish
     * @param timeout Ticks to wait per subscriber if queue full (0 = don't wait)
     * @return Number of Actors that received the message
     */
    uint8_t publish(const Message& msg, TickType_t timeout = 0);

    /**
     * @brief Publish a message from an ISR context
     *
     * Use this from interrupt handlers. Never blocks.
     *
     * @param msg Message to publish
     * @return Number of Actors that received the message
     */
    uint8_t publishFromISR(const Message& msg);

    // ========================================================================
    // Diagnostics
    // ========================================================================

    /**
     * @brief Get number of subscribers for a message type
     */
    uint8_t getSubscriberCount(MessageType type) const;

    /**
     * @brief Get total number of active subscription entries
     */
    uint8_t getActiveEntryCount() const;

    /**
     * @brief Get total messages published since startup
     */
    uint32_t getTotalPublished() const { return m_totalPublished; }

    /**
     * @brief Get total messages delivered (sum across all subscribers)
     */
    uint32_t getTotalDelivered() const { return m_totalDelivered; }

    /**
     * @brief Get number of failed deliveries (queue full)
     */
    uint32_t getFailedDeliveries() const { return m_failedDeliveries; }

    /**
     * @brief Reset statistics counters
     */
    void resetStats();

    /**
     * @brief Dump subscription table to serial (debug)
     */
    void dumpSubscriptions();

private:
    // Private constructor for singleton
    MessageBus();
    ~MessageBus();

    /**
     * @brief Find the subscription entry for a message type
     * @return Pointer to entry, or nullptr if not found
     */
    static SubscriptionEntry* findEntry(SubscriptionEntry* table, MessageType type);
    static const SubscriptionEntry* findEntry(const SubscriptionEntry* table, MessageType type);

    /**
     * @brief Find or create a subscription entry
     * @return Pointer to entry, or nullptr if table full
     */
    static SubscriptionEntry* findOrCreateEntry(SubscriptionEntry* table, MessageType type);

    // Subscription table (copy-on-write, double-buffered).
    // subscribe/unsubscribe mutate the inactive table then atomically swap.
    SubscriptionEntry m_tables[2][MAX_TRACKED_TYPES];
    std::atomic<uint8_t> m_activeTableIndex;

    // Latched (sticky) state messages for selected MessageTypes.
    // Used to solve publish-before-subscribe races for state-like topics.
    struct LatchedSlot {
        MessageType type;
        std::atomic<uint32_t> seq;
        Message buffers[2];
        bool active;
    };

    static constexpr uint8_t MAX_LATCHED_TYPES = 8;
    LatchedSlot m_latched[MAX_LATCHED_TYPES];
    uint32_t m_failedLatchedDeliveries;

    // Mutex for subscribe/unsubscribe operations
    SemaphoreHandle_t m_mutex;

    // Statistics
    volatile uint32_t m_totalPublished;
    volatile uint32_t m_totalDelivered;
    volatile uint32_t m_failedDeliveries;
};

// ============================================================================
// Convenience Macros
// ============================================================================

/**
 * @brief Quick access to the MessageBus singleton
 */
#define MSG_BUS (::lightwaveos::bus::MessageBus::instance())

/**
 * @brief Subscribe this Actor to a message type
 */
#define SUBSCRIBE(type) MSG_BUS.subscribe(type, this)

/**
 * @brief Unsubscribe this Actor from a message type
 */
#define UNSUBSCRIBE(type) MSG_BUS.unsubscribe(type, this)

/**
 * @brief Publish a message to all subscribers
 */
#define PUBLISH(msg) MSG_BUS.publish(msg)

} // namespace bus
} // namespace lightwaveos
