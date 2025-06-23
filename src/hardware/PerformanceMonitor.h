#ifndef PERFORMANCE_MONITOR_H
#define PERFORMANCE_MONITOR_H

#include <Arduino.h>
#include <esp_timer.h>
#include "../config/hardware_config.h"
#include "../config/features.h"

// Performance monitoring and profiling system
class PerformanceMonitor {
private:
  // Timing measurements in microseconds
  struct TimingMetrics {
    uint32_t effectProcessing = 0;
    uint32_t fastLEDShow = 0;
    uint32_t serialProcessing = 0;
    uint32_t totalFrame = 0;
    uint32_t idle = 0;
  };
  
  // Current frame metrics
  TimingMetrics currentFrame;
  
  // Averaged metrics (rolling average)
  TimingMetrics avgMetrics;
  
  // Peak metrics
  TimingMetrics peakMetrics;
  
  // Frame statistics
  uint32_t frameCount = 0;
  uint32_t droppedFrames = 0;
  uint32_t targetFrameTime = 8333; // 120 FPS default (microseconds)
  
  // Timing helpers
  uint64_t frameStartTime = 0;
  uint64_t sectionStartTime = 0;
  
  // CPU usage calculation
  uint32_t totalCPUTime = 0;
  uint32_t activeCPUTime = 0;
  float cpuUsagePercent = 0.0f;
  
  // Memory tracking
  size_t lastFreeHeap = 0;
  size_t minFreeHeap = 0xFFFFFFFF;
  size_t heapFragmentation = 0;
  
  // FastLED specific metrics
  uint32_t pixelsPerSecond = 0;
  float dataRateMbps = 0.0f;
  
  // History for graphs (last 60 samples)
  static const uint8_t HISTORY_SIZE = 60;
  uint8_t fpsHistory[HISTORY_SIZE] = {0};
  uint8_t cpuHistory[HISTORY_SIZE] = {0};
  uint8_t historyIndex = 0;
  
public:
  
  void begin(uint16_t targetFPS = 120) {
    targetFrameTime = 1000000 / targetFPS; // Convert to microseconds
    minFreeHeap = ESP.getFreeHeap();
    Serial.println(F("[PERF] Performance Monitor initialized"));
  }
  
  // Start timing a new frame
  void startFrame() {
    frameStartTime = esp_timer_get_time();
    frameCount++;
    
    // Reset current frame metrics
    currentFrame = TimingMetrics();
  }
  
  // Start timing a section
  void startSection() {
    sectionStartTime = esp_timer_get_time();
  }
  
  // End timing for effect processing
  void endEffectProcessing() {
    currentFrame.effectProcessing = esp_timer_get_time() - sectionStartTime;
  }
  
  // End timing for FastLED.show()
  void endFastLEDShow() {
    currentFrame.fastLEDShow = esp_timer_get_time() - sectionStartTime;
  }
  
  // End timing for serial processing
  void endSerialProcessing() {
    currentFrame.serialProcessing = esp_timer_get_time() - sectionStartTime;
  }
  
  // End frame and calculate all metrics
  void endFrame() {
    uint64_t now = esp_timer_get_time();
    currentFrame.totalFrame = now - frameStartTime;
    
    // Calculate idle time
    uint32_t activeTime = currentFrame.effectProcessing + 
                         currentFrame.fastLEDShow + 
                         currentFrame.serialProcessing;
    currentFrame.idle = currentFrame.totalFrame - activeTime;
    
    // Check for dropped frames
    if (currentFrame.totalFrame > targetFrameTime * 1.5) {
      droppedFrames++;
    }
    
    // Update rolling averages (exponential moving average)
    const float alpha = 0.1f; // Smoothing factor
    avgMetrics.effectProcessing = avgMetrics.effectProcessing * (1 - alpha) + currentFrame.effectProcessing * alpha;
    avgMetrics.fastLEDShow = avgMetrics.fastLEDShow * (1 - alpha) + currentFrame.fastLEDShow * alpha;
    avgMetrics.serialProcessing = avgMetrics.serialProcessing * (1 - alpha) + currentFrame.serialProcessing * alpha;
    avgMetrics.totalFrame = avgMetrics.totalFrame * (1 - alpha) + currentFrame.totalFrame * alpha;
    avgMetrics.idle = avgMetrics.idle * (1 - alpha) + currentFrame.idle * alpha;
    
    // Update peak metrics
    peakMetrics.effectProcessing = max(peakMetrics.effectProcessing, currentFrame.effectProcessing);
    peakMetrics.fastLEDShow = max(peakMetrics.fastLEDShow, currentFrame.fastLEDShow);
    peakMetrics.serialProcessing = max(peakMetrics.serialProcessing, currentFrame.serialProcessing);
    peakMetrics.totalFrame = max(peakMetrics.totalFrame, currentFrame.totalFrame);
    
    // Calculate CPU usage
    totalCPUTime += currentFrame.totalFrame;
    activeCPUTime += activeTime;
    cpuUsagePercent = (float)activeCPUTime / totalCPUTime * 100.0f;
    
    // Update memory metrics
    size_t currentHeap = ESP.getFreeHeap();
    if (currentHeap < minFreeHeap) {
      minFreeHeap = currentHeap;
    }
    size_t maxBlock = ESP.getMaxAllocHeap();
    heapFragmentation = 100 - (maxBlock * 100 / currentHeap);
    
    // Calculate FastLED data rate (for WS2812: 30us per LED * number of LEDs)
    // Each LED needs 24 bits at 800kHz = 30 microseconds
    extern uint16_t fps;
    pixelsPerSecond = HardwareConfig::NUM_LEDS * fps;
    dataRateMbps = (pixelsPerSecond * 24) / 1000000.0f; // 24 bits per pixel
    
    // Update history
    if (frameCount % 10 == 0) { // Update history every 10 frames
      fpsHistory[historyIndex] = constrain(1000000 / avgMetrics.totalFrame, 0, 255);
      cpuHistory[historyIndex] = constrain(cpuUsagePercent, 0, 100);
      historyIndex = (historyIndex + 1) % HISTORY_SIZE;
    }
  }
  
  // Get current FPS
  float getCurrentFPS() const {
    return avgMetrics.totalFrame > 0 ? 1000000.0f / avgMetrics.totalFrame : 0.0f;
  }
  
  // Get CPU usage percentage
  float getCPUUsage() const {
    return cpuUsagePercent;
  }
  
  // Print detailed performance report
  void printDetailedReport() {
    Serial.println(F("\n=== PERFORMANCE REPORT ==="));
    
    // Frame timing breakdown
    Serial.println(F("Frame Timing (avg/peak μs):"));
    Serial.print(F("  Effect Processing: "));
    Serial.print(avgMetrics.effectProcessing);
    Serial.print(F(" / "));
    Serial.println(peakMetrics.effectProcessing);
    
    Serial.print(F("  FastLED.show():    "));
    Serial.print(avgMetrics.fastLEDShow);
    Serial.print(F(" / "));
    Serial.println(peakMetrics.fastLEDShow);
    
    Serial.print(F("  Serial Processing: "));
    Serial.print(avgMetrics.serialProcessing);
    Serial.print(F(" / "));
    Serial.println(peakMetrics.serialProcessing);
    
    Serial.print(F("  Total Frame Time:  "));
    Serial.print(avgMetrics.totalFrame);
    Serial.print(F(" / "));
    Serial.println(peakMetrics.totalFrame);
    
    Serial.print(F("  Idle Time:         "));
    Serial.print(avgMetrics.idle);
    Serial.println(F(" μs"));
    
    // Performance metrics
    Serial.println(F("\nPerformance Metrics:"));
    Serial.print(F("  Current FPS:       "));
    Serial.println(getCurrentFPS(), 1);
    
    Serial.print(F("  CPU Usage:         "));
    Serial.print(cpuUsagePercent, 1);
    Serial.println(F("%"));
    
    Serial.print(F("  Dropped Frames:    "));
    Serial.print(droppedFrames);
    Serial.print(F(" ("));
    Serial.print((float)droppedFrames / frameCount * 100, 1);
    Serial.println(F("%)"));
    
    // FastLED metrics
    Serial.println(F("\nFastLED Metrics:"));
    Serial.print(F("  Pixels/Second:     "));
    Serial.println(pixelsPerSecond);
    
    Serial.print(F("  Data Rate:         "));
    Serial.print(dataRateMbps, 2);
    Serial.println(F(" Mbps"));
    
    Serial.print(F("  Time per LED:      "));
    Serial.print((float)avgMetrics.fastLEDShow / HardwareConfig::NUM_LEDS, 1);
    Serial.println(F(" μs"));
    
    // Memory metrics
    Serial.println(F("\nMemory Metrics:"));
    Serial.print(F("  Free Heap:         "));
    Serial.print(ESP.getFreeHeap());
    Serial.println(F(" bytes"));
    
    Serial.print(F("  Min Free Heap:     "));
    Serial.print(minFreeHeap);
    Serial.println(F(" bytes"));
    
    Serial.print(F("  Fragmentation:     "));
    Serial.print(heapFragmentation);
    Serial.println(F("%"));
    
    Serial.print(F("  Total Frames:      "));
    Serial.println(frameCount);
    
    Serial.println(F("========================\n"));
  }
  
  // Print compact performance line
  void printCompactStatus() {
    Serial.print(F("[PERF] FPS: "));
    Serial.print(getCurrentFPS(), 1);
    Serial.print(F(" | CPU: "));
    Serial.print(cpuUsagePercent, 1);
    Serial.print(F("% | Effect: "));
    Serial.print(avgMetrics.effectProcessing);
    Serial.print(F("μs | LED: "));
    Serial.print(avgMetrics.fastLEDShow);
    Serial.print(F("μs | Heap: "));
    Serial.print(ESP.getFreeHeap());
    Serial.println();
  }
  
  // Get timing breakdown percentages
  void getTimingPercentages(float& effectPct, float& ledPct, float& serialPct, float& idlePct) {
    if (avgMetrics.totalFrame == 0) return;
    
    effectPct = (float)avgMetrics.effectProcessing / avgMetrics.totalFrame * 100.0f;
    ledPct = (float)avgMetrics.fastLEDShow / avgMetrics.totalFrame * 100.0f;
    serialPct = (float)avgMetrics.serialProcessing / avgMetrics.totalFrame * 100.0f;
    idlePct = (float)avgMetrics.idle / avgMetrics.totalFrame * 100.0f;
  }
  
  // Draw ASCII performance graph
  void drawPerformanceGraph() {
    Serial.println(F("\nFPS History (last 60 samples):"));
    
    // Find max value for scaling
    uint8_t maxFPS = 0;
    for (uint8_t i = 0; i < HISTORY_SIZE; i++) {
      if (fpsHistory[i] > maxFPS) maxFPS = fpsHistory[i];
    }
    
    // Draw graph (10 rows)
    for (int8_t row = 9; row >= 0; row--) {
      Serial.print(F("|"));
      for (uint8_t col = 0; col < HISTORY_SIZE; col++) {
        uint8_t scaledValue = map(fpsHistory[col], 0, maxFPS, 0, 10);
        Serial.print(scaledValue > row ? "*" : " ");
      }
      Serial.println(F("|"));
    }
    
    Serial.print(F("+"));
    for (uint8_t i = 0; i < HISTORY_SIZE; i++) Serial.print(F("-"));
    Serial.println(F("+"));
    
    Serial.print(F("0 FPS"));
    Serial.print(F("                                                    "));
    Serial.print(maxFPS);
    Serial.println(F(" FPS"));
  }
  
  // Reset peak metrics
  void resetPeaks() {
    peakMetrics = TimingMetrics();
    droppedFrames = 0;
    minFreeHeap = ESP.getFreeHeap();
  }
  
  // Getters for integration
  uint32_t getEffectTime() const { return avgMetrics.effectProcessing; }
  uint32_t getFastLEDTime() const { return avgMetrics.fastLEDShow; }
  uint32_t getFrameTime() const { return avgMetrics.totalFrame; }
  uint32_t getDroppedFrames() const { return droppedFrames; }
  size_t getMinFreeHeap() const { return minFreeHeap; }
};


#endif // PERFORMANCE_MONITOR_H