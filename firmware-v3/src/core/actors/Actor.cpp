/**
 * @file Actor.cpp
 * @brief Implementation of the Actor base class
 *
 * This file implements the FreeRTOS-based Actor system for LightwaveOS v2.
 *
 * Key implementation details:
 * - Tasks are pinned to specific cores using xTaskCreatePinnedToCore()
 * - Message queues use zero-copy 16-byte Message structs
 * - Graceful shutdown with timeout and forced deletion fallback
 * - Stack overflow detection via high water mark monitoring
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "Actor.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <esp_log.h>
#include <esp_task_wdt.h>

static const char* TAG = "Actor";
#endif

namespace lightwaveos {
namespace actors {

// ============================================================================
// Constructor / Destructor
// ============================================================================

Actor::Actor(const ActorConfig& config)
    : m_config(config)
    , m_taskHandle(nullptr)
    , m_queue(nullptr)
    , m_running(false)
    , m_shutdownRequested(false)
    , m_messageCount(0)
{
    // Create the message queue
    // Queue item size = sizeof(Message) = 16 bytes
    m_queue = xQueueCreate(m_config.queueSize, sizeof(Message));

    if (m_queue == nullptr) {
#ifndef NATIVE_BUILD
        ESP_LOGE(TAG, "[%s] Failed to create queue (size=%d)",
                 m_config.name, m_config.queueSize);
#endif
    }
}

Actor::~Actor()
{
    // Ensure the task is stopped
    if (m_running) {
        stop();
    }

    // Delete the queue
    if (m_queue != nullptr) {
        vQueueDelete(m_queue);
        m_queue = nullptr;
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

bool Actor::start()
{
    if (m_running) {
#ifndef NATIVE_BUILD
        ESP_LOGW(TAG, "[%s] Already running", m_config.name);
#endif
        return false;
    }

    if (m_queue == nullptr) {
#ifndef NATIVE_BUILD
        ESP_LOGE(TAG, "[%s] Cannot start - queue not created", m_config.name);
#endif
        return false;
    }

    m_shutdownRequested = false;
    m_messageCount = 0;

    // Create the FreeRTOS task pinned to the specified core
    BaseType_t result = xTaskCreatePinnedToCore(
        taskFunction,           // Task function
        m_config.name,          // Task name
        m_config.stackSize * 4, // Stack size in bytes (words × 4)
        this,                   // Parameter (this pointer)
        m_config.priority,      // Priority
        &m_taskHandle,          // Task handle output
        m_config.coreId         // Core ID (0 or 1)
    );

    if (result != pdPASS) {
#ifndef NATIVE_BUILD
        ESP_LOGE(TAG, "[%s] Failed to create task (result=%d)",
                 m_config.name, result);
#endif
        m_taskHandle = nullptr;
        return false;
    }

#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "[%s] Started on core %d (priority=%d, stack=%d)",
             m_config.name, m_config.coreId, m_config.priority,
             m_config.stackSize * 4);
#endif

    return true;
}

void Actor::stop()
{
    if (!m_running && m_taskHandle == nullptr) {
        return; // Already stopped
    }

#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "[%s] Stopping...", m_config.name);
#endif

    // Signal shutdown
    m_shutdownRequested = true;

    // Send a SHUTDOWN message to wake up the task if it's waiting
    Message shutdownMsg(MessageType::SHUTDOWN);
    send(shutdownMsg, pdMS_TO_TICKS(10));

    // Wait for the task to exit gracefully (100ms timeout)
    const TickType_t timeout = pdMS_TO_TICKS(100);
    TickType_t startTick = xTaskGetTickCount();

    while (m_running && (xTaskGetTickCount() - startTick) < timeout) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // If still running after timeout, force delete
    if (m_running && m_taskHandle != nullptr) {
#ifndef NATIVE_BUILD
        ESP_LOGW(TAG, "[%s] Force deleting task (did not exit gracefully)",
                 m_config.name);
#endif
        vTaskDelete(m_taskHandle);
        m_running = false;
    }

    m_taskHandle = nullptr;

#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "[%s] Stopped", m_config.name);
#endif
}

// ============================================================================
// Message Passing
// ============================================================================

bool Actor::send(const Message& msg, TickType_t timeout)
{
    if (m_queue == nullptr) {
        return false;
    }

    // Check queue utilization before sending
    UBaseType_t currentLength = uxQueueMessagesWaiting(m_queue);
    UBaseType_t queueCapacity = m_config.queueSize;
    
    // Warn if queue is > 80% full
    if (queueCapacity > 0) {
        uint8_t utilizationPercent = (currentLength * 100) / queueCapacity;
        if (utilizationPercent >= 80) {
#ifndef NATIVE_BUILD
            ESP_LOGW(TAG, "[%s] Queue utilization high: %d%% (%d/%d messages)",
                     m_config.name, utilizationPercent, currentLength, queueCapacity);
#endif
        }
    }

    BaseType_t result = xQueueSend(m_queue, &msg, timeout);
    
    // Log failure if queue was full
    if (result != pdTRUE) {
#ifndef NATIVE_BUILD
        ESP_LOGW(TAG, "[%s] Failed to send message (queue full or timeout): type=0x%02X, queue=%d/%d",
                 m_config.name, static_cast<uint8_t>(msg.type), currentLength, queueCapacity);
#endif
    }
    
    return (result == pdTRUE);
}

bool Actor::sendFromISR(const Message& msg)
{
    if (m_queue == nullptr) {
        return false;
    }

    BaseType_t higherPriorityTaskWoken = pdFALSE;
    BaseType_t result = xQueueSendFromISR(m_queue, &msg, &higherPriorityTaskWoken);

    // Yield to higher priority task if needed
    if (higherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }

    return (result == pdTRUE);
}

UBaseType_t Actor::getQueueLength() const
{
    if (m_queue == nullptr) {
        return 0;
    }
    return uxQueueMessagesWaiting(m_queue);
}

uint8_t Actor::getQueueUtilization() const
{
    if (m_queue == nullptr || m_config.queueSize == 0) {
        return 0;
    }
    UBaseType_t currentLength = uxQueueMessagesWaiting(m_queue);
    return (currentLength * 100) / m_config.queueSize;
}

// ============================================================================
// Diagnostics
// ============================================================================

UBaseType_t Actor::getStackHighWaterMark() const
{
    if (m_taskHandle == nullptr) {
        return 0;
    }
    return uxTaskGetStackHighWaterMark(m_taskHandle);
}

// ============================================================================
// Utilities
// ============================================================================

uint32_t Actor::getTickCount() const
{
    return xTaskGetTickCount();
}

void Actor::sleep(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

// ============================================================================
// Private Implementation
// ============================================================================

void Actor::taskFunction(void* param)
{
    Actor* actor = static_cast<Actor*>(param);
    if (actor != nullptr) {
        actor->run();
    }

    // Task function should never return, but if it does, delete self
    vTaskDelete(nullptr);
}

void Actor::run()
{
    m_running = true;

#ifndef NATIVE_BUILD
    ESP_LOGD(TAG, "[%s] Task started, calling onStart()", m_config.name);
#endif

    // Call derived class initialization
    onStart();

#ifndef NATIVE_BUILD
    ESP_LOGI(TAG, "[%s] onStart() complete, entering main loop (tickInterval=%lu)", 
             m_config.name, (unsigned long)m_config.tickInterval);
#endif

    // Main message loop
    while (!m_shutdownRequested) {
        Message msg;

        // Queue saturation prevention: drain multiple messages when queue is getting full
        // This prevents command rejection when rapid inputs (e.g. encoder rotation, zone updates)
        // exceed the single-message-per-tick processing rate.
        uint8_t queueUtil = getQueueUtilization();
        const uint8_t DRAIN_THRESHOLD = 50;  // Start draining at 50% full
        const uint8_t MAX_MESSAGES_PER_TICK = 4;  // Tightened from 8 to shorten each drain cycle

        if (queueUtil > DRAIN_THRESHOLD) {
            // Queue is getting full - drain a bounded number of messages with
            // non-blocking receives. After each dispatch we feed the task
            // watchdog and yield so lower-priority tasks on the same core
            // (notably loopTask running SerialCLI) can still run. Without
            // these two calls a sustained burst of SET_EFFECT messages would
            // wedge the core for the entire drain — serial polling, WiFi
            // heartbeat and loop-level work all stop, producing a silent
            // lock-up with LEDs frozen on the last rendered frame.
            uint8_t messagesProcessed = 0;
            while (messagesProcessed < MAX_MESSAGES_PER_TICK && !m_shutdownRequested) {
                BaseType_t received = xQueueReceive(m_queue, &msg, 0);  // Non-blocking
                if (received != pdTRUE) {
                    break;  // Queue empty
                }

                // Handle shutdown message specially
                if (msg.type == MessageType::SHUTDOWN) {
                    m_shutdownRequested = true;
                    break;
                }

                // Dispatch to derived class handler
                m_messageCount++;
                onMessage(msg);
                messagesProcessed++;

#ifndef NATIVE_BUILD
                // Feed wdt if this task is subscribed (silent no-op otherwise)
                // and yield so equal/lower-priority tasks on this core can run.
                esp_task_wdt_reset();
                taskYIELD();
#endif
            }

            // Force one onTick() after a drain cycle: for the Renderer this
            // pushes a frame and feeds its own wdt from inside onTick; for
            // self-clocked actors it preserves their periodic work. Without
            // this, a queue that stays above DRAIN_THRESHOLD would starve
            // onTick() indefinitely — LEDs freeze even though messages are
            // being consumed.
            onTick();
            continue;
        }

        // Normal operation: wait for message with timeout based on tick interval
        // 
        // tickInterval semantics:
        //   > 0: Periodic tick mode - wait up to tickInterval, call onTick on timeout
        //   = 0: Self-clocked mode - poll queue non-blocking, always call onTick
        //        (Actor's onTick is expected to block internally, e.g., on I2S read)
        //   portMAX_DELAY: No tick mode - wait forever for messages only
        //
        TickType_t waitTime;
        if (m_config.tickInterval == 0) {
            // Self-clocked mode: poll queue non-blocking, then call onTick
            // onTick() is expected to block (e.g., AudioActor blocks on I2S read)
            waitTime = 0;
        } else if (m_config.tickInterval > 0) {
            waitTime = m_config.tickInterval;
        } else {
            waitTime = portMAX_DELAY; // Wait forever if no tick needed
        }

        // Wait for a message (or poll if waitTime=0)
        BaseType_t received = xQueueReceive(m_queue, &msg, waitTime);

        if (received == pdTRUE) {
            // Handle shutdown message specially
            if (msg.type == MessageType::SHUTDOWN) {
                m_shutdownRequested = true;
                break;
            }

            // Dispatch to derived class handler
            m_messageCount++;
            onMessage(msg);
        } else {
            // Timeout or no message (waitTime=0) - call onTick
            // For tickInterval=0 (self-clocked), always tick
            // For tickInterval>0 (periodic), tick on timeout
            if (m_config.tickInterval == 0 || m_config.tickInterval > 0) {
                onTick();
            }
        }

        // Stack overflow detection (development aid)
#ifndef NATIVE_BUILD
#if CONFIG_FREERTOS_ASSERT_FAIL_ABORT
        UBaseType_t highWater = getStackHighWaterMark();
        if (highWater < 100) { // Less than 400 bytes remaining
            ESP_LOGW(TAG, "[%s] Stack low! High water mark: %d words",
                     m_config.name, highWater);
        }
#endif
#endif
    }

#ifndef NATIVE_BUILD
    ESP_LOGD(TAG, "[%s] Task stopping, calling onStop()", m_config.name);
#endif

    // Call derived class cleanup
    onStop();

    m_running = false;
}

} // namespace actors
} // namespace lightwaveos
