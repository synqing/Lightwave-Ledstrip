# Technical Debrief: "Audio Unavailable" Issue on ESP32-P4

**Date:** Session Analysis  
**Platform:** ESP32-P4 (Waveshare ESP32-P4-WIFI6)  
**Firmware:** LightwaveOS-P4 v2  
**Build System:** ESP-IDF v5.5.2  

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Observed Symptoms](#2-observed-symptoms)
3. [System Architecture Overview](#3-system-architecture-overview)
4. [Detailed Code Path Analysis](#4-detailed-code-path-analysis)
5. [Attempted Fixes and Why They Failed](#5-attempted-fixes-and-why-they-failed)
6. [Root Cause Analysis](#6-root-cause-analysis)
7. [Diagnostic Checklist](#7-diagnostic-checklist)
8. [Recommended Resolution Path](#8-recommended-resolution-path)

---

## 1. Executive Summary

The ESP32-P4 build of LightwaveOS reports "Audio unavailable" every 2 seconds in serial output, with the diagnostic message showing:

```
Audio unavailable: seq=0 prevSeq=0 age_s=X.XXX staleness_s=0.100 hop_seq=0
```

The critical observation is **`seq=0` and `prevSeq=0`** - the sequence number from the `SnapshotBuffer` never increments. This definitively proves that **AudioActor is never publishing ControlBusFrame data** to the shared buffer that RendererActor reads.

The investigation initially pursued a FreeRTOS tick interval hypothesis, but this was **incorrect**. The actual issue lies upstream in the audio pipeline - specifically, `AudioActor::onTick()` is either:
1. Never being called, OR
2. Being called but returning early due to state not being `RUNNING`, OR
3. Being called but `captureHop()` is failing, OR
4. Being called with successful capture but `processHop()` never reaches the publish phase

---

## 2. Observed Symptoms

### 2.1 Primary Symptom
Serial output every 2 seconds:
```
[WARN] Audio unavailable: seq=0 prevSeq=0 age_s=X.XXX staleness_s=0.100 hop_seq=0
```

### 2.2 Symptom Analysis

| Field | Value | Meaning |
|-------|-------|---------|
| `seq` | 0 | `SnapshotBuffer::ReadLatest()` returns 0 - no data ever published |
| `prevSeq` | 0 | Previous read also returned 0 - confirms no change over time |
| `age_s` | Variable | Computed age between last ControlBus timestamp and render time |
| `staleness_s` | 0.100 | Configured threshold (100ms) from `AudioContractTuning` |
| `hop_seq` | 0 | `ControlBusFrame.hop_seq` field is 0 - frame was never populated |

### 2.3 What This Tells Us

The `SnapshotBuffer` sequence counter starts at 0 and increments with each `Publish()` call:

```cpp
// From firmware/v2/src/audio/contracts/SnapshotBuffer.h:52
m_seq.fetch_add(1U, std::memory_order_release);
```

**`seq=0` means `Publish()` was NEVER called.** The buffer is in its initial state.

---

## 3. System Architecture Overview

### 3.1 Actor System

LightwaveOS uses a FreeRTOS-based Actor system with the following relevant actors:

| Actor | Core | Priority | Tick Interval | Purpose |
|-------|------|----------|---------------|---------|
| RendererActor | 1 | 5 | 8ms (120 FPS) | LED output, effect rendering |
| AudioActor | 0 | 4 | 8ms (125 Hz) | Audio capture, DSP, ControlBus publish |

### 3.2 Data Flow

```
┌─────────────────────────────────────────────────────────────────────────┐
│ Core 0: AudioActor                                                       │
│                                                                          │
│  ┌──────────┐    ┌───────────┐    ┌────────────┐    ┌────────────────┐  │
│  │ I2S/DMA  │───▶│ captureHop│───▶│ processHop │───▶│ControlBusBuffer│  │
│  │ ES8311   │    │           │    │ (DSP)      │    │ .Publish()     │  │
│  └──────────┘    └───────────┘    └────────────┘    └───────┬────────┘  │
│                                                              │           │
└──────────────────────────────────────────────────────────────┼───────────┘
                                                               │
                        SnapshotBuffer (lock-free)             │
                                                               ▼
┌──────────────────────────────────────────────────────────────┬───────────┐
│ Core 1: RendererActor                                        │           │
│                                                              │           │
│  ┌────────────────┐    ┌─────────────┐    ┌──────────────┐  │           │
│  │ControlBusBuffer│───▶│ Freshness   │───▶│ AudioContext │───▶│ Effects │
│  │ .ReadLatest()  │    │ Check       │    │              │  │           │
│  └────────────────┘    └─────────────┘    └──────────────┘  │           │
│                                                              │           │
└──────────────────────────────────────────────────────────────────────────┘
```

### 3.3 Key Files

| File | Purpose |
|------|---------|
| `src/audio/AudioActor.cpp` | Audio capture orchestration, DSP pipeline, publish |
| `src/audio/AudioCapture.cpp` | I2S driver configuration, DMA read, ES8311 codec init |
| `src/audio/contracts/SnapshotBuffer.h` | Lock-free double buffer for cross-core data sharing |
| `src/core/actors/Actor.cpp` | Base Actor class, FreeRTOS task, tick dispatch |
| `src/core/actors/RendererActor.cpp` | Reads ControlBusBuffer, determines audio availability |
| `src/config/audio_config.h` | Audio timing constants, pin assignments |
| `src/config/chip_esp32p4.h` | ESP32-P4 specific GPIO and peripheral config |

---

## 4. Detailed Code Path Analysis

### 4.1 Actor Tick Mechanism

The Actor base class (`Actor.cpp:274-377`) implements the main loop:

```cpp
void Actor::run()
{
    m_running = true;
    onStart();  // Derived class initialization

    while (!m_shutdownRequested) {
        // ... queue drain logic omitted ...

        TickType_t waitTime;
        if (m_config.tickInterval > 0) {
            waitTime = m_config.tickInterval;
        } else {
            waitTime = portMAX_DELAY;
        }

        BaseType_t received = xQueueReceive(m_queue, &msg, waitTime);

        if (received == pdTRUE) {
            // Handle message
            onMessage(msg);
        } else {
            // Timeout - call onTick if configured
            if (m_config.tickInterval > 0) {
                onTick();  // <-- THIS IS WHERE AUDIO PROCESSING HAPPENS
            }
        }
    }

    onStop();
    m_running = false;
}
```

**Key insight:** `onTick()` is called when `xQueueReceive()` times out after `waitTime` ticks.

### 4.2 AudioActor Configuration

From `src/audio/AudioActor.h:775-784`:

```cpp
inline actors::ActorConfig Audio() {
    return actors::ActorConfig(
        "Audio",                                    // name
        AUDIO_ACTOR_STACK_WORDS,                    // stackSize (16KB)
        AUDIO_ACTOR_PRIORITY,                       // priority (4)
        AUDIO_ACTOR_CORE,                           // coreId (0)
        16,                                         // queueSize
        pdMS_TO_TICKS(AUDIO_ACTOR_TICK_MS)          // tickInterval
    );
}
```

From `src/config/audio_config.h:82-94` (ESP32-P4 branch):

```cpp
#if CHIP_ESP32_P4
constexpr uint16_t SAMPLE_RATE = 16000;       // 16 kHz
constexpr uint16_t HOP_SIZE = 128;            // 8ms hop @ 16kHz = 125 Hz frames
#else
constexpr uint16_t SAMPLE_RATE = 12800;       // 12.8 kHz
constexpr uint16_t HOP_SIZE = 256;            // 20ms hop @ 12.8kHz = 50 Hz frames
#endif

constexpr float HOP_DURATION_MS = (HOP_SIZE * 1000.0f) / SAMPLE_RATE;  // 8ms on P4
```

And line 167:
```cpp
constexpr uint16_t AUDIO_ACTOR_TICK_MS = static_cast<uint16_t>(HOP_DURATION_MS + 0.5f);
```

**For ESP32-P4:**
- `HOP_DURATION_MS = 128 * 1000 / 16000 = 8.0ms`
- `AUDIO_ACTOR_TICK_MS = 8`

### 4.3 FreeRTOS Tick Math

From `sdkconfig:2637`:
```
CONFIG_FREERTOS_HZ=100
```

This means:
- 1 FreeRTOS tick = 10ms
- `pdMS_TO_TICKS(8) = 8 * 100 / 1000 = 0 ticks`

**CRITICAL ISSUE:** With `CONFIG_FREERTOS_HZ=100`, `pdMS_TO_TICKS(8)` returns **0**.

When `waitTime = 0` in `xQueueReceive()`:
- If queue is empty, returns immediately with `pdFALSE`
- This triggers `onTick()` immediately
- Creates a tight loop calling `onTick()` as fast as possible

This is NOT a "tick never fires" problem - it's a "tick fires too fast" problem that could cause other issues (CPU starvation, watchdog triggers, etc.).

### 4.4 AudioActor::onTick() Flow

From `src/audio/AudioActor.cpp:406-471`:

```cpp
void AudioActor::onTick()
{
    // Skip if not in running state
    if (m_state != AudioActorState::RUNNING) {
        return;  // <-- EARLY RETURN #1: State check
    }

    m_stats.tickCount++;

    // DEBUG: Log first few ticks
    static uint32_t s_tickDbgCount = 0;
    s_tickDbgCount++;
    if (s_tickDbgCount <= 5 || (s_tickDbgCount % 1250) == 0) {
        LW_LOGI("AudioActor tick #%lu (state=%d)", (unsigned long)s_tickDbgCount, static_cast<int>(m_state));
    }

    // Capture one hop of audio
    captureHop();  // <-- CAPTURE HAPPENS HERE

    // ... rest of tick ...
}
```

### 4.5 AudioActor::onStart() - State Transition

From `src/audio/AudioActor.cpp:345-376`:

```cpp
void AudioActor::onStart()
{
    LW_LOGI("AudioActor starting on Core %d", xPortGetCoreID());

    m_state = AudioActorState::INITIALIZING;

    // Initialize diagnostics
    m_diag.reset();
    m_diag.diagStartTimeUs = esp_timer_get_time();

    // Initialize I2S audio capture
    if (!m_capture.init()) {
        LW_LOGE("Failed to initialize audio capture");
        m_state = AudioActorState::ERROR;  // <-- STATE BECOMES ERROR
        return;  // <-- EARLY RETURN, onTick() will skip
    }

    m_state = AudioActorState::RUNNING;  // <-- ONLY SET IF INIT SUCCEEDS

    // ... rest of initialization ...
}
```

**If `m_capture.init()` fails, `m_state` becomes `ERROR` and `onTick()` returns early at line 409.**

### 4.6 AudioCapture::init() - ESP32-P4 Branch

From `src/audio/AudioCapture.cpp:60-104`:

```cpp
bool AudioCapture::init()
{
    if (m_initialized) {
        LW_LOGW("Already initialized");
        return true;
    }

#if defined(CHIP_ESP32_P4) && CHIP_ESP32_P4
    LW_LOGI("Initializing I2S (std driver, ESP32-P4)");
#else
    LW_LOGI("Initializing I2S (legacy driver with >>10 shift)");
#endif

    if (!configureI2S()) {
        LW_LOGE("Failed to configure I2S driver");
        return false;  // <-- FAILURE PATH
    }

#if defined(CHIP_ESP32_P4) && CHIP_ESP32_P4
    esp_err_t err = i2s_channel_enable(m_rxChannel);
    if (err != ESP_OK) {
        LW_LOGE("Failed to enable I2S: %s", esp_err_to_name(err));
        return false;  // <-- FAILURE PATH
    }
#endif

    m_initialized = true;
    LW_LOGI("I2S initialized (std driver, ES8311)");
    // ... logging ...

    return true;
}
```

### 4.7 AudioCapture::configureI2S() - ESP32-P4 Branch

From `src/audio/AudioCapture.cpp:306-408`:

```cpp
bool AudioCapture::configureI2S()
{
#if defined(CHIP_ESP32_P4) && CHIP_ESP32_P4
    // 1. Create I2S channel
    i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chanCfg.auto_clear = true;
    chanCfg.dma_frame_num = HOP_SIZE;
    chanCfg.dma_desc_num = 4;

    esp_err_t err = i2s_new_channel(&chanCfg, nullptr, &m_rxChannel);
    if (err != ESP_OK) {
        LW_LOGE("i2s_new_channel failed: %s", esp_err_to_name(err));
        return false;  // <-- FAILURE POINT 1
    }

    // 2. Configure I2S standard mode
    i2s_std_config_t stdCfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = static_cast<gpio_num_t>(chip::gpio::I2S_MCLK),  // GPIO 13
            .bclk = static_cast<gpio_num_t>(chip::gpio::I2S_BCLK),  // GPIO 12
            .ws = static_cast<gpio_num_t>(chip::gpio::I2S_LRCL),    // GPIO 10
            .dout = static_cast<gpio_num_t>(chip::gpio::I2S_DOUT),  // GPIO 9
            .din = static_cast<gpio_num_t>(chip::gpio::I2S_DIN),    // GPIO 11
            // ...
        },
    };
    stdCfg.clk_cfg.mclk_multiple = static_cast<i2s_mclk_multiple_t>(I2S_MCLK_MULTIPLE);
    stdCfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    err = i2s_channel_init_std_mode(m_rxChannel, &stdCfg);
    if (err != ESP_OK) {
        LW_LOGE("i2s_channel_init_std_mode failed: %s", esp_err_to_name(err));
        return false;  // <-- FAILURE POINT 2
    }

    // 3. Configure ES8311 codec via I2C
    // ... GPIO config for AUDIO_PA_EN ...

    // 4. Initialize I2C for ES8311
    i2c_config_t es_i2c_cfg = {};
    es_i2c_cfg.sda_io_num = static_cast<gpio_num_t>(chip::gpio::I2C_SDA);  // GPIO 7
    es_i2c_cfg.scl_io_num = static_cast<gpio_num_t>(chip::gpio::I2C_SCL);  // GPIO 8
    // ...

    err = i2c_param_config(I2C_NUM_0, &es_i2c_cfg);
    if (err != ESP_OK) {
        LW_LOGE("i2c_param_config failed: %s", esp_err_to_name(err));
        return false;  // <-- FAILURE POINT 3
    }

    err = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        LW_LOGE("i2c_driver_install failed: %s", esp_err_to_name(err));
        return false;  // <-- FAILURE POINT 4
    }

    // 5. Create and initialize ES8311 codec handle
    static es8311_handle_t s_es8311 = nullptr;
    if (s_es8311 == nullptr) {
        s_es8311 = es8311_create(I2C_NUM_0, ES8311_ADDRRES_0);
    }
    if (s_es8311 == nullptr) {
        LW_LOGE("es8311_create failed");
        return false;  // <-- FAILURE POINT 5
    }

    const es8311_clock_config_t es_clk = {
        .mclk_inverted = false,
        .sclk_inverted = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency = static_cast<int>(SAMPLE_RATE * I2S_MCLK_MULTIPLE),
        .sample_frequency = static_cast<int>(SAMPLE_RATE),
    };
    err = es8311_init(s_es8311, &es_clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
    if (err != ESP_OK) {
        LW_LOGE("es8311_init failed: %s", esp_err_to_name(err));
        return false;  // <-- FAILURE POINT 6
    }

    // ... more ES8311 config ...

    return true;
#else
    // ESP32-S3 legacy driver path
    // ...
#endif
}
```

### 4.8 RendererActor Audio Availability Check

From `src/core/actors/RendererActor.cpp:1020-1070`:

```cpp
bool audioAvailable = false;
if (m_controlBusBuffer != nullptr) {
    // 1. Read latest ControlBusFrame BY VALUE
    uint32_t seq = m_controlBusBuffer->ReadLatest(m_lastControlBus);

    // Store previous sequence BEFORE updating
    uint32_t prevSeq = m_lastControlBusSeq;

    // ... time extrapolation ...

    // 5. Compute freshness
    float age_s = audio::AudioTime_SecondsBetween(m_lastControlBus.t, render_now);
    float staleness_s = m_audioContractTuning.audioStalenessMs / 1000.0f;
    bool sequence_changed = (seq != prevSeq);
    bool age_within_tolerance = (age_s >= -0.01f && age_s < staleness_s);
    audioAvailable = sequence_changed || age_within_tolerance;
    
    // Debug logging every 2 seconds
    if (nowDbg - lastAudioDbg >= 2000) {
        if (!audioAvailable) {
            LW_LOGW("Audio unavailable: seq=%u prevSeq=%u age_s=%.3f staleness_s=%.3f hop_seq=%u",
                    seq, prevSeq, age_s, staleness_s, m_lastControlBus.hop_seq);
        }
    }
}
```

---

## 5. Attempted Fixes and Why They Failed

### 5.1 FreeRTOS Tick Rate Hypothesis

**Hypothesis:** `CONFIG_FREERTOS_HZ=100` combined with `pdMS_TO_TICKS(8)` = 0 ticks causes `onTick()` to never fire.

**Why it was pursued:**
- Initial assumption was that 0-tick timeout would mean "wait forever" like `portMAX_DELAY`

**Why it's WRONG:**
- `xQueueReceive(queue, &msg, 0)` with timeout=0 is a **non-blocking** call
- If queue is empty, it returns immediately with `pdFALSE`
- This triggers the `else` branch which calls `onTick()`
- Result: `onTick()` is called in a tight loop, NOT never

**Proposed fix that was NOT the solution:**
```c
// sdkconfig change
CONFIG_FREERTOS_HZ=1000  // Gives 1ms tick resolution
```

This would change `pdMS_TO_TICKS(8)` from 0 to 8, but:
1. It doesn't fix the actual problem
2. The actual problem is upstream (init failure or capture failure)

### 5.2 Why "Simple Fix" Was Rejected

The user correctly pushed back on applying the tick rate change without verifying it was the actual root cause. The investigation then continued to trace the actual data flow.

---

## 6. Root Cause Analysis

### 6.1 Confirmed Facts

1. **`seq=0` means no publish ever happened** - The SnapshotBuffer sequence starts at 0 and increments on each `Publish()` call
2. **`hop_seq=0` means ControlBusFrame was never populated** - This field is set during `processHop()`
3. **The publish happens in `processHop()`** - Line 1222: `m_controlBusBuffer.Publish(frameToPublish)`

### 6.2 Possible Root Causes (In Order of Likelihood)

#### Cause A: AudioCapture::init() Fails (MOST LIKELY)

The ESP32-P4 audio path requires:
1. I2S channel creation with new ESP-IDF 5.x `i2s_std` driver
2. I2C initialization for ES8311 codec control
3. ES8311 codec initialization and configuration

Any failure in this chain causes `m_capture.init()` to return `false`, which sets `m_state = AudioActorState::ERROR`, causing `onTick()` to return early without processing.

**Evidence needed:** Check serial output for:
- `"Initializing I2S (std driver, ESP32-P4)"` - init started
- `"i2s_new_channel failed"` - I2S channel creation failed
- `"i2s_channel_init_std_mode failed"` - I2S config failed
- `"i2c_param_config failed"` - I2C config failed
- `"es8311_create failed"` - Codec handle creation failed
- `"es8311_init failed"` - Codec initialization failed
- `"I2S initialized (std driver, ES8311)"` - SUCCESS

#### Cause B: I2S Read Fails / Times Out

If init succeeds but `captureHop()` always fails:

From `AudioCapture.cpp:128-168`:
```cpp
esp_err_t err = i2s_channel_read(m_rxChannel, m_dmaBuffer, expectedBytes, &bytesRead, timeout);

if (err == ESP_ERR_TIMEOUT) {
    m_stats.dmaTimeouts++;
    return CaptureResult::DMA_TIMEOUT;
}
```

If every capture times out or errors, `processHop()` is never called, so `Publish()` never happens.

**Evidence needed:** Check serial output for:
- `"I2S read #N: result=..."` - First 5 captures are logged
- DMA timeout count in periodic diagnostics

#### Cause C: processHop() Never Reaches Publish

Even if capture succeeds, if `processHop()` has an early return or crash before line 1222, no publish happens.

The publish is guarded by:
```cpp
// Line 1201-1202
if (!m_silentMode) {
    ControlBusFrame frameToPublish = m_controlBus.GetFrame();
    // ... populate ...
    m_controlBusBuffer.Publish(frameToPublish);
}
```

If `m_silentMode` is true, publish is skipped.

#### Cause D: AudioActor Task Never Starts

If `m_audio->start()` fails in `ActorSystem::start()`, the task never runs.

From `ActorSystem.cpp:183-189`:
```cpp
if (m_audio && !m_audio->start()) {
    ESP_LOGE(TAG, "Failed to start AudioActor");
    ESP_LOGW(TAG, "Continuing without audio sync");
}
```

Note: Audio failure is non-fatal, so the system continues.

**Evidence needed:** Check serial output for:
- `"[Audio] Started on core 0"` - Task started
- `"Failed to start AudioActor"` - Task failed to start

### 6.3 Most Probable Root Cause

**ES8311 codec initialization failure** is the most likely cause because:

1. The ESP32-P4 uses an onboard ES8311 audio codec (unlike ESP32-S3 which uses direct I2S MEMS mic)
2. ES8311 requires I2C communication to configure
3. Multiple initialization steps can fail silently or with non-obvious errors
4. The Waveshare board may have different ES8311 I2C address or wiring than expected

---

## 7. Diagnostic Checklist

When the device is connected, check serial output for these messages **in order**:

### Stage 1: Actor System Startup
- [ ] `"[ActorSys] Starting actors..."` - ActorSystem::start() called
- [ ] `"[Actor] [Audio] Started on core 0 (priority=4, stack=16384)"` - Task created

### Stage 2: AudioActor Initialization
- [ ] `"AudioActor starting on Core 0"` - onStart() called
- [ ] `"Initializing I2S (std driver, ESP32-P4)"` - init() started

### Stage 3: I2S Configuration
- [ ] NOT `"i2s_new_channel failed"` - Channel created OK
- [ ] NOT `"i2s_channel_init_std_mode failed"` - Config applied OK
- [ ] `"I2S std configured: DMA frame=128 samples, descs=4"` - I2S ready

### Stage 4: ES8311 Codec
- [ ] NOT `"i2c_param_config failed"` - I2C configured
- [ ] NOT `"i2c_driver_install failed"` - I2C driver ready
- [ ] NOT `"es8311_create failed"` - Codec handle created
- [ ] NOT `"es8311_init failed"` - Codec initialized
- [ ] `"I2S initialized (std driver, ES8311)"` - Full audio init complete

### Stage 5: AudioActor Running
- [ ] `"AudioActor started (tick=8ms, hop=128, rate=125.0Hz)"` - Ready to capture
- [ ] `"AudioActor tick #1 (state=3)"` - First tick (state=3 is RUNNING)
- [ ] `"I2S read #1: result=ESP_OK, got 256 bytes"` - First capture succeeded

### Stage 6: Audio Integration
- [ ] `"Audio integration enabled - ControlBus + TempoTracker"` - Renderer wired up

---

## 8. Recommended Resolution Path

### Step 1: Gather Evidence

Connect the device and capture full boot log. Search for the diagnostic messages above.

### Step 2: Based on Evidence

**If init fails at I2S level:**
- Verify GPIO assignments in `chip_esp32p4.h` match actual board
- Check if I2S peripheral is available (not used by other component)

**If init fails at I2C level:**
- Verify I2C GPIO (7/8) are correct for Waveshare board
- Check for I2C bus conflicts (other devices on same bus)

**If init fails at ES8311 level:**
- Verify ES8311 I2C address (`ES8311_ADDRRES_0` = 0x18)
- Check MCLK frequency calculation: `16000 * 384 = 6.144 MHz`
- Verify ES8311 is receiving MCLK from I2S peripheral

**If init succeeds but captures fail:**
- Check DMA buffer sizing
- Verify I2S clock configuration matches ES8311 expectations
- Check for clock domain issues

### Step 3: Potential Code Fixes

1. **Add more diagnostic logging** to `configureI2S()` to identify exact failure point
2. **Verify ES8311 I2C address** by scanning I2C bus
3. **Check MCLK configuration** - ES8311 may need specific MCLK/LRCK ratio
4. **Consider tick rate change** as secondary fix if 0-tick is causing CPU starvation

---

## Appendix: Key Constants (ESP32-P4)

```cpp
// Audio timing
SAMPLE_RATE = 16000           // 16 kHz
HOP_SIZE = 128                // samples per hop
HOP_DURATION_MS = 8.0         // ms per hop
HOP_RATE_HZ = 125.0           // hops per second
AUDIO_ACTOR_TICK_MS = 8       // Actor tick interval

// GPIO (Waveshare ESP32-P4)
I2S_BCLK = 12
I2S_DIN = 11
I2S_DOUT = 9
I2S_LRCL = 10
I2S_MCLK = 13
I2C_SDA = 7
I2C_SCL = 8
AUDIO_PA_EN = 53

// ES8311
I2C_ADDRESS = 0x18 (ES8311_ADDRRES_0)
MCLK_MULTIPLE = 384
MCLK_FREQUENCY = 6,144,000 Hz (16000 * 384)

// FreeRTOS
CONFIG_FREERTOS_HZ = 100      // 10ms tick
pdMS_TO_TICKS(8) = 0          // !! Resolves to 0 ticks !!
```

---

*Document generated from code analysis session. Last updated based on firmware state at time of analysis.*
