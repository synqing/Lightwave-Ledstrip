// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#include "lvgl_bridge.h"

#include <esp_heap_caps.h>
#include <Arduino.h>
#include <esp_task_wdt.h>

namespace LVGLBridge {
static lv_display_t* gDisplay = nullptr;
static lv_indev_t* gTouchIndev = nullptr;
static uint16_t* gDrawBuf = nullptr;

static uint16_t s_screenWidth = 0;
static uint16_t s_screenHeight = 0;
static constexpr uint16_t kBufferLines = 64;

static void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map);
static void touch_read_cb(lv_indev_t* indev, lv_indev_data_t* data);
static uint32_t tick_cb();

bool init() {
  if (gDisplay) return true;

  s_screenWidth = M5.Display.width();
  s_screenHeight = M5.Display.height();
  
  // Safety check
  if (s_screenWidth == 0 || s_screenHeight == 0) {
      s_screenWidth = 1280;
      s_screenHeight = 720;
  }
  
  Serial.printf("[LVGL] Display size: %dx%d\n", s_screenWidth, s_screenHeight);
  
  // #region agent log (DISABLED)
  // Test a known color value to verify byte order
  // White in RGB565: 0xFFFF, Black: 0x0000, Red: 0xF800, Green: 0x07E0, Blue: 0x001F
  // Create a test pixel buffer with known colors
  // uint16_t testColors[4] = {0xFFFF, 0xF800, 0x07E0, 0x001F}; // White, Red, Green, Blue
  // #ifdef ENABLE_VERBOSE_DEBUG
  // Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H2\",\"location\":\"lvgl_bridge.cpp:19\",\"message\":\"LVGLBridge.init\",\"data\":{\"width\":%d,\"height\":%d,\"colorDepth\":16,\"colorSwap\":0,\"testColors\":[\"0x%04X\",\"0x%04X\",\"0x%04X\",\"0x%04X\"]},\"timestamp\":%lu}\n",
            // s_screenWidth, s_screenHeight,
            // testColors[0], testColors[1], testColors[2], testColors[3],
            // (unsigned long)millis());
  // #endif // ENABLE_VERBOSE_DEBUG
    // #endregion

  const size_t bufSizeBytes = (size_t)s_screenWidth * (size_t)kBufferLines * sizeof(uint16_t);
  gDrawBuf = static_cast<uint16_t*>(
      heap_caps_malloc(bufSizeBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!gDrawBuf) {
      Serial.println("[LVGL] Failed to allocate draw buffer");
      return false;
  }
  memset(gDrawBuf, 0, bufSizeBytes);

  lv_init();
  lv_tick_set_cb(tick_cb);

  gDisplay = lv_display_create(s_screenWidth, s_screenHeight);
  if (!gDisplay) return false;

  const uint32_t bufPxCnt = (uint32_t)s_screenWidth * (uint32_t)kBufferLines;
  lv_display_set_buffers(gDisplay, gDrawBuf, nullptr, bufPxCnt, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(gDisplay, flush_cb);

  gTouchIndev = lv_indev_create();
  if (!gTouchIndev) return false;
  lv_indev_set_type(gTouchIndev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(gTouchIndev, touch_read_cb);

  return true;
}

void update() {
  if (!gDisplay) return;
  lv_timer_handler();
}

lv_display_t* getDisplay() {
  return gDisplay;
}

lv_indev_t* getTouchDevice() {
  return gTouchIndev;
}

static void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
  const int32_t w = area->x2 - area->x1 + 1;
  const int32_t h = area->y2 - area->y1 + 1;

  // #region agent log - DISABLED by default (enable with ENABLE_VERBOSE_DEBUG) (DISABLED)
  // #ifdef ENABLE_VERBOSE_DEBUG
  // static uint32_t s_flushCount = 0;
  // static uint32_t s_lastLogTime = 0;
  // s_flushCount++;
  // uint32_t now = millis();
  // bool shouldLog = (s_flushCount <= 10) || (now - s_lastLogTime >= 2000);
  
  // if (shouldLog && w > 0 && h > 0) {
    // uint16_t* px16 = (uint16_t*)px_map;
    // uint16_t sample0 = px16[0];
    // uint16_t sample1 = (w * h > 1) ? px16[1] : 0;
    // uint16_t sample2 = (w * h > 2) ? px16[2] : 0;
    // uint16_t sampleMid = (w * h > 10) ? px16[w * h / 2] : 0;
    
    // Extract RGB components from RGB565 (assuming little-endian)
    // uint8_t r0 = (sample0 >> 11) & 0x1F;
    // uint8_t g0 = (sample0 >> 5) & 0x3F;
    // uint8_t b0 = sample0 & 0x1F;
    
    // Also check byte-swapped interpretation
    // uint16_t swapped0 = ((sample0 & 0xFF) << 8) | ((sample0 >> 8) & 0xFF);
    // uint8_t r0_swapped = (swapped0 >> 11) & 0x1F;
    // uint8_t g0_swapped = (swapped0 >> 5) & 0x3F;
    // uint8_t b0_swapped = swapped0 & 0x1F;
    
    // Check if this looks like a known color pattern
    // const char* colorHint = "unknown";
    // if (sample0 == 0xFFFF || sample0 == 0x00FF) colorHint = "white_candidate";
    // else if (sample0 == 0xF800 || sample0 == 0x00F8) colorHint = "red_candidate";
    // else if (sample0 == 0x07E0 || sample0 == 0xE007) colorHint = "green_candidate";
    // else if (sample0 == 0x001F || sample0 == 0x1F00) colorHint = "blue_candidate";
    
    // Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1\",\"location\":\"lvgl_bridge.cpp:78\",\"message\":\"flush_cb.entry\",\"data\":{\"area\":\"%ld,%ld,%ld,%ld\",\"size\":\"%ldx%ld\",\"sample0\":\"0x%04X\",\"sample0_rgb\":\"R:%d G:%d B:%d\",\"sample0_swapped\":\"0x%04X\",\"sample0_swapped_rgb\":\"R:%d G:%d B:%d\",\"sample1\":\"0x%04X\",\"sample2\":\"0x%04X\",\"sampleMid\":\"0x%04X\",\"colorHint\":\"%s\",\"flushCount\":%lu},\"timestamp\":%lu}\n",
              // (long)area->x1, (long)area->y1, (long)area->x2, (long)area->y2,
              // (long)w, (long)h,
              // sample0, r0, g0, b0,
              // swapped0, r0_swapped, g0_swapped, b0_swapped,
              // sample1, sample2, sampleMid,
              // colorHint,
              // (unsigned long)s_flushCount, (unsigned long)now);
    // s_lastLogTime = now;
  // }
  // #endif // ENABLE_VERBOSE_DEBUG
    // #endregion

  // IMPORTANT: flush_cb runs inside lv_timer_handler() on Arduino loopTask (CPU1).
  // Large SPI transfers can take long enough to trip the task watchdog if we never
  // feed it during the transfer.
  esp_task_wdt_reset();
  const uint32_t t0 = millis();
  M5.Display.startWrite();
  M5.Display.pushImage(area->x1, area->y1, w, h, (uint16_t*)px_map);
  M5.Display.endWrite();
  const uint32_t dt = millis() - t0;
  esp_task_wdt_reset();

  // Lightweight stall signal (kept always-on but only logs when clearly bad).
  // If this fires, consider lowering LV_DISP_DEF_REFR_PERIOD, reducing invalidation,
  // or switching to DMA push if supported by the display driver.
  if (dt > 250) {
    Serial.printf("[LVGL] WARNING: flush_cb %ldx%ld took %lu ms\n",
                  (long)w, (long)h, (unsigned long)dt);
  }

  lv_display_flush_ready(disp);
}

static void touch_read_cb(lv_indev_t*, lv_indev_data_t* data) {
  auto t = M5.Touch.getDetail();
  if (t.isPressed()) {
    data->point.x = t.x;
    data->point.y = t.y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

static uint32_t tick_cb() {
  return millis();
}
} // namespace LVGLBridge
