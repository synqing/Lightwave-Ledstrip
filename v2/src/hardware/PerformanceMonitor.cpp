/**
 * @file PerformanceMonitor.cpp
 * @brief Implementation of PerformanceMonitor for LightwaveOS v2
 *
 * @copyright 2024 LightwaveOS Project
 */

#include "PerformanceMonitor.h"
#include <Arduino.h>
#include <esp_timer.h>

// ESP32 heap functions
extern "C" {
    size_t esp_get_free_heap_size(void);
    size_t esp_get_minimum_free_heap_size(void);
    size_t heap_caps_get_largest_free_block(uint32_t caps);
}

// MALLOC_CAP_DEFAULT for heap_caps_get_largest_free_block
#ifndef MALLOC_CAP_DEFAULT
#define MALLOC_CAP_DEFAULT (1 << 0)
#endif

namespace lightwaveos {
namespace hardware {

void PerformanceMonitor::begin(uint16_t targetFPS) {
    m_targetFrameTime = 1000000 / targetFPS;  // Convert to microseconds
    m_memoryMetrics.minFreeHeap = esp_get_free_heap_size();
    m_frameCount = 0;
    m_droppedFrames = 0;
    m_totalCPUTime = 0;
    m_activeCPUTime = 0;
    m_cpuUsagePercent = 0.0f;
    m_historyIndex = 0;

    // Reset all timing metrics
    m_currentFrame = TimingMetrics();
    m_avgMetrics = TimingMetrics();
    m_peakMetrics = TimingMetrics();

    Serial.println(F("[PERF] Performance Monitor initialized"));
}

void PerformanceMonitor::startFrame() {
    m_frameStartTime = esp_timer_get_time();
    m_frameCount++;

    // Reset current frame metrics
    m_currentFrame = TimingMetrics();
}

void PerformanceMonitor::startSection() {
    m_sectionStartTime = esp_timer_get_time();
}

void PerformanceMonitor::endEffectProcessing() {
    m_currentFrame.effectProcessing = static_cast<uint32_t>(
        esp_timer_get_time() - m_sectionStartTime
    );
}

void PerformanceMonitor::endFastLEDShow() {
    m_currentFrame.fastLEDShow = static_cast<uint32_t>(
        esp_timer_get_time() - m_sectionStartTime
    );
}

void PerformanceMonitor::endSerialProcessing() {
    m_currentFrame.serialProcessing = static_cast<uint32_t>(
        esp_timer_get_time() - m_sectionStartTime
    );
}

void PerformanceMonitor::endNetworkProcessing() {
    m_currentFrame.networkProcessing = static_cast<uint32_t>(
        esp_timer_get_time() - m_sectionStartTime
    );
}

void PerformanceMonitor::endFrame() {
    int64_t now = esp_timer_get_time();
    m_currentFrame.totalFrame = static_cast<uint32_t>(now - m_frameStartTime);

    // Calculate idle time
    uint32_t activeTime = m_currentFrame.effectProcessing +
                          m_currentFrame.fastLEDShow +
                          m_currentFrame.serialProcessing +
                          m_currentFrame.networkProcessing;
    m_currentFrame.idle = (m_currentFrame.totalFrame > activeTime)
                            ? (m_currentFrame.totalFrame - activeTime)
                            : 0;

    // Check for dropped frames (>1.5x target frame time)
    if (m_currentFrame.totalFrame > (m_targetFrameTime * 3 / 2)) {
        m_droppedFrames++;
    }

    // Update rolling averages (exponential moving average)
    m_avgMetrics.effectProcessing = static_cast<uint32_t>(
        m_avgMetrics.effectProcessing * (1.0f - EMA_ALPHA) +
        m_currentFrame.effectProcessing * EMA_ALPHA
    );
    m_avgMetrics.fastLEDShow = static_cast<uint32_t>(
        m_avgMetrics.fastLEDShow * (1.0f - EMA_ALPHA) +
        m_currentFrame.fastLEDShow * EMA_ALPHA
    );
    m_avgMetrics.serialProcessing = static_cast<uint32_t>(
        m_avgMetrics.serialProcessing * (1.0f - EMA_ALPHA) +
        m_currentFrame.serialProcessing * EMA_ALPHA
    );
    m_avgMetrics.networkProcessing = static_cast<uint32_t>(
        m_avgMetrics.networkProcessing * (1.0f - EMA_ALPHA) +
        m_currentFrame.networkProcessing * EMA_ALPHA
    );
    m_avgMetrics.totalFrame = static_cast<uint32_t>(
        m_avgMetrics.totalFrame * (1.0f - EMA_ALPHA) +
        m_currentFrame.totalFrame * EMA_ALPHA
    );
    m_avgMetrics.idle = static_cast<uint32_t>(
        m_avgMetrics.idle * (1.0f - EMA_ALPHA) +
        m_currentFrame.idle * EMA_ALPHA
    );

    // Update peak metrics
    if (m_currentFrame.effectProcessing > m_peakMetrics.effectProcessing) {
        m_peakMetrics.effectProcessing = m_currentFrame.effectProcessing;
    }
    if (m_currentFrame.fastLEDShow > m_peakMetrics.fastLEDShow) {
        m_peakMetrics.fastLEDShow = m_currentFrame.fastLEDShow;
    }
    if (m_currentFrame.serialProcessing > m_peakMetrics.serialProcessing) {
        m_peakMetrics.serialProcessing = m_currentFrame.serialProcessing;
    }
    if (m_currentFrame.networkProcessing > m_peakMetrics.networkProcessing) {
        m_peakMetrics.networkProcessing = m_currentFrame.networkProcessing;
    }
    if (m_currentFrame.totalFrame > m_peakMetrics.totalFrame) {
        m_peakMetrics.totalFrame = m_currentFrame.totalFrame;
    }

    // Calculate CPU usage
    m_totalCPUTime += m_currentFrame.totalFrame;
    m_activeCPUTime += activeTime;
    if (m_totalCPUTime > 0) {
        m_cpuUsagePercent = static_cast<float>(m_activeCPUTime) /
                            static_cast<float>(m_totalCPUTime) * 100.0f;
    }

    // Update memory metrics
    updateMemoryMetrics();

    // Update history (every 10 frames)
    if (m_frameCount % 10 == 0) {
        updateHistory();
    }
}

void PerformanceMonitor::updateMemoryMetrics() {
    m_memoryMetrics.freeHeap = esp_get_free_heap_size();

    if (m_memoryMetrics.freeHeap < m_memoryMetrics.minFreeHeap) {
        m_memoryMetrics.minFreeHeap = m_memoryMetrics.freeHeap;
    }

    m_memoryMetrics.maxAllocBlock = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);

    // Calculate fragmentation: 100 - (largest_block / total_free * 100)
    if (m_memoryMetrics.freeHeap > 0) {
        m_memoryMetrics.fragmentationPercent = static_cast<uint8_t>(
            100 - (m_memoryMetrics.maxAllocBlock * 100 / m_memoryMetrics.freeHeap)
        );
    } else {
        m_memoryMetrics.fragmentationPercent = 0;
    }
}

void PerformanceMonitor::updateHistory() {
    // Store FPS in history (clamped to 0-255)
    uint32_t fps = (m_avgMetrics.totalFrame > 0)
                     ? (1000000 / m_avgMetrics.totalFrame)
                     : 0;
    m_fpsHistory[m_historyIndex] = (fps > 255) ? 255 : static_cast<uint8_t>(fps);

    // Store CPU usage in history (clamped to 0-100)
    m_cpuHistory[m_historyIndex] = (m_cpuUsagePercent > 100.0f)
                                      ? 100
                                      : static_cast<uint8_t>(m_cpuUsagePercent);

    m_historyIndex = (m_historyIndex + 1) % HISTORY_SIZE;
}

// ========== Getters for REST API ==========

float PerformanceMonitor::getFPS() const {
    return (m_avgMetrics.totalFrame > 0)
             ? (1000000.0f / static_cast<float>(m_avgMetrics.totalFrame))
             : 0.0f;
}

float PerformanceMonitor::getCPUPercent() const {
    return m_cpuUsagePercent;
}

size_t PerformanceMonitor::getHeapFree() const {
    return m_memoryMetrics.freeHeap;
}

uint8_t PerformanceMonitor::getHeapFragmentation() const {
    return m_memoryMetrics.fragmentationPercent;
}

uint32_t PerformanceMonitor::getEffectTimeUs() const {
    return m_avgMetrics.effectProcessing;
}

uint32_t PerformanceMonitor::getShowTimeUs() const {
    return m_avgMetrics.fastLEDShow;
}

uint32_t PerformanceMonitor::getTotalFrameTimeUs() const {
    return m_avgMetrics.totalFrame;
}

uint32_t PerformanceMonitor::getDroppedFrames() const {
    return m_droppedFrames;
}

uint32_t PerformanceMonitor::getTotalFrames() const {
    return m_frameCount;
}

size_t PerformanceMonitor::getMinFreeHeap() const {
    return m_memoryMetrics.minFreeHeap;
}

uint32_t PerformanceMonitor::getTargetFrameTimeUs() const {
    return m_targetFrameTime;
}

PerformanceStats PerformanceMonitor::getStats() const {
    PerformanceStats stats;
    stats.fps = getFPS();
    stats.cpuPercent = m_cpuUsagePercent;
    stats.effectTimeUs = m_avgMetrics.effectProcessing;
    stats.showTimeUs = m_avgMetrics.fastLEDShow;
    stats.totalFrameTimeUs = m_avgMetrics.totalFrame;
    stats.heapFree = m_memoryMetrics.freeHeap;
    stats.heapFragmentation = m_memoryMetrics.fragmentationPercent;
    stats.droppedFrames = m_droppedFrames;
    stats.totalFrames = m_frameCount;
    return stats;
}

// ========== Fragmentation Alerts ==========

bool PerformanceMonitor::isFragmentationWarning() const {
    return m_memoryMetrics.fragmentationPercent >= FRAGMENTATION_WARNING_THRESHOLD;
}

bool PerformanceMonitor::isFragmentationCritical() const {
    return m_memoryMetrics.fragmentationPercent >= FRAGMENTATION_CRITICAL_THRESHOLD;
}

// ========== Timing Breakdown ==========

void PerformanceMonitor::getTimingPercentages(float& effectPct, float& ledPct,
                                               float& serialPct, float& idlePct) const {
    if (m_avgMetrics.totalFrame == 0) {
        effectPct = ledPct = serialPct = idlePct = 0.0f;
        return;
    }

    float total = static_cast<float>(m_avgMetrics.totalFrame);
    effectPct = static_cast<float>(m_avgMetrics.effectProcessing) / total * 100.0f;
    ledPct = static_cast<float>(m_avgMetrics.fastLEDShow) / total * 100.0f;
    serialPct = static_cast<float>(m_avgMetrics.serialProcessing) / total * 100.0f;
    idlePct = static_cast<float>(m_avgMetrics.idle) / total * 100.0f;
}

// ========== Serial Output ==========

void PerformanceMonitor::printStatus() const {
    Serial.print(F("[PERF] FPS: "));
    Serial.print(getFPS(), 1);
    Serial.print(F(" | CPU: "));
    Serial.print(m_cpuUsagePercent, 1);
    Serial.print(F("% | Effect: "));
    Serial.print(m_avgMetrics.effectProcessing);
    Serial.print(F("us | LED: "));
    Serial.print(m_avgMetrics.fastLEDShow);
    Serial.print(F("us | Heap: "));
    Serial.print(m_memoryMetrics.freeHeap);
    Serial.print(F(" | Frag: "));
    Serial.print(m_memoryMetrics.fragmentationPercent);
    Serial.print(F("%"));

    // Alert on high fragmentation
    if (isFragmentationCritical()) {
        Serial.print(F(" [CRITICAL]"));
    } else if (isFragmentationWarning()) {
        Serial.print(F(" [WARN]"));
    }
    Serial.println();
}

void PerformanceMonitor::printDetailedReport() const {
    Serial.println(F("\n=== PERFORMANCE REPORT ==="));

    // Frame timing breakdown
    Serial.println(F("Frame Timing (avg/peak us):"));
    Serial.print(F("  Effect Processing: "));
    Serial.print(m_avgMetrics.effectProcessing);
    Serial.print(F(" / "));
    Serial.println(m_peakMetrics.effectProcessing);

    Serial.print(F("  FastLED.show():    "));
    Serial.print(m_avgMetrics.fastLEDShow);
    Serial.print(F(" / "));
    Serial.println(m_peakMetrics.fastLEDShow);

    Serial.print(F("  Serial Processing: "));
    Serial.print(m_avgMetrics.serialProcessing);
    Serial.print(F(" / "));
    Serial.println(m_peakMetrics.serialProcessing);

    Serial.print(F("  Network Processing:"));
    Serial.print(m_avgMetrics.networkProcessing);
    Serial.print(F(" / "));
    Serial.println(m_peakMetrics.networkProcessing);

    Serial.print(F("  Total Frame Time:  "));
    Serial.print(m_avgMetrics.totalFrame);
    Serial.print(F(" / "));
    Serial.println(m_peakMetrics.totalFrame);

    Serial.print(F("  Idle Time:         "));
    Serial.print(m_avgMetrics.idle);
    Serial.println(F(" us"));

    // Performance metrics
    Serial.println(F("\nPerformance Metrics:"));
    Serial.print(F("  Current FPS:       "));
    Serial.println(getFPS(), 1);

    Serial.print(F("  Target FPS:        "));
    Serial.println(1000000.0f / static_cast<float>(m_targetFrameTime), 1);

    Serial.print(F("  CPU Usage:         "));
    Serial.print(m_cpuUsagePercent, 1);
    Serial.println(F("%"));

    Serial.print(F("  Dropped Frames:    "));
    Serial.print(m_droppedFrames);
    if (m_frameCount > 0) {
        Serial.print(F(" ("));
        Serial.print(static_cast<float>(m_droppedFrames) /
                     static_cast<float>(m_frameCount) * 100.0f, 2);
        Serial.println(F("%)"));
    } else {
        Serial.println();
    }

    Serial.print(F("  Total Frames:      "));
    Serial.println(m_frameCount);

    // Memory metrics
    Serial.println(F("\nMemory Metrics:"));
    Serial.print(F("  Free Heap:         "));
    Serial.print(m_memoryMetrics.freeHeap);
    Serial.println(F(" bytes"));

    Serial.print(F("  Min Free Heap:     "));
    Serial.print(m_memoryMetrics.minFreeHeap);
    Serial.println(F(" bytes"));

    Serial.print(F("  Max Alloc Block:   "));
    Serial.print(m_memoryMetrics.maxAllocBlock);
    Serial.println(F(" bytes"));

    Serial.print(F("  Fragmentation:     "));
    Serial.print(m_memoryMetrics.fragmentationPercent);
    Serial.print(F("%"));
    if (isFragmentationCritical()) {
        Serial.println(F(" [CRITICAL]"));
    } else if (isFragmentationWarning()) {
        Serial.println(F(" [WARNING]"));
    } else {
        Serial.println(F(" [OK]"));
    }

    Serial.println(F("========================\n"));
}

void PerformanceMonitor::drawPerformanceGraph() const {
    Serial.println(F("\nFPS History (last 60 samples):"));

    // Find max value for scaling
    uint8_t maxFPS = 0;
    for (uint8_t i = 0; i < HISTORY_SIZE; i++) {
        if (m_fpsHistory[i] > maxFPS) {
            maxFPS = m_fpsHistory[i];
        }
    }

    if (maxFPS == 0) {
        Serial.println(F("  (No data yet)"));
        return;
    }

    // Draw graph (10 rows)
    for (int8_t row = 9; row >= 0; row--) {
        Serial.print(F("|"));
        for (uint8_t col = 0; col < HISTORY_SIZE; col++) {
            uint8_t scaledValue = static_cast<uint8_t>(
                static_cast<uint32_t>(m_fpsHistory[col]) * 10 / maxFPS
            );
            Serial.print(scaledValue > row ? '*' : ' ');
        }
        Serial.println(F("|"));
    }

    Serial.print(F("+"));
    for (uint8_t i = 0; i < HISTORY_SIZE; i++) {
        Serial.print(F("-"));
    }
    Serial.println(F("+"));

    Serial.print(F("0 FPS                                                    "));
    Serial.print(maxFPS);
    Serial.println(F(" FPS"));
}

// ========== Control ==========

void PerformanceMonitor::resetPeaks() {
    m_peakMetrics = TimingMetrics();
    m_droppedFrames = 0;
    m_memoryMetrics.minFreeHeap = esp_get_free_heap_size();
    m_totalCPUTime = 0;
    m_activeCPUTime = 0;
    Serial.println(F("[PERF] Peak metrics reset"));
}

} // namespace hardware
} // namespace lightwaveos
