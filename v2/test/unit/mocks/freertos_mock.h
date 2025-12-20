#pragma once

/**
 * FreeRTOS Mock for Native Unit Tests
 *
 * Provides minimal FreeRTOS API implementation for testing actor system
 * and message passing without requiring actual ESP32 hardware.
 *
 * Features:
 * - Queue implementation using std::queue
 * - Mutex/semaphore using std::mutex
 * - Deterministic task creation (no-op in native tests)
 * - Millisecond time tracking
 */

#ifdef NATIVE_BUILD

#include <cstdint>
#include <queue>
#include <mutex>
#include <memory>
#include <chrono>

namespace freertos_mock {

// Mock queue structure
struct Queue {
    std::queue<std::vector<uint8_t>> data;
    std::mutex mutex;
    size_t itemSize;
    size_t maxLength;
};

// Mock semaphore structure
struct Semaphore {
    std::mutex mutex;
    bool available;
};

} // namespace freertos_mock

// FreeRTOS Type Definitions
typedef void* TaskHandle_t;
typedef freertos_mock::Queue* QueueHandle_t;
typedef freertos_mock::Semaphore* SemaphoreHandle_t;
typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;

// FreeRTOS Constants
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS pdTRUE
#define pdFAIL 0
#define portMAX_DELAY 0xFFFFFFFF
#define portTICK_PERIOD_MS 1

// Queue Functions
QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t itemSize);
BaseType_t xQueueSend(QueueHandle_t queue, const void* item, TickType_t wait);
BaseType_t xQueueReceive(QueueHandle_t queue, void* buffer, TickType_t wait);
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue);
void vQueueDelete(QueueHandle_t queue);

// Semaphore Functions
SemaphoreHandle_t xSemaphoreCreateMutex();
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t wait);
BaseType_t xSemaphoreGive(SemaphoreHandle_t sem);
void vSemaphoreDelete(SemaphoreHandle_t sem);

// Task Functions (no-op in native tests)
BaseType_t xTaskCreatePinnedToCore(
    void (*taskFunction)(void*),
    const char* name,
    uint32_t stackSize,
    void* parameter,
    UBaseType_t priority,
    TaskHandle_t* handle,
    BaseType_t coreId
);
void vTaskDelete(TaskHandle_t handle);
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t handle);
void vTaskDelay(TickType_t ticks);

// Time Functions
uint32_t millis();
void delay(uint32_t ms);

// Mock Control Functions (for testing)
namespace freertos_mock {
    void reset();  // Reset all mock state
    uint32_t getMillis();
    void setMillis(uint32_t ms);
    void advanceTime(uint32_t ms);
}

#endif // NATIVE_BUILD
