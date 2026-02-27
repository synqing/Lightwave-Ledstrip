/**
 * FreeRTOS Mock Implementation
 *
 * Minimal FreeRTOS implementation for native unit tests.
 * Uses C++ STL containers for queue/mutex primitives.
 */

#ifdef NATIVE_BUILD

#include "freertos_mock.h"
#include <cstring>
#include <chrono>

namespace freertos_mock {
    static uint32_t currentMillis = 0;
    static auto startTime = std::chrono::steady_clock::now();
}

//==============================================================================
// Queue Implementation
//==============================================================================

QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t itemSize) {
    auto* queue = new freertos_mock::Queue();
    queue->itemSize = itemSize;
    queue->maxLength = length;
    return queue;
}

BaseType_t xQueueSend(QueueHandle_t queue, const void* item, TickType_t wait) {
    if (!queue || !item) {
        return pdFAIL;
    }

    std::lock_guard<std::mutex> lock(queue->mutex);

    if (queue->data.size() >= queue->maxLength) {
        return pdFAIL;  // Queue full (simplified - real FreeRTOS would wait)
    }

    // Copy item data into vector
    std::vector<uint8_t> itemData(queue->itemSize);
    std::memcpy(itemData.data(), item, queue->itemSize);
    queue->data.push(std::move(itemData));

    return pdPASS;
}

BaseType_t xQueueReceive(QueueHandle_t queue, void* buffer, TickType_t wait) {
    if (!queue || !buffer) {
        return pdFAIL;
    }

    std::lock_guard<std::mutex> lock(queue->mutex);

    if (queue->data.empty()) {
        return pdFAIL;  // Queue empty (simplified - real FreeRTOS would wait)
    }

    // Copy item from queue to buffer
    const auto& itemData = queue->data.front();
    std::memcpy(buffer, itemData.data(), queue->itemSize);
    queue->data.pop();

    return pdPASS;
}

UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue) {
    if (!queue) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(queue->mutex);
    return static_cast<UBaseType_t>(queue->data.size());
}

void vQueueDelete(QueueHandle_t queue) {
    if (queue) {
        delete queue;
    }
}

//==============================================================================
// Semaphore Implementation
//==============================================================================

SemaphoreHandle_t xSemaphoreCreateMutex() {
    auto* sem = new freertos_mock::Semaphore();
    sem->available = true;
    return sem;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t wait) {
    if (!sem) {
        return pdFAIL;
    }

    // Try to lock (simplified - no timeout handling)
    if (sem->mutex.try_lock()) {
        sem->available = false;
        return pdPASS;
    }

    return pdFAIL;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t sem) {
    if (!sem) {
        return pdFAIL;
    }

    sem->available = true;
    sem->mutex.unlock();
    return pdPASS;
}

void vSemaphoreDelete(SemaphoreHandle_t sem) {
    if (sem) {
        delete sem;
    }
}

//==============================================================================
// Task Functions (No-op in native tests)
//==============================================================================

BaseType_t xTaskCreatePinnedToCore(
    void (*taskFunction)(void*),
    const char* name,
    uint32_t stackSize,
    void* parameter,
    UBaseType_t priority,
    TaskHandle_t* handle,
    BaseType_t coreId
) {
    // No-op: We don't actually create tasks in native tests
    // Tests will call the task function directly if needed
    if (handle) {
        *handle = reinterpret_cast<TaskHandle_t>(0x1234);  // Dummy handle
    }
    return pdPASS;
}

void vTaskDelete(TaskHandle_t handle) {
    // No-op
}

UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t handle) {
    return 1024;  // Dummy value
}

void vTaskDelay(TickType_t ticks) {
    freertos_mock::currentMillis += ticks * portTICK_PERIOD_MS;
}

//==============================================================================
// Time Functions
//==============================================================================

uint32_t millis() {
    if (freertos_mock::currentMillis > 0) {
        // Using manual time control
        return freertos_mock::currentMillis;
    }

    // Using real time
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - freertos_mock::startTime
    );
    return static_cast<uint32_t>(elapsed.count());
}

void delay(uint32_t ms) {
    freertos_mock::currentMillis += ms;
}

//==============================================================================
// Mock Control Functions
//==============================================================================

namespace freertos_mock {

void reset() {
    currentMillis = 0;
    startTime = std::chrono::steady_clock::now();
}

uint32_t getMillis() {
    return currentMillis;
}

void setMillis(uint32_t ms) {
    currentMillis = ms;
}

void advanceTime(uint32_t ms) {
    currentMillis += ms;
}

} // namespace freertos_mock

#endif // NATIVE_BUILD
