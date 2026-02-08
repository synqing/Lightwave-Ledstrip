/**
 * @file Actor.h
 * @brief Base Actor class for LightwaveOS v2 cross-core communication
 *
 * The Actor Model provides thread-safe, lock-free communication between
 * cores on the ESP32-S3. Each Actor runs on a pinned core and communicates
 * exclusively via message queues, eliminating race conditions.
 *
 * Architecture:
 *   Core 0 (Network/Input): NetworkActor, HmiActor, PluginManagerActor
 *   Core 1 (Rendering): RendererActor, StateStoreActor
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <cstdint>
#include <cstring>

#ifdef NATIVE_BUILD
// Native build - use mocks
#include "mocks/freertos_mock.h"
#else
// ESP32 build - use real FreeRTOS
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <Arduino.h>
#endif

namespace lightwaveos {
namespace actors {

// ============================================================================
// Message Types
// ============================================================================

/**
 * @brief All message types in the system
 *
 * Message types are categorized:
 * - 0x00-0x1F: Effect commands
 * - 0x20-0x3F: Zone commands
 * - 0x40-0x5F: Transition commands
 * - 0x60-0x7F: System commands
 * - 0x80-0xFF: Events (notifications)
 */
enum class MessageType : uint8_t {
    // Effect commands (0x00-0x1F)
    SET_EFFECT          = 0x00,
    SET_BRIGHTNESS      = 0x01,
    SET_SPEED           = 0x02,
    SET_PALETTE         = 0x03,
    SET_SATURATION      = 0x04,
    SET_INTENSITY       = 0x05,
    SET_COMPLEXITY      = 0x06,
    SET_VARIATION       = 0x07,
    SET_HUE             = 0x08,
    SET_MOOD            = 0x09,  // Sensory Bridge mood (0-255): reactive to smooth
    SET_FADE_AMOUNT     = 0x0A,  // Trail fade speed (0-255): 0=no fade, higher=faster

    // Zone commands (0x20-0x3F)
    ZONE_ENABLE         = 0x20,
    ZONE_DISABLE        = 0x21,
    ZONE_SET_EFFECT     = 0x22,
    ZONE_SET_PALETTE    = 0x23,
    ZONE_SET_BRIGHTNESS = 0x24,
    ZONE_SET_COUNT      = 0x25,

    // Transition commands (0x40-0x5F)
    TRIGGER_TRANSITION  = 0x40,
    SET_TRANSITION_TYPE = 0x41,
    SET_TRANSITION_TIME = 0x42,
    CANCEL_TRANSITION   = 0x43,
    START_TRANSITION    = 0x44,  // param1=effectId, param2=transitionType, param4=durationMs

    // System commands (0x60-0x7F)
    SHUTDOWN            = 0x60,
    HEALTH_CHECK        = 0x61,
    RESET_STATE         = 0x62,
    SAVE_STATE          = 0x63,
    LOAD_STATE          = 0x64,
    PING                = 0x65,
    PONG                = 0x66,

    // Sync commands (0x68-0x6F)
    SYNC_REQUEST        = 0x68,
    SYNC_RESPONSE       = 0x69,
    SYNC_STATE          = 0x6A,

    // Show control commands (0x70-0x7F)
    SHOW_LOAD           = 0x70,
    SHOW_START          = 0x71,
    SHOW_STOP           = 0x72,
    SHOW_PAUSE          = 0x73,
    SHOW_RESUME         = 0x74,
    SHOW_SEEK           = 0x75,
    SHOW_UNLOAD         = 0x76,

    // Events/Notifications (0x80-0xFF)
    EFFECT_CHANGED      = 0x80,
    FRAME_RENDERED      = 0x81,
    STATE_UPDATED       = 0x82,
    PALETTE_CHANGED     = 0x83,
    ZONE_CHANGED        = 0x84,
    TRANSITION_COMPLETE = 0x85,
    ERROR_OCCURRED      = 0x86,
    HEALTH_STATUS       = 0x87,

    // HMI Events (0x90-0x9F)
    ENCODER_ROTATED     = 0x90,
    ENCODER_PRESSED     = 0x91,
    ENCODER_RELEASED    = 0x92,

    // Network Events (0xA0-0xAF)
    CLIENT_CONNECTED    = 0xA0,
    CLIENT_DISCONNECTED = 0xA1,
    COMMAND_RECEIVED    = 0xA2,

    // Show Events (0xB0-0xBF)
    SHOW_STARTED        = 0xB0,
    SHOW_STOPPED        = 0xB1,
    SHOW_PAUSED          = 0xB2,
    SHOW_RESUMED         = 0xB3,
    SHOW_CHAPTER_CHANGED = 0xB4,
    SHOW_COMPLETED       = 0xB5,

    // Audio Events (0xC0-0xCF) - Phase 2
    AUDIO_TEMPO_ESTIMATE   = 0xC0,  // param4 = bpm * 100 (fixed point)
    AUDIO_BEAT_OBSERVATION = 0xC1,  // param1 = strength (0-255), param2 = is_downbeat
    AUDIO_BANDS_UPDATED    = 0xC2,  // Notification that band analysis completed
    AUDIO_ERROR            = 0xC3,  // param1 = error code

    // Trinity sync commands (0xD0-0xDF)
    TRINITY_BEAT  = 0xD0,  // param1=bpm_hi, param2=bpm_lo, param3=phase, param4=flags
    TRINITY_MACRO = 0xD1,  // param1-4 = packed macro values
    TRINITY_SYNC  = 0xD2,  // param1=action, param4=position_ms
    TRINITY_SEGMENT = 0xD3 // param1=index, param2-3=labelHash16 (hi/lo), param4=start_ms, _reserved=end_ms
};

// ============================================================================
// Message Structure
// ============================================================================

/**
 * @brief Fixed-size message structure for queue-based communication
 *
 * Design constraints:
 * - 16 bytes maximum for efficient FreeRTOS queue transfer
 * - No pointers (prevents use-after-free across cores)
 * - Timestamp for debugging and ordering
 *
 * Parameter usage varies by message type:
 * - SET_EFFECT: param1=effectId, param4=transitionMs
 * - SET_BRIGHTNESS: param1=brightness (0-255)
 * - ZONE_SET_EFFECT: param1=zoneId, param2=effectId
 * - TRIGGER_TRANSITION: param1=transitionType, param4=durationMs
 * - START_TRANSITION: param1=effectId, param2=transitionType, param4=durationMs
 */
struct Message {
    MessageType type;       // 1 byte - Message type
    uint8_t param1;         // 1 byte - Primary parameter
    uint8_t param2;         // 1 byte - Secondary parameter
    uint8_t param3;         // 1 byte - Tertiary parameter
    uint32_t param4;        // 4 bytes - Extended parameter (duration, flags, etc.)
    uint32_t timestamp;     // 4 bytes - Creation timestamp (millis)
    uint32_t _reserved;     // 4 bytes - Future use / alignment padding
    // Total: 16 bytes

    Message()
        : type(MessageType::HEALTH_CHECK)
        , param1(0), param2(0), param3(0)
        , param4(0), timestamp(0), _reserved(0) {}

    Message(MessageType t, uint8_t p1 = 0, uint8_t p2 = 0,
            uint8_t p3 = 0, uint32_t p4 = 0)
        : type(t)
        , param1(p1), param2(p2), param3(p3)
        , param4(p4)
        , timestamp(millis())
        , _reserved(0) {}

    /**
     * @brief Check if this is a command (vs event/notification)
     */
    bool isCommand() const {
        return static_cast<uint8_t>(type) < 0x80;
    }

    /**
     * @brief Check if this is an event/notification
     */
    bool isEvent() const {
        return static_cast<uint8_t>(type) >= 0x80;
    }
};

static_assert(sizeof(Message) == 16, "Message must be exactly 16 bytes");

// ============================================================================
// Actor Configuration
// ============================================================================

/**
 * @brief Configuration for Actor creation
 *
 * Stack sizes (in words, 4 bytes each):
 * - RendererActor: 4096 words (16KB) - effect rendering + FastLED
 * - NetworkActor: 3072 words (12KB) - WebSocket + HTTP handling
 * - HmiActor: 2048 words (8KB) - encoder polling
 * - Others: 2048 words (8KB) - default
 *
 * Priorities (higher = more important):
 * - RendererActor: 5 - Must hit 120 FPS
 * - NetworkActor: 3 - Responsive but not critical
 * - Others: 2 - Background processing
 */
struct ActorConfig {
    const char* name;           // Task name for debugging
    uint16_t stackSize;         // Stack size in words (x4 for bytes)
    uint8_t priority;           // FreeRTOS priority (0-configMAX_PRIORITIES)
    BaseType_t coreId;          // Core affinity (0 or 1)
    uint8_t queueSize;          // Message queue depth
    TickType_t tickInterval;    // Interval for onTick() callback (0 = disabled)

    ActorConfig()
        : name("Actor")
        , stackSize(2048)
        , priority(2)
        , coreId(0)
        , queueSize(16)
        , tickInterval(0) {}

    ActorConfig(const char* n, uint16_t stack, uint8_t prio,
                BaseType_t core, uint8_t qSize, TickType_t tick = 0)
        : name(n)
        , stackSize(stack)
        , priority(prio)
        , coreId(core)
        , queueSize(qSize)
        , tickInterval(tick) {}
};

// ============================================================================
// Actor Base Class
// ============================================================================

/**
 * @brief Base class for all Actors in the system
 *
 * Lifecycle:
 * 1. Constructor - Store config, allocate queue
 * 2. start() - Create FreeRTOS task, call onStart()
 * 3. run() loop - Receive messages, dispatch to onMessage()
 * 4. stop() - Signal shutdown, wait for task to exit
 * 5. Destructor - Clean up queue and resources
 *
 * Thread safety:
 * - send() is thread-safe (can be called from any core)
 * - sendFromISR() is ISR-safe (interrupt context)
 * - onMessage() is always called from the Actor's own task
 */
class Actor {
public:
    /**
     * @brief Construct an Actor with the given configuration
     * @param config Actor configuration (name, stack, priority, etc.)
     */
    explicit Actor(const ActorConfig& config);

    /**
     * @brief Virtual destructor - cleans up FreeRTOS resources
     */
    virtual ~Actor();

    // Prevent copying (FreeRTOS handles can't be copied)
    Actor(const Actor&) = delete;
    Actor& operator=(const Actor&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    /**
     * @brief Start the Actor's FreeRTOS task
     * @return true if task created successfully
     */
    bool start();

    /**
     * @brief Stop the Actor gracefully
     *
     * Sends SHUTDOWN message and waits up to 100ms for the task to exit.
     * If the task doesn't exit in time, it will be forcefully deleted.
     */
    void stop();

    /**
     * @brief Check if the Actor is currently running
     */
    bool isRunning() const { return m_running; }

    // ========================================================================
    // Message Passing
    // ========================================================================

    /**
     * @brief Send a message to this Actor's queue
     *
     * Thread-safe - can be called from any task on any core.
     *
     * @param msg Message to send
     * @param timeout Ticks to wait if queue is full (0 = don't wait)
     * @return true if message was queued successfully
     */
    bool send(const Message& msg, TickType_t timeout = 0);

    /**
     * @brief Send a message from an ISR context
     *
     * Use this from interrupt handlers. Never blocks.
     *
     * @param msg Message to send
     * @return true if message was queued (may trigger context switch)
     */
    bool sendFromISR(const Message& msg);

    /**
     * @brief Get number of messages waiting in the queue
     */
    UBaseType_t getQueueLength() const;

    /**
     * @brief Get queue utilization percentage (0-100)
     * @return Utilization percentage, or 0 if queue is null
     */
    uint8_t getQueueUtilization() const;

    // ========================================================================
    // Diagnostics
    // ========================================================================

    /**
     * @brief Get the Actor's name
     */
    const char* getName() const { return m_config.name; }

    /**
     * @brief Get the core this Actor runs on
     */
    BaseType_t getCoreId() const { return m_config.coreId; }

    /**
     * @brief Get stack high water mark (minimum free stack ever)
     *
     * Useful for tuning stack sizes. If this gets too low, increase
     * the Actor's stack size.
     *
     * @return Minimum free stack in words (multiply by 4 for bytes)
     */
    UBaseType_t getStackHighWaterMark() const;

    /**
     * @brief Get total messages received since start
     */
    uint32_t getMessageCount() const { return m_messageCount; }

protected:
    // ========================================================================
    // Override in Derived Classes
    // ========================================================================

    /**
     * @brief Called once when the Actor starts
     *
     * Override to perform initialization that requires the task context
     * (e.g., initializing hardware that needs to run on a specific core).
     */
    virtual void onStart() {}

    /**
     * @brief Called for each received message
     *
     * This is the main message handler. Implement your Actor's logic here.
     * Always called from the Actor's own task (single-threaded).
     *
     * @param msg The received message
     */
    virtual void onMessage(const Message& msg) = 0;

    /**
     * @brief Called periodically when no messages are pending
     *
     * Override for time-based work (e.g., rendering frames).
     * The interval is controlled by config.tickInterval.
     * If tickInterval is 0, this is never called.
     */
    virtual void onTick() {}

    /**
     * @brief Called when the Actor is stopping
     *
     * Override to clean up resources before the task exits.
     */
    virtual void onStop() {}

    // ========================================================================
    // Utilities for Derived Classes
    // ========================================================================

    /**
     * @brief Get current tick count (useful for timing)
     */
    uint32_t getTickCount() const;

    /**
     * @brief Sleep for the specified duration
     * @param ms Milliseconds to sleep
     */
    void sleep(uint32_t ms);

    /**
     * @brief Get the Actor's configuration
     */
    const ActorConfig& getConfig() const { return m_config; }

private:
    /**
     * @brief Static task entry point (trampoline to run())
     */
    static void taskFunction(void* param);

    /**
     * @brief Main run loop - receives messages and dispatches
     */
    void run();

    ActorConfig m_config;               // Configuration
    TaskHandle_t m_taskHandle;          // FreeRTOS task handle
    QueueHandle_t m_queue;              // Message queue
    volatile bool m_running;            // Running flag (atomic on ESP32)
    volatile bool m_shutdownRequested;  // Shutdown signal
    uint32_t m_messageCount;            // Diagnostic counter
};

// ============================================================================
// Predefined Actor Configurations
// ============================================================================

namespace ActorConfigs {

/**
 * @brief Configuration for RendererActor
 *
 * Runs on Core 1 at highest priority for deterministic 120 FPS rendering.
 * Large queue (32) to buffer commands during frame rendering.
 * Tick interval of 8ms (~120 FPS) for continuous rendering.
 * 
 * Stack size: 6144 words (24KB) for zone mode safety margin.
 * Actual usage: ~8-10KB (effect rendering, FastLED, context setup).
 * High water mark monitoring recommended to verify adequate margin.
 */
inline ActorConfig Renderer() {
    return ActorConfig(
        "Renderer",     // name
        4096,           // stackSize (16KB) - reduced for memory constraints
        5,              // priority (highest)
        1,              // coreId (Core 1 - application)
        32,             // queueSize
        (pdMS_TO_TICKS(8) > 0 ? pdMS_TO_TICKS(8) : 1) // tickInterval (~120 FPS, clamp to 1 tick)
    );
}

/**
 * @brief Configuration for NetworkActor
 *
 * Runs on Core 0 where WiFi stack runs. Medium priority.
 * 
 * Stack size: 3072 words (12KB) with 50% safety margin.
 * Actual usage: ~6-8KB (WebSocket, HTTP, JSON parsing, mDNS).
 * High water mark monitoring recommended to verify adequate margin.
 */
inline ActorConfig Network() {
    return ActorConfig(
        "Network",      // name
        3072,           // stackSize (12KB) - 50% safety margin over ~6-8KB usage
        3,              // priority
        0,              // coreId (Core 0 - system)
        16,             // queueSize
        0               // tickInterval (event-driven)
    );
}

/**
 * @brief Configuration for HmiActor
 *
 * Runs on Core 0 for I2C encoder polling.
 * Tick interval of 20ms for 50Hz polling rate.
 * 
 * Stack size: 2048 words (8KB) with 50% safety margin.
 * Actual usage: ~2-4KB (I2C operations, encoder state).
 * High water mark monitoring recommended to verify adequate margin.
 */
inline ActorConfig Hmi() {
    return ActorConfig(
        "Hmi",          // name
        2048,           // stackSize (8KB) - 50% safety margin over ~2-4KB usage
        2,              // priority
        0,              // coreId (Core 0 - system)
        16,             // queueSize
        pdMS_TO_TICKS(20) // tickInterval (50Hz polling)
    );
}

/**
 * @brief Configuration for StateStoreActor
 *
 * Manages persistent state (NVS). Runs on Core 1 with Renderer.
 * 
 * Stack size: 2048 words (8KB) with 50% safety margin.
 * Actual usage: ~2-4KB (NVS operations, state management).
 * High water mark monitoring recommended to verify adequate margin.
 */
inline ActorConfig StateStore() {
    return ActorConfig(
        "StateStore",   // name
        2048,           // stackSize (8KB) - 50% safety margin over ~2-4KB usage
        2,              // priority
        1,              // coreId (Core 1)
        16,             // queueSize
        0               // tickInterval (event-driven)
    );
}

/**
 * @brief Configuration for SyncManagerActor
 *
 * Handles multi-device synchronization. Runs on Core 0 with network.
 * Tick interval of 100ms for heartbeat/discovery updates.
 * 
 * Stack size: 8192 words (32KB) with 50% safety margin.
 * Actual usage: ~16-20KB (JSON serialization, WebSocket frames, state sync).
 * High water mark monitoring recommended to verify adequate margin.
 */
inline ActorConfig SyncManager() {
    return ActorConfig(
        "SyncManager",  // name
        8192,           // stackSize (32KB) - 50% safety margin over ~16-20KB usage
        2,              // priority
        0,              // coreId (Core 0 - network)
        16,             // queueSize
        pdMS_TO_TICKS(100) // tickInterval (100ms for heartbeats/discovery)
    );
}

/**
 * @brief Configuration for PluginManagerActor
 *
 * Manages plugin lifecycle. Runs on Core 0.
 */
inline ActorConfig PluginManager() {
    return ActorConfig(
        "PluginMgr",    // name
        2048,           // stackSize (8KB)
        2,              // priority
        0,              // coreId (Core 0)
        16,             // queueSize
        0               // tickInterval (event-driven)
    );
}

} // namespace ActorConfigs

} // namespace actors
} // namespace lightwaveos
