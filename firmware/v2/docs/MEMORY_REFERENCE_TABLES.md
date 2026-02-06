# LightwaveOS Memory Allocation Reference Tables

Technical reference for memory allocation, buffer configurations, and system constraints.

---

## Table of Contents

1. [ESP32 Hardware Specifications](#esp32-hardware-specifications)
2. [Build Configuration Flags](#build-configuration-flags)
3. [Memory Budget Allocation](#memory-budget-allocation)
4. [FreeRTOS Task Stack Configuration](#freertos-task-stack-configuration)
5. [Buffer Size Reference](#buffer-size-reference)
6. [Network Configuration Constants](#network-configuration-constants)
7. [Audio Pipeline Configuration](#audio-pipeline-configuration)
8. [Safety Thresholds and Limits](#safety-thresholds-and-limits)
9. [Build Environment Variants](#build-environment-variants)

---

## ESP32 Hardware Specifications

### ESP32-S3 (Primary Hardware)

| Parameter | Value | Unit | Notes |
|-----------|-------|------|-------|
| **CPU Architecture** | Xtensa LX7 | - | Dual-core |
| **CPU Cores** | 2 | cores | Core 0 (network), Core 1 (rendering) |
| **CPU Frequency** | 240 | MHz | Maximum |
| **Internal SRAM** | 384 | KB | Total capacity |
| **PSRAM OPI** | 8 | MB | Optional (populated on N16R8) |
| **PSRAM Type** | OPI (Octal) | - | Quad-SPI: use qio_qspi on FH4R2 |
| **RMT Channels** | 8 | channels | For LED strip control |
| **GPIO Pins** | 45 | pins | Total available |
| **Flash Memory** | 16 | MB | N16R8 variant |
| **WiFi Integrated** | Yes | - | 802.11 b/g/n |
| **Bluetooth Integrated** | Yes | - | BLE 5.2 |
| **Ethernet MAC** | No | - | Not integrated |

### ESP32-P4 (Alternative: Higher Performance)

| Parameter | Value | Unit | Notes |
|-----------|-------|------|-------|
| **CPU Architecture** | RISC-V HP | - | 400 MHz HP core + 40 MHz LP core |
| **CPU Cores** | 2 | cores | HP core + LP core |
| **CPU Frequency (HP)** | 400 | MHz | Higher performance |
| **CPU Frequency (LP)** | 40 | MHz | For background tasks |
| **Internal SRAM** | 768 | KB | 2x ESP32-S3 |
| **PSRAM** | 32 | MB | Maximum capacity |
| **RMT Channels** | 4 | channels | 2x fewer than S3 |
| **GPIO Pins** | 55 | pins | More than S3 |
| **Flash Memory** | 8 | MB | Typical configuration |
| **WiFi Integrated** | No | - | Use ESP-Hosted |
| **Bluetooth Integrated** | No | - | Not available |
| **Ethernet MAC** | Yes | - | 10/100 Mbps (unused) |

### Memory Regions (ESP32-S3)

| Region | Size | Purpose |
|--------|------|---------|
| **SRAM** | 320 KB | Main working RAM (after boot) |
| **Available after boot** | ~280 KB | User allocation pool |
| **WiFi SRAM pool** | ~40 KB | WiFi stack (AP+STA concurrent) |
| **FreeRTOS heap** | ~200 KB | Task stacks + dynamic alloc |
| **DMA SRAM** | 16 KB | Shared with main heap |

---

## Build Configuration Flags

### Memory-Related PlatformIO Flags

| Flag | Value | Purpose | Build |
|------|-------|---------|--------|
| **BOARD_HAS_PSRAM** | 1 | Enable PSRAM support | esp32dev_audio |
| **PSRAM_SIZE** | 8388608 | PSRAM size in bytes (8MB) | esp32dev_audio |
| **PSRAM_SIZE** | 2097152 | PSRAM size in bytes (2MB) | esp32dev_FH4R2 |
| **board_build.psram_type** | opi | Octal SPI (8-bit bus) | esp32dev_audio |
| **board_build.psram_type** | qio | Quad SPI (4-bit bus) | esp32dev_FH4R2 |
| **board_build.f_cpu** | 240000000L | CPU frequency (240 MHz) | esp32dev_audio |
| **board_build.f_flash** | 80000000L | Flash frequency (80 MHz) | secondary boards |
| **CONFIG_ASYNC_TCP_RUNNING_CORE** | 0 | AsyncTCP affinity | all |
| **CONFIG_ASYNC_TCP_USE_WDT** | 0 | Disable watchdog for async | all |

### Heap Monitoring and Debug Flags

| Flag | Value | Purpose | Default |
|------|-------|---------|---------|
| **CONFIG_HEAP_CORRUPTION_DETECTION** | 1 | Detect heap corruption | Enabled |
| **CONFIG_HEAP_POISONING_COMPREHENSIVE** | 1 | Fill freed memory with pattern | Enabled |
| **configUSE_MALLOC_FAILED_HOOK** | 1 | Hook on malloc failure | Enabled |
| **FEATURE_HEAP_MONITORING** | 1 | Enable heap monitoring subsystem | Enabled |
| **FEATURE_MEMORY_LEAK_DETECTION** | 1 | Track memory leaks | Enabled |
| **FEATURE_MEMORY_DEBUG** | 0 | Detailed allocation tracking | Disabled |
| **FEATURE_STACK_PROFILING** | 1 | Monitor stack usage per task | Enabled |
| **FEATURE_VALIDATION_PROFILING** | 1 | Profile effect validation | Enabled |

### Stack Overflow Detection

| Flag | Value | Purpose |
|------|-------|---------|
| **configCHECK_FOR_STACK_OVERFLOW** | 2 | Method 2: check on context switch |
| **configASSERT_DEFINED** | 1 | Enable assert checks |
| **CONFIG_FREERTOS_ASSERT_FAIL_ABORT** | 1 | Abort on assert failure |

### FastLED Configuration

| Flag | Value | Purpose | Notes |
|------|-------|---------|-------|
| **FASTLED_RMT_BUILTIN_DRIVER** | 1 | Use built-in RMT driver | Required |
| **FASTLED_ALLOW_INTERRUPTS** | 0 | Disable interrupts during show | Timing critical |
| **FASTLED_ESP32_FLASH_LOCK** | 0 | No flash locking needed | |
| **FASTLED_INTERRUPT_RETRY_COUNT** | 0 | No retry on interrupt | |

### WebSocket Configuration

| Flag | Value | Purpose | Notes |
|------|-------|---------|-------|
| **WS_MAX_QUEUED_MESSAGES** | 8 | Max queued outgoing frames | Low to prevent SRAM starvation |
| **WS_MAX_CLIENTS** | 4 | Maximum concurrent clients | network_config.h |

---

## Memory Budget Allocation

### Total System Memory (ESP32-S3)

| Component | Size | Allocation | Notes |
|-----------|------|-----------|-------|
| **Total SRAM** | 384 KB | Fixed | Hardware limit |
| **Available for application** | ~280 KB | Dynamic | After boot overhead |
| **Reserved (boot/framework)** | ~104 KB | Fixed | FreeRTOS + WiFi |
| **Recommended max use** | ~224 KB | 80% threshold | Safety margin |

### Per-Subsystem Allocation

| Subsystem | Bytes | Percentage | Purpose |
|-----------|-------|-----------|---------|
| **LED Buffers** | 1,920 | 0.5% | 2 strips × 160 LEDs × 3 bytes + transition |
| **Audio DMA** | 8,192 | 2.6% | 4 buffers × 512 samples × 4 bytes |
| **Audio Analysis** | 24,576 | 7.8% | Goertzel + ControlBus state |
| **Effect Context** | 12,288 | 3.9% | Audio reactive context per frame |
| **Zone System** | 3,840 | 1.2% | Zone buffers + metadata |
| **Network (WiFi stack)** | 40,960 | 13% | WiFi SRAM + AsyncTCP |
| **WebSocket buffers** | 4,608 | 1.5% | RX/TX queues (8 msg × 464 bytes) |
| **FreeRTOS kernel** | 16,384 | 5.2% | Scheduler + task control blocks |
| **Task Stacks** | 73,728 | 23.5% | Renderer(8KB) + Audio(16KB) + WiFi(4KB) + others |
| **Dynamic heap pool** | ~100,000 | ~32% | Available for malloc/new |

### Critical Thresholds

| Threshold | Value (KB) | Condition | Action |
|-----------|-----------|-----------|--------|
| **Minimum safe heap** | 40 | Min for stable WiFi | chip_esp32s3.h |
| **WiFi warning** | 50 | Low heap warning | Log + monitor |
| **Critical threshold** | 30 | Emergency condition | Disable features |
| **OOM trigger** | < 20 | Out of memory | System restart |

---

## FreeRTOS Task Stack Configuration

### Task Stack Sizes (in 32-bit words)

| Task | Stack (Words) | Stack (Bytes) | Core | Priority | Purpose |
|------|--------------|---------------|------|----------|---------|
| **RendererActor** | 8192 | 32 KB | 1 | 5 | Effect rendering (time-critical) |
| **AudioActor** | 4096 | 16 KB | 0 | 4 | Audio analysis + sync |
| **WiFiManager** | 4096 | 16 KB | 0 | 3 | WiFi state machine |
| **WebServer** | 4096 | 16 KB | 0 | 2 | HTTP + WebSocket handler |
| **OTA** | 3072 | 12 KB | 0 | 2 | OTA firmware update |
| **Serial menu** | 2048 | 8 KB | 0 | 1 | CLI command processing |
| **Encoder** | 4096 | 16 KB | 0 | 3 | M5ROTATE8 jog dial |
| **Idle task** | 2048 | 8 KB | Both | 0 | FreeRTOS idle |

### Stack Size Derivation

```
Audio Actor Stack:
- AUDIO_ACTOR_STACK_WORDS = 4096 (16 KB)
- Purpose: Goertzel analysis, TempoTracker, ControlBus filtering
- Rationale: Deep call stack for audio analysis + local arrays
- Note: Increased from 3072 to 4096 in Jan 2026 for robustness

Renderer Stack:
- 8192 words (32 KB)
- Purpose: Effect rendering, zone calculation, transitions
- Rationale: Highest priority, needs margin for nested effects
- Note: Core 1 exclusive (no WiFi contention)

WiFi/Network tasks:
- 4096 words per task (WiFiManager, WebServer)
- Purpose: Async network operations, callback chains
- Rationale: FreRTOS async patterns need deep stacks
```

### P4 Stack Multiplier

| Chip | Multiplier | Reason |
|------|-----------|--------|
| **ESP32-S3** | 1.0x | Xtensa baseline |
| **ESP32-P4** | 1.2x | RISC-V needs ~20% more stack |

---

## Buffer Size Reference

### LED Streaming Buffers

| Buffer | Size (bytes) | Content | Purpose |
|--------|------------|---------|---------|
| **LED frame (v1)** | 966 | [MAGIC][VERSION][STRIPS][LEDS_PER][STRIP0_ID][RGB×160][STRIP1_ID][RGB×160] | WebSocket streaming |
| **LED payload** | 962 | Dual-strip RGB data (2 × 481 bytes) | Frame body |
| **LED frame header** | 4 | Magic + version + num strips + LEDs per strip | Frame metadata |
| **Strip 0 data** | 481 | Strip ID (1) + RGB×160 (480) | Top edge LEDs |
| **Strip 1 data** | 481 | Strip ID (1) + RGB×160 (480) | Bottom edge LEDs |
| **Legacy frame (v0)** | 961 | [MAGIC][RGB×320] | For compatibility |

### Audio Streaming Buffers

| Buffer | Size (bytes) | Content | Purpose |
|--------|------------|---------|---------|
| **Audio frame** | 464 | [MAGIC][VERSION][HOP][BANDS_DATA][HEAVY_BANDS_DATA] | UDP/WebSocket audio |
| **Bands data** | 32 | 8 floats × 4 bytes | Frequency analysis |
| **Heavy bands data** | 32 | 8 floats × 4 bytes | Heavy frequency bands |
| **Audio frame header** | 8 | Metadata + offsets | Frame control |

### DMA Buffers (I2S)

| Buffer | Count | Size Per (samples) | Total (bytes) | Purpose |
|--------|-------|------------------|---------------|---------|
| **DMA buffers** | 4 | 512 | 8,192 | I2S microphone input |
| **Single buffer** | 1 | 512 | 2,048 | One DMA transaction |

### WebSocket Message Queue

| Queue | Capacity | Item Size (bytes) | Total (bytes) | Purpose |
|-------|----------|------------------|--------------|---------|
| **Outgoing messages** | 8 | variable | ~3,712 | AsyncWebSocket queue |
| **Per-client buffer** | 1 | 464 | 464 | UDP audio stream |
| **Per-client buffer** | 1 | 966 | 966 | LED frame stream |

---

## Network Configuration Constants

### WebSocket Settings

| Parameter | Value | Unit | Purpose |
|-----------|-------|------|---------|
| **WS_MAX_CLIENTS** | 4 | clients | Maximum concurrent connections |
| **WS_MAX_QUEUED_MESSAGES** | 8 | messages | Per-client outgoing queue limit |
| **WS_PING_INTERVAL_MS** | 15000 | ms | Heartbeat/keepalive interval |
| **WEBSOCKET_PORT** | 81 | - | WebSocket server port |

### Web Server Settings

| Parameter | Value | Unit | Purpose |
|-----------|-------|------|---------|
| **WEB_SERVER_PORT** | 80 | - | HTTP server port |
| **MDNS_HOSTNAME** | lightwaveos | - | mDNS hostname (lightwaveos.local) |
| **WIFI_CONNECT_TIMEOUT_MS** | 20000 | ms | STA connection timeout |
| **WIFI_RETRY_COUNT** | 5 | retries | Scan/connection retry attempts |

### WiFi AP (Soft-AP) Settings

| Parameter | Value | Unit | Purpose |
|-----------|-------|------|---------|
| **AP_SSID** | LightwaveOS-AP | - | SSID broadcast name |
| **AP_PASSWORD** | "" | - | Empty = open network |
| **AP_AUTH_MODE** | WPA2_PSK | - | Conditional on password |
| **AP_MAX_CLIENTS** | 4 | clients | Maximum concurrent connections |
| **SCAN_INTERVAL_MS** | 60000 | ms | WiFi rescan period |
| **RECONNECT_DELAY_MS** | 5000 | ms | Between retry attempts |
| **MAX_RECONNECT_DELAY_MS** | 60000 | ms | Maximum backoff |

### WiFi STA (Station) Settings

| Parameter | Value | Unit | Purpose |
|-----------|-------|------|---------|
| **WIFI_ATTEMPTS_PER_NETWORK** | 2 | attempts | Try each network 2× before switching |
| **WIFI_SSID_NETWORKS** | 3 | networks | Primary + 2 fallback networks |
| **WIFI_AP_ONLY** | Configurable | flag | FH4R2 forces AP-only mode |

---

## Audio Pipeline Configuration

### Audio Parameters

| Parameter | Value | Unit | Notes |
|-----------|-------|------|-------|
| **Sample Rate (S3)** | 12800 | Hz | Emotiscope Tab5 parity |
| **Sample Rate (P4)** | 16000 | Hz | P4 onboard front-end |
| **Hop Size (S3)** | 256 | samples | 20ms @ 12.8kHz = 50 Hz |
| **Hop Size (P4)** | 160 | samples | 10ms @ 16kHz = 100 Hz |
| **Hop Rate (S3)** | 50 | Hz | Update frequency |
| **Hop Rate (P4)** | 125 | Hz | Update frequency |
| **FFT Size** | 512 | samples | Spectral analysis window |
| **Goertzel Window** | 512 | samples | Bass coherence detection |

### Frequency Bands

| Band | Center Freq (Hz) | Description |
|------|------------------|-------------|
| **0** | 60 | Sub-bass / kick |
| **1** | 120 | Bass |
| **2** | 250 | Low-mid |
| **3** | 500 | Mid |
| **4** | 1000 | Upper-mid |
| **5** | 2000 | Presence |
| **6** | 4000 | Brilliance |
| **7** | 7800 | Air / hi-hats |

### Band Allocation

| Parameter | Value | Size (bytes) | Purpose |
|-----------|-------|-------------|---------|
| **NUM_BANDS** | 8 | - | Frequency band count |
| **bands[]** | [0,1] | 32 | Regular band magnitudes |
| **heavy_bands[]** | [0,1] | 32 | Heavy frequency bands |
| **band_smoothing** | 0.85 | per-band | Exponential moving average |

### Audio Actor Configuration

| Parameter | Value | Unit | Purpose |
|-----------|-------|------|---------|
| **AUDIO_ACTOR_STACK_WORDS** | 4096 | words | 16 KB stack |
| **AUDIO_ACTOR_PRIORITY** | 4 | FreeRTOS | Below Renderer (5) |
| **AUDIO_ACTOR_CORE** | 0 | core | Core 0 (WiFi compatible) |
| **AUDIO_ACTOR_TICK_MS** | 20 | ms | Hop interval (S3) |
| **STALENESS_THRESHOLD_MS** | 100 | ms | Data freshness limit |

---

## Safety Thresholds and Limits

### System Constraints

| Constraint | Value | Limit | Notes |
|-----------|-------|-------|-------|
| **Effect ID Range** | 0-112 | 113 total | MAX_EFFECTS = 113 |
| **Parameter queue size** | 16 | entries | PARAM_QUEUE_SIZE |
| **LED count** | 320 | LEDs | 2 × 160 per strip |
| **LEDs per strip** | 160 | LEDs | Dual strip configuration |
| **Center point** | 79/80 | index | Origin for effects |

### Memory Safety Thresholds

| Threshold | Value (KB) | Action | Context |
|-----------|-----------|--------|---------|
| **Minimum safe** | 40 | Allow normal ops | MIN_FREE_HEAP_KB |
| **Warning level** | 50 | Log warning | Monitor actively |
| **Critical** | 30 | Degrade features | Disable optional systems |
| **Emergency** | < 20 | Restart system | Out-of-memory shutdown |

### Performance Budgets

| Operation | Budget | Actual | Unit | Notes |
|-----------|--------|--------|------|-------|
| **Frame total** | 8.33 | 5.68 | ms | 120 FPS target |
| **Effect calc** | 2.0 | ~1.2 | ms | 50% margin |
| **Transition** | 1.0 | ~0.8 | ms | Per-zone blend |
| **FastLED.show()** | Fixed | 2.5 | ms | Hardware timing |
| **LED data transfer** | Fixed | 9.6 | ms | 320 LEDs @ 800kHz |

### WiFi Constraints

| Parameter | Value | Unit | Purpose |
|-----------|-------|------|---------|
| **Max concurrent clients** | 4 | clients | WebSocket limit |
| **Max queued messages/client** | 8 | messages | Prevent SRAM starvation |
| **Ping interval** | 15 | seconds | Keepalive timeout |
| **Connection timeout** | 20 | seconds | STA connection attempt |
| **Reconnect backoff** | 5-60 | seconds | Exponential backoff |

---

## Build Environment Variants

### Memory Configurations by Board

| Board | Flash | PSRAM | SRAM | Config |
|-------|-------|-------|------|--------|
| **esp32dev_audio (N16R8)** | 16 MB | 8 MB OPI | 384 KB | Primary |
| **esp32dev_FH4R2** | 4 MB | 2 MB QIO | 384 KB | Constrained |
| **esp32dev_FH4** | 4 MB | None | 384 KB | SRAM-only |
| **esp32dev_SSB (Secondary)** | 4 MB | 2 MB OPI | 384 KB | Custom GPIO |
| **esp32p4_idf** | 8 MB | 32 MB | 768 KB | High-perf |

### Build Flags by Variant

#### Primary Build (esp32dev_audio)

```
-D BOARD_HAS_PSRAM=1
-D PSRAM_SIZE=8388608
-D board_build.arduino.memory_type=qio_opi
-D FEATURE_HEAP_MONITORING=1
-D FEATURE_MEMORY_LEAK_DETECTION=1
-D FEATURE_STACK_PROFILING=1
-D FEATURE_VALIDATION_PROFILING=1
```

#### Constrained Build (esp32dev_FH4)

```
-D BOARD_HAS_PSRAM=0
-D PSRAM_SIZE=0
-D FEATURE_HEAP_MONITORING=0
-D FEATURE_MEMORY_LEAK_DETECTION=0
-D FEATURE_VALIDATION_PROFILING=0
-D FEATURE_STACK_PROFILING=0
-D FEATURE_TRANSITIONS=0
```

#### P4 Build (esp32p4_idf)

```
-D FEATURE_WIFI=0
-D FEATURE_ETHERNET=0
-D FEATURE_WEB_SERVER=0
-D FEATURE_MULTI_DEVICE=0
-D SRAM_SIZE_KB=768
```

---

## Critical Sections: No Allocation

### Render Loop (Every 8.33ms)

```cpp
// ❌ FORBIDDEN in RendererActor::render()
void render() {
    CRGB* buffer = new CRGB[160];  // Dynamic allocation
    String json = "{...}";          // String construction
    // ...
    delete[] buffer;
}

// ✅ REQUIRED: Static allocation
static CRGB zoneBuffer[160];
void render() {
    // Use pre-allocated zoneBuffer
}
```

### Audio Analysis Hop (Every 20ms)

```cpp
// ❌ FORBIDDEN in AudioActor::analyzeHop()
void analyzeHop() {
    float* bands = (float*)malloc(8 * sizeof(float));  // malloc in hot path
    // ...
    free(bands);
}

// ✅ REQUIRED: Pre-allocated
static float bandsBuffer[CONTROLBUS_NUM_BANDS];
void analyzeHop() {
    // Reuse static bandsBuffer
}
```

---

## Reference Links

### Source Files
- `platformio.ini` - Build flags and environment configuration
- `src/config/chip_esp32s3.h` - ESP32-S3 hardware constants
- `src/config/chip_esp32p4.h` - ESP32-P4 hardware constants
- `src/config/audio_config.h` - Audio pipeline parameters
- `src/config/network_config.h` - Network/WebSocket settings
- `src/config/features.h` - Feature flag defaults
- `src/core/actors/RendererActor.h` - Renderer stack + effect limits
- `src/audio/contracts/ControlBus.h` - Band configuration
- `src/network/webserver/LedFrameEncoder.h` - LED streaming format

### Measured Constants
- Audio DMA: 4 buffers × 512 samples = 8 KB total
- LED frame: 966 bytes (header 4 + payload 962)
- Audio frame: 464 bytes (bands + heavy_bands + header)
- WiFi SRAM: ~40 KB (AP+STA concurrent mode)
- Renderer stack: 32 KB (highest priority, timing critical)
- Audio stack: 16 KB (analysis + smoothing)

---

**Last Updated:** 2026-02-06
**Document Version:** 2.0 (Memory Allocation Reference)
**Target Hardware:** ESP32-S3 N16R8, ESP32-S3 FH4R2, ESP32-S3 FH4, ESP32-P4
