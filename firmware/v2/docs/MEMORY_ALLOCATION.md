# Memory Allocation Guide for LightwaveOS v2

> **MANDATORY READING**: All agents and developers MUST read this document before modifying any firmware/v2 code. Memory crashes have been a recurring issue. Failure to understand these constraints will result in heap exhaustion, WiFi/lwIP crashes, and device instability.

---

## Executive Summary

LightwaveOS v2 runs on the **ESP32-S3 N16R8**, which has:
- **320 KB Internal SRAM** (DMA-capable, required by WiFi/lwIP/FreeRTOS)
- **8 MB PSRAM** (external, slower, NOT DMA-capable)

The firmware historically crashed due to **internal SRAM exhaustion** while diagnostic tools reported abundant "free heap" (which combines internal + PSRAM). This document establishes hard rules and budgets to prevent future memory-related crashes.

**Key insight**: `ESP.getFreeHeap()` and `esp_get_free_heap_size()` return **combined** internal + PSRAM values, masking true internal SRAM exhaustion. WiFi crashes when internal SRAM drops below ~8KB.

---

## Table of Contents

1. [Hardware Constraints](#1-hardware-constraints)
2. [Memory Regions](#2-memory-regions)
3. [Allocation Rules](#3-allocation-rules)
4. [Component Memory Budgets](#4-component-memory-budgets)
5. [Diagnostic Gotchas](#5-diagnostic-gotchas)
6. [Common Mistakes](#6-common-mistakes)
7. [Code Patterns](#7-code-patterns)
8. [Initialisation Sequence](#8-initialisation-sequence)
9. [References](#9-references)

---

## 1. Hardware Constraints

### ESP32-S3 N16R8 Memory Map

| Region | Size | Speed | DMA-Capable | Primary Use |
|--------|------|-------|-------------|-------------|
| Internal SRAM | ~320 KB | Fast | Yes | WiFi, lwIP, FreeRTOS, DMA buffers |
| PSRAM (OPI) | 8 MB | Slower | **No** | Large tables, history buffers, caches |
| Flash | 16 MB | Slowest | No | Code, PROGMEM, LittleFS |

### Critical Thresholds

| Metric | Threshold | Consequence of Violation |
|--------|-----------|-------------------------|
| Free Internal SRAM | < 8 KB | WiFi/lwIP stack crashes |
| Free Internal SRAM | < 15 KB | Timer allocation failures (ESP_ERR_NO_MEM) |
| Largest Free Block (Internal) | < 4 KB | Fragmentation issues, allocation failures |

### Startup Memory Baseline

After boot, before network initialisation:
- **Total Internal SRAM available**: ~280 KB (after FreeRTOS/ESP-IDF reservations)
- **Static allocations**: ~100 KB (task stacks, LED buffers, effect registrations)
- **Remaining for runtime**: ~180 KB

---

## 2. Memory Regions

### 2.1 Internal SRAM (~320 KB)

**What MUST stay here (non-negotiable):**

| Component | Size (approx) | Reason |
|-----------|---------------|--------|
| LED DMA buffers (`RendererActor::m_leds[]`) | 960 B | RMT/DMA requires internal SRAM |
| FastLED/RMT driver buffers | ~2 KB | DMA peripheral access |
| I2S microphone DMA buffers | ~1 KB | DMA peripheral access |
| FreeRTOS task stacks | ~96 KB total | FreeRTOS requirement |
| WiFi/lwIP buffers | ~40-60 KB | Network stack requirement |
| AsyncTCP/WebSocket per-client buffers | ~4.6 KB/client | TCP stack requirement |
| ESP-IDF timers and queues | ~10-20 KB | System requirement |

**What SHOULD stay here (performance-critical):**

| Component | Size | Reason |
|-----------|------|--------|
| Active effect state (per-frame) | < 2 KB | 120 FPS inner loop |
| Current palette (CRGB[16]) | 48 B | Per-pixel lookups |
| Audio band magnitudes (current frame) | 256 B | DSP hot path |

### 2.2 PSRAM (8 MB)

**What SHOULD go here (large, non-DMA, infrequent access):**

| Component | Size | Notes |
|-----------|------|-------|
| ESV11 sample_history buffer | 16 KB | `SAMPLE_HISTORY_LENGTH * sizeof(float)` = 4096 * 4 |
| ESV11 window_lookup buffer | 16 KB | Same size |
| ESV11 novelty_curve buffers (x4) | 16 KB | 4 buffers * 1024 * sizeof(float) |
| ESV11 tempi array | 3.8 KB | `NUM_TEMPI * sizeof(tempo)` |
| ESV11 frequencies_musical | ~5 KB | `NUM_FREQS * sizeof(freq)` |
| ESV11 spectrogram_average | 3 KB | 12 samples * 64 bins * 4 bytes |
| ESV11 noise_history | 2.5 KB | 10 samples * 64 bins * 4 bytes |
| **Total ESV11 PSRAM usage** | **~68 KB** | Previously crashed when in internal SRAM |
| ZoneComposer buffers | ~4 KB | `MAX_ZONES * TOTAL_LEDS * sizeof(CRGB)` = 4 * 320 * 3 |
| TransitionEngine buffers | ~2 KB | Source/target frame buffers |
| AudioMappingRegistry | ~8 KB | Effect-to-audio mapping tables |
| Debug capture buffers | ~3 KB | Triple-buffered frame captures |

**What is SAFE for PSRAM:**

- Lookup tables (palettes, gamma curves)
- History buffers (audio, tempo tracking)
- Configuration tables (zone mappings, effect registrations)
- Debug-only features (frame capture, validation profiling)
- Infrequently accessed caches

### 2.3 Flash/PROGMEM

**Use for truly constant data:**

- Palette definitions (`PROGMEM`)
- Effect metadata strings
- Gamma correction tables
- Configuration templates

---

## 3. Allocation Rules

### 3.1 Hard Rules (MUST/MUST NOT)

| Rule | Rationale |
|------|-----------|
| **MUST NOT** allocate in `render()` paths | 120 FPS = 8.3ms budget; heap ops break timing |
| **MUST NOT** use `new`, `malloc`, or `String` in render loops | Heap fragmentation and timing |
| **MUST** use `heap_caps_calloc(..., MALLOC_CAP_SPIRAM)` for large buffers | Preserve internal SRAM |
| **MUST** place DMA buffers in internal SRAM | Peripherals cannot access PSRAM |
| **MUST** provide fallback when PSRAM allocation fails | Graceful degradation |
| **MUST NOT** assume `ESP.getFreeHeap()` reflects internal SRAM | It includes PSRAM |
| **MUST** use `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)` for diagnostics | True internal SRAM reading |
| **MUST** check PSRAM availability at startup | `psramFound()` and `ESP.getPsramSize()` |

### 3.2 Soft Guidelines (SHOULD/PREFER)

| Guideline | Notes |
|-----------|-------|
| **PREFER** static buffers over heap allocation | Predictable memory layout |
| **SHOULD** use `StaticJsonDocument` over `DynamicJsonDocument` | Fixed stack allocation |
| **SHOULD** limit WebSocket broadcast JSON to < 2 KB | AsyncTCP callback stack is limited |
| **PREFER** PSRAM for anything > 1 KB that isn't DMA | Preserve internal SRAM headroom |
| **SHOULD** monitor stack high-water marks regularly | Catch stack overflows early |
| **PREFER** pre-allocated arrays over `std::vector` in hot paths | Avoid heap fragmentation |

---

## 4. Component Memory Budgets

### 4.1 FreeRTOS Task Stacks (Internal SRAM)

| Actor | Stack Size (words) | Stack Size (bytes) | Actual Usage | Core |
|-------|--------------------|--------------------|--------------|------|
| RendererActor | 4096 | 16 KB | ~10 KB | Core 1 |
| NetworkActor | 3072 | 12 KB | ~8 KB | Core 0 |
| AudioActor | 4096 | 16 KB | ~12 KB | Core 1 |
| SyncManager | 8192 | 32 KB | ~20 KB | Core 0 |
| HmiActor | 2048 | 8 KB | ~4 KB | Core 0 |
| StateStoreActor | 2048 | 8 KB | ~4 KB | Core 1 |
| WiFiManager task | 4096 | 4 KB | ~3 KB | Core 0 |
| EncoderManager task | 4096 | 4 KB | ~2 KB | Core 0 |
| **Total** | | **~96 KB** | | |

All task stacks MUST remain in internal SRAM (FreeRTOS requirement).

### 4.2 Network Subsystem

| Component | Memory | Location | Notes |
|-----------|--------|----------|-------|
| WiFi station mode | ~40 KB | Internal | lwIP buffers |
| WiFi AP mode | ~20 KB | Internal | Additional when concurrent |
| Per WebSocket client | ~4.6 KB | Internal | TCP buffers; max 8 clients |
| AsyncWebServer | ~8 KB | Internal | Request handling |
| mDNS | ~2 KB | Internal | Service advertisement |
| **Worst case (8 WS clients)** | ~100 KB | Internal | Leaves ~80 KB headroom |

### 4.3 Audio DSP (ESV11 Backend)

All ESV11 buffers allocated in PSRAM via `heap_caps_calloc(..., MALLOC_CAP_SPIRAM)`:

| Buffer | Size | Calculation |
|--------|------|-------------|
| sample_history | 16,384 B | 4096 floats |
| window_lookup | 16,384 B | 4096 floats |
| novelty_curve | 4,096 B | 1024 floats |
| novelty_curve_normalized | 4,096 B | 1024 floats |
| vu_curve | 4,096 B | 1024 floats |
| vu_curve_normalized | 4,096 B | 1024 floats |
| tempi | 3,840 B | 96 * sizeof(tempo) ~40B |
| frequencies_musical | 5,120 B | 64 * sizeof(freq) ~80B |
| spectrogram_average | 3,072 B | 12 * 64 * 4 |
| noise_history | 2,560 B | 10 * 64 * 4 |
| **Total ESV11** | **~68 KB** | PSRAM |

### 4.4 Rendering Subsystem

| Component | Size | Location | Notes |
|-----------|------|----------|-------|
| m_leds[320] | 960 B | Internal | DMA buffer for FastLED |
| TransitionEngine (PSRAM mode) | ~2 KB | PSRAM | Source/target buffers |
| TransitionEngine (fallback) | ~2 KB | Internal | If PSRAM unavailable |
| ZoneComposer buffers | ~4 KB | PSRAM | 4 zones * 320 * 3 bytes |
| Frame capture (debug) | ~3 KB | PSRAM | Triple-buffer for tap points |

---

## 5. Diagnostic Gotchas

### 5.1 The `getFreeHeap()` Trap

**WRONG** - This masks internal SRAM exhaustion:
```cpp
// DO NOT USE FOR DIAGNOSTICS
size_t free = ESP.getFreeHeap();  // Returns internal + PSRAM combined!
```

**CORRECT** - Query internal SRAM specifically:
```cpp
#include <esp_heap_caps.h>

// Internal SRAM only (what WiFi needs)
size_t internalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

// PSRAM only
size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

// Largest contiguous block in internal SRAM
size_t largestInternal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
```

### 5.2 Fragmentation Detection

```cpp
size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);

// Fragmentation percentage (0 = no fragmentation, 100 = fully fragmented)
uint8_t fragmentation = 100 - ((largestBlock * 100) / freeInternal);

if (fragmentation > 50) {
    LW_LOGW("Internal SRAM fragmentation high: %d%%", fragmentation);
}
```

### 5.3 Critical Memory Thresholds to Monitor

```cpp
void checkMemoryHealth() {
    size_t internalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    if (internalFree < 8000) {
        LW_LOGE("CRITICAL: Internal SRAM < 8KB! WiFi will crash!");
    } else if (internalFree < 15000) {
        LW_LOGW("WARNING: Internal SRAM < 15KB. Timer allocations at risk.");
    } else if (internalFree < 30000) {
        LW_LOGI("Internal SRAM: %u bytes (healthy)", internalFree);
    }
}
```

### 5.4 Stack High-Water Mark Monitoring

```cpp
// In Actor-derived class
UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
if (hwm < 100) {  // Less than 400 bytes remaining
    LW_LOGW("[%s] Stack low! High water mark: %d words (%d bytes)",
            getName(), hwm, hwm * 4);
}
```

---

## 6. Common Mistakes

### 6.1 Anti-Pattern: String Concatenation in Loops

**BAD** - Creates heap fragmentation:
```cpp
void broadcastStatus() {
    String msg = "{";
    msg += "\"effect\":";
    msg += String(effectId);  // Heap allocation!
    msg += "}";
    ws->textAll(msg);  // msg.c_str() may fragment heap
}
```

**GOOD** - Use ArduinoJson with static document:
```cpp
void broadcastStatus() {
    StaticJsonDocument<256> doc;  // Stack allocation
    doc["effect"] = effectId;
    char buffer[256];
    serializeJson(doc, buffer);
    ws->textAll(buffer);
}
```

### 6.2 Anti-Pattern: Dynamic Allocation in Render Loop

**BAD** - Breaks 120 FPS timing:
```cpp
void IMyEffect::render(CRGB* leds, uint16_t count, EffectContext& ctx) {
    auto* temp = new CRGB[count];  // NEVER allocate here!
    // ... rendering ...
    delete[] temp;
}
```

**GOOD** - Use static or member buffer:
```cpp
class MyEffect : public IEffect {
    CRGB m_tempBuffer[320];  // Member allocation

    void render(CRGB* leds, uint16_t count, EffectContext& ctx) override {
        // Use m_tempBuffer directly
    }
};
```

### 6.3 Anti-Pattern: Large Stack Allocations in Callbacks

**BAD** - AsyncTCP callbacks have limited stack (4-8 KB):
```cpp
void onWsEvent(AsyncWebSocketClient* client, ...) {
    StaticJsonDocument<4096> doc;  // Too large for callback stack!
    char buffer[2048];  // Compounds the problem
    // ...
}
```

**GOOD** - Keep callback allocations small or use task-based handling:
```cpp
void onWsEvent(AsyncWebSocketClient* client, ...) {
    StaticJsonDocument<512> doc;  // Reasonable for callback
    // For larger responses, queue a message to an Actor
}
```

### 6.4 Anti-Pattern: Ignoring PSRAM Allocation Failures

**BAD** - Crashes if PSRAM unavailable:
```cpp
void init() {
    m_buffer = static_cast<float*>(
        heap_caps_calloc(4096, sizeof(float), MALLOC_CAP_SPIRAM));
    // No null check - crashes on access if allocation failed
    m_buffer[0] = 1.0f;
}
```

**GOOD** - Graceful degradation:
```cpp
bool init() {
    m_buffer = static_cast<float*>(
        heap_caps_calloc(4096, sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    if (!m_buffer) {
        // Try internal SRAM as fallback (smaller buffer)
        m_buffer = static_cast<float*>(
            heap_caps_calloc(1024, sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        if (!m_buffer) {
            LW_LOGE("Buffer allocation failed - feature disabled");
            m_enabled = false;
            return false;
        }
        m_bufferSize = 1024;
        LW_LOGW("Using reduced buffer (internal SRAM fallback)");
    } else {
        m_bufferSize = 4096;
    }
    m_enabled = true;
    return true;
}
```

### 6.5 Anti-Pattern: Trusting HeapMonitor for WiFi Health

**BAD** - HeapMonitor uses combined heap metrics:
```cpp
if (HeapMonitor::getFreeHeap() > 10000) {
    // WiFi is fine! (WRONG - this includes PSRAM)
}
```

**GOOD** - Check internal SRAM specifically:
```cpp
if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) > 15000) {
    // WiFi has adequate internal SRAM headroom
}
```

---

## 7. Code Patterns

### 7.1 Correct PSRAM Allocation with Fallback

```cpp
#include <esp_heap_caps.h>

bool MyComponent::allocateBuffers() {
    const size_t bufferSize = 16384;  // 16 KB

    // Attempt PSRAM allocation first
    m_buffer = static_cast<uint8_t*>(
        heap_caps_calloc(1, bufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    if (m_buffer) {
        m_bufferLocation = BufferLocation::PSRAM;
        LW_LOGI("Allocated %u bytes in PSRAM", bufferSize);
        return true;
    }

    // Fallback to internal SRAM with reduced size
    const size_t fallbackSize = 4096;
    m_buffer = static_cast<uint8_t*>(
        heap_caps_calloc(1, fallbackSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    if (m_buffer) {
        m_bufferLocation = BufferLocation::InternalSRAM;
        m_bufferSize = fallbackSize;
        LW_LOGW("PSRAM unavailable, using %u byte internal buffer", fallbackSize);
        return true;
    }

    // Complete failure - disable feature
    LW_LOGE("Buffer allocation failed - feature disabled");
    m_enabled = false;
    return false;
}
```

### 7.2 PSRAM Verification at Startup

```cpp
void verifyPsramConfiguration() {
    if (!psramFound()) {
        LW_LOGE("PSRAM NOT DETECTED! Build requires PSRAM.");
        LW_LOGE("Check platformio.ini: board_build.psram_type = opi");
        // Enter safe mode or halt
        return;
    }

    size_t psramSize = ESP.getPsramSize();
    size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    LW_LOGI("PSRAM: %u KB total, %u KB free", psramSize / 1024, psramFree / 1024);

    if (psramSize < 8 * 1024 * 1024) {
        LW_LOGW("Expected 8MB PSRAM, found %u MB", psramSize / (1024 * 1024));
    }
}
```

### 7.3 Memory-Safe WebSocket Response

```cpp
void sendEffectStatus(AsyncWebSocketClient* client) {
    // Small stack-allocated document for callback safety
    StaticJsonDocument<512> doc;

    doc["type"] = "effect_status";
    doc["effectId"] = m_currentEffectId;
    doc["brightness"] = m_brightness;
    doc["speed"] = m_speed;

    // Measure serialised size before allocation
    size_t jsonSize = measureJson(doc);

    // Use stack buffer if small enough
    if (jsonSize < 256) {
        char buffer[256];
        serializeJson(doc, buffer);
        client->text(buffer);
    } else {
        // Larger response - check heap before allocation
        size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        if (freeInternal < 20000) {
            LW_LOGW("Skipping large response - low internal SRAM");
            return;
        }
        // Proceed with allocation
        String response;
        serializeJson(doc, response);
        client->text(response);
    }
}
```

### 7.4 DMA Buffer Allocation (Internal SRAM Only)

```cpp
// LED buffers MUST stay in internal SRAM for RMT/DMA
class RendererActor {
private:
    // Static member - guaranteed internal SRAM
    CRGB m_leds[LedConfig::TOTAL_LEDS];  // 320 * 3 = 960 bytes

    // DO NOT use PSRAM for DMA buffers:
    // WRONG: heap_caps_malloc(..., MALLOC_CAP_SPIRAM)
};
```

### 7.5 Diagnostics Logging Function

```cpp
void logMemoryDiagnostics(const char* context) {
    size_t internalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t internalLargest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    uint8_t fragmentation = (internalFree > 0)
        ? (100 - ((internalLargest * 100) / internalFree))
        : 100;

    LW_LOGI("[%s] Internal: %uKB free, %uKB largest block, %d%% frag | PSRAM: %uKB free",
            context,
            internalFree / 1024,
            internalLargest / 1024,
            fragmentation,
            psramFree / 1024);

    // Warning thresholds
    if (internalFree < 15000) {
        LW_LOGW("Internal SRAM critically low!");
    }
    if (fragmentation > 50) {
        LW_LOGW("Internal SRAM fragmentation high!");
    }
}
```

---

## 8. Initialisation Sequence

The firmware initialises components in a specific order to ensure PSRAM is available before large allocations. See [INIT_SEQUENCE.md](./INIT_SEQUENCE.md) for full details.

**Key points:**

1. **Boot verification** - OTA checks before anything else
2. **System monitoring** - HeapMonitor, StackMonitor initialised early
3. **Actor system** - Creates FreeRTOS tasks (internal SRAM)
4. **Effect registration** - Metadata only, no large buffers
5. **PSRAM-backed subsystems** - AudioMappingRegistry, ZoneComposer (allocation may fail)
6. **Network** - WiFi and WebServer (consumes significant internal SRAM)

**Graceful degradation**: If PSRAM allocation fails during step 5, the subsystem disables itself but the device continues to function with reduced features.

---

## 9. References

### Internal Documentation

| Document | Path | Topic |
|----------|------|-------|
| **Memory Inventory** | [MEMORY_ALLOCATION_INVENTORY.md](./MEMORY_ALLOCATION_INVENTORY.md) | Forensic file:line allocation map |
| **Reference Tables** | [MEMORY_REFERENCE_TABLES.md](./MEMORY_REFERENCE_TABLES.md) | Configuration constants, buffer sizes |
| DMA/PSRAM Rules | [DMA_AND_PSRAM.md](./DMA_AND_PSRAM.md) | What can/cannot use PSRAM |
| Init Sequence | [INIT_SEQUENCE.md](./INIT_SEQUENCE.md) | Startup order for memory safety |
| Architecture | [../ARCHITECTURE.md](../ARCHITECTURE.md) | System overview |
| Build Variants | [../docs/agent/BUILD.md](../docs/agent/BUILD.md) | PSRAM configuration per board |

### Key Source Files

| File | Relevance |
|------|-----------|
| `src/audio/backends/esv11/vendor/EsV11Buffers.cpp` | PSRAM allocation for DSP |
| `src/effects/zones/ZoneComposer.cpp:191-198` | Zone buffer PSRAM allocation |
| `src/effects/transitions/TransitionEngine.cpp:49-68` | Transition buffer allocation |
| `src/core/system/HeapMonitor.cpp` | Heap monitoring (uses combined metrics!) |
| `src/network/webserver/UdpStreamer.cpp:226` | Correct internal SRAM check |
| `src/network/WebServer.cpp:139` | Internal SRAM getter |
| `src/core/actors/Actor.h:208-573` | Task stack size definitions |

### ESP-IDF Documentation

- [ESP32-S3 Memory Map](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/mem_alloc.html)
- [Heap Capabilities Allocator](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/heap_debug.html)
- [PSRAM Configuration](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/external-ram.html)

---

## Appendix A: Quick Reference Card

### Memory Check Snippets

```cpp
// Internal SRAM free
heap_caps_get_free_size(MALLOC_CAP_INTERNAL)

// PSRAM free
heap_caps_get_free_size(MALLOC_CAP_SPIRAM)

// Largest contiguous block (internal)
heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)

// PSRAM available?
psramFound() && ESP.getPsramSize() > 0
```

### Allocation Flags

```cpp
// PSRAM (preferred for large buffers)
MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT

// Internal SRAM (required for DMA)
MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT

// Default (combined, avoid for explicit allocation)
MALLOC_CAP_DEFAULT
```

### Critical Thresholds

| Metric | Warning | Critical |
|--------|---------|----------|
| Internal SRAM Free | < 30 KB | < 8 KB |
| Internal Fragmentation | > 50% | > 75% |
| Task Stack Remaining | < 1 KB | < 400 B |

---

## Appendix B: Build Configuration

### platformio.ini PSRAM Settings (Required)

```ini
[env:esp32dev_audio]
board = esp32-s3-devkitc1-n16r8
board_build.arduino.memory_type = qio_opi
build_flags =
    -D BOARD_HAS_PSRAM
```

**WARNING**: Missing `board_build.arduino.memory_type = qio_opi` will result in PSRAM not being initialised, causing the device to run on internal SRAM only (~280 KB) and crash under load.

---

*Document version: 1.0*
*Last updated: 2026-02-06*
*Author: LightwaveOS Team*
