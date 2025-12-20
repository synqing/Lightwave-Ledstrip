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
        m_config.stackSize,     // Stack size in words
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

    BaseType_t result = xQueueSend(m_queue, &msg, timeout);
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

    // Main message loop
    while (!m_shutdownRequested) {
        Message msg;

        // Calculate wait time based on tick interval
        TickType_t waitTime;
        if (m_config.tickInterval > 0) {
            waitTime = m_config.tickInterval;
        } else {
            waitTime = portMAX_DELAY; // Wait forever if no tick needed
        }

        // Wait for a message
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
            // Timeout - call onTick if configured
            if (m_config.tickInterval > 0) {
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
