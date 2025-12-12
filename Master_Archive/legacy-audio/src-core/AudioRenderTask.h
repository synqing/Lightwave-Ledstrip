#ifndef AUDIO_RENDER_TASK_H
#define AUDIO_RENDER_TASK_H

#include <Arduino.h>
#include <FastLED.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "../config/hardware_config.h"

/**
 * AudioRenderTask - Consolidated 8ms deterministic task for audio and LED rendering
 * 
 * Features:
 * - Runs on Core 1 with high priority
 * - 125Hz update rate (8ms period) for smooth visuals
 * - Deterministic timing with drift compensation
 * - Separates time-critical rendering from non-critical housekeeping
 * - Proper synchronization between audio processing and LED updates
 */
class AudioRenderTask {
private:
    // Task configuration
    static constexpr uint32_t TASK_PERIOD_US = 8000;  // 8ms = 125Hz
    static constexpr uint32_t TASK_STACK_SIZE = 8192;
    static constexpr UBaseType_t TASK_PRIORITY = 2;  // High priority
    static constexpr BaseType_t TASK_CORE = 1;       // Core 1
    
    // Task handle
    TaskHandle_t taskHandle = nullptr;
    
    // Timing state
    uint32_t nextWakeTime = 0;
    uint32_t frameCount = 0;
    uint32_t missedFrames = 0;
    uint32_t maxJitter = 0;
    
    // Synchronization
    SemaphoreHandle_t renderMutex;
    volatile bool shouldExit = false;
    
    // Function pointers for callbacks
    typedef void (*AudioUpdateFunc)();
    typedef void (*RenderUpdateFunc)();
    typedef void (*EffectUpdateFunc)();
    
    AudioUpdateFunc audioCallback = nullptr;
    RenderUpdateFunc renderCallback = nullptr;
    EffectUpdateFunc effectCallback = nullptr;
    
    // Statistics
    struct TaskStats {
        uint32_t totalFrames = 0;
        uint32_t missedFrames = 0;
        uint32_t audioTime = 0;
        uint32_t renderTime = 0;
        uint32_t showTime = 0;
        uint32_t maxFrameTime = 0;
        uint32_t avgFrameTime = 0;
        uint32_t lastReportTime = 0;
    } stats;
    
public:
    AudioRenderTask() {
        renderMutex = xSemaphoreCreateMutex();
    }
    
    ~AudioRenderTask() {
        stop();
        if (renderMutex) {
            vSemaphoreDelete(renderMutex);
        }
    }
    
    /**
     * Start the audio/render task
     * @param audio Audio update callback (i2sMic.update, audioSynq.update, etc)
     * @param render Render update callback (effects, transitions, etc)
     * @param effect Effect update callback (current effect function)
     */
    bool start(AudioUpdateFunc audio, RenderUpdateFunc render, EffectUpdateFunc effect) {
        if (taskHandle != nullptr) {
            Serial.println("[AudioRenderTask] Task already running");
            return false;
        }
        
        audioCallback = audio;
        renderCallback = render;
        effectCallback = effect;
        shouldExit = false;
        
        // Create task on Core 1
        BaseType_t result = xTaskCreatePinnedToCore(
            taskWrapper,
            "AudioRender",
            TASK_STACK_SIZE,
            this,
            TASK_PRIORITY,
            &taskHandle,
            TASK_CORE
        );
        
        if (result == pdPASS) {
            Serial.println("[AudioRenderTask] Started 8ms deterministic task on Core 1");
            return true;
        } else {
            Serial.println("[AudioRenderTask] Failed to create task");
            return false;
        }
    }
    
    /**
     * Stop the audio/render task
     */
    void stop() {
        if (taskHandle != nullptr) {
            shouldExit = true;
            vTaskDelay(pdMS_TO_TICKS(20));  // Wait for task to exit
            taskHandle = nullptr;
            Serial.println("[AudioRenderTask] Stopped");
        }
    }
    
    /**
     * Update callbacks dynamically
     */
    void setAudioCallback(AudioUpdateFunc func) { audioCallback = func; }
    void setRenderCallback(RenderUpdateFunc func) { renderCallback = func; }
    void setEffectCallback(EffectUpdateFunc func) { effectCallback = func; }
    
    /**
     * Get task statistics
     */
    void getStats(uint32_t& frames, uint32_t& missed, uint32_t& avgTime, uint32_t& maxTime) {
        frames = stats.totalFrames;
        missed = stats.missedFrames;
        avgTime = stats.avgFrameTime;
        maxTime = stats.maxFrameTime;
    }
    
private:
    static void taskWrapper(void* parameter) {
        AudioRenderTask* task = static_cast<AudioRenderTask*>(parameter);
        task->taskLoop();
    }
    
    void taskLoop() {
        // Initialize timing
        nextWakeTime = esp_timer_get_time();
        uint32_t frameStartTime, audioStart, renderStart, showStart;
        uint32_t audioElapsed, renderElapsed, showElapsed, totalElapsed;
        
        while (!shouldExit) {
            frameStartTime = esp_timer_get_time();
            
            // === AUDIO UPDATE PHASE ===
            audioStart = esp_timer_get_time();
            if (audioCallback) {
                audioCallback();
            }
            audioElapsed = esp_timer_get_time() - audioStart;
            
            // === RENDER UPDATE PHASE ===
            renderStart = esp_timer_get_time();
            if (xSemaphoreTake(renderMutex, 0) == pdTRUE) {
                // Update effect
                if (effectCallback) {
                    effectCallback();
                }
                
                // Update transitions/rendering
                if (renderCallback) {
                    renderCallback();
                }
                
                xSemaphoreGive(renderMutex);
            }
            renderElapsed = esp_timer_get_time() - renderStart;
            
            // === LED SHOW PHASE ===
            showStart = esp_timer_get_time();
            FastLED.show();
            showElapsed = esp_timer_get_time() - showStart;
            
            // === TIMING AND STATISTICS ===
            totalElapsed = esp_timer_get_time() - frameStartTime;
            
            // Update statistics
            stats.totalFrames++;
            stats.audioTime = (stats.audioTime * 7 + audioElapsed) / 8;
            stats.renderTime = (stats.renderTime * 7 + renderElapsed) / 8;
            stats.showTime = (stats.showTime * 7 + showElapsed) / 8;
            stats.avgFrameTime = (stats.avgFrameTime * 7 + totalElapsed) / 8;
            if (totalElapsed > stats.maxFrameTime) {
                stats.maxFrameTime = totalElapsed;
            }
            
            // Calculate next wake time for deterministic timing
            nextWakeTime += TASK_PERIOD_US;
            int32_t timeUntilNext = nextWakeTime - esp_timer_get_time();
            
            // Check for timing violations
            if (timeUntilNext < 0) {
                stats.missedFrames++;
                // Reset timing if we're too far behind
                if (timeUntilNext < -TASK_PERIOD_US) {
                    nextWakeTime = esp_timer_get_time() + TASK_PERIOD_US;
                }
            } else {
                // Sleep until next frame
                vTaskDelay(pdMS_TO_TICKS(timeUntilNext / 1000));
            }
            
            // Periodic statistics report
            if (esp_timer_get_time() - stats.lastReportTime > 10000000) {  // Every 10 seconds
                stats.lastReportTime = esp_timer_get_time();
                printStatistics();
            }
        }
        
        vTaskDelete(NULL);
    }
    
    void printStatistics() {
        float fps = stats.totalFrames > 0 ? 1000000.0f / stats.avgFrameTime : 0;
        float audioMs = stats.audioTime / 1000.0f;
        float renderMs = stats.renderTime / 1000.0f;
        float showMs = stats.showTime / 1000.0f;
        float totalMs = stats.avgFrameTime / 1000.0f;
        
        Serial.printf("[AudioRenderTask] FPS: %.1f, Missed: %lu, Times(ms): Audio=%.2f Render=%.2f Show=%.2f Total=%.2f\n",
                     fps, stats.missedFrames, audioMs, renderMs, showMs, totalMs);
        
        // Reset max frame time
        stats.maxFrameTime = stats.avgFrameTime;
    }
};

// Global instance
extern AudioRenderTask audioRenderTask;

#endif // AUDIO_RENDER_TASK_H