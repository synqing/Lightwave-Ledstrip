# ESP32-P4 Audio Pipeline Cadence Fix

**Date:** February 2026  
**Status:** RESOLVED  
**Platform:** ESP32-P4 (Waveshare ESP32-P4-WIFI6)  
**Audio Codec:** ES8311 via I2S  

---

## Executive Summary

The ESP32-P4 audio pipeline was experiencing instability including DMA timeouts, watchdog triggers, and erratic capture rates. The root cause was a **fundamental mismatch between the configured hop rate (125 Hz) and FreeRTOS scheduler tick rate (100 Hz)**. This document captures the problems encountered, attempted solutions, the final fix, and lessons learned.

---

## The Problem

### Symptoms Observed
1. **DMA timeouts** during I2S reads
2. **Watchdog triggers** (task_wdt) when using self-clocked mode
3. **Erratic capture rates** oscillating between 90-130 Hz
4. **High spike detection warnings** (avg > 5 spikes/frame)
5. **Sequence gaps** in hop tracking
6. **IDLE0 task starvation** on Core 0

### Root Cause Analysis

The original configuration:
- **Sample rate:** 16,000 Hz
- **Hop size:** 128 samples
- **Expected hop duration:** 128 / 16000 = **8 ms**
- **Expected hop rate:** **125 Hz**

The FreeRTOS scheduler:
- **Tick rate:** 100 Hz
- **Tick duration:** **10 ms**

**The Mismatch:**
- Audio DMA fills a 128-sample buffer every 8 ms
- The AudioActor tick runs every 10 ms (one FreeRTOS tick)
- Result: The actor can't keep up with the DMA → backlog → timeouts

### Failed Attempts

1. **tickInterval=0 (self-clocked mode):** Caused watchdog triggers because the tight loop starved IDLE0 task on Core 0. The actor blocked on I2S reads with no yield points.

2. **Multi-hop catch-up logic:** Tried to read multiple hops per tick when behind. This created complex state management and still couldn't match 125 Hz with a 100 Hz scheduler.

3. **Short timeout with fallback:** Tried 5ms timeouts with early returns. This caused inconsistent frame timing and data gaps.

---

## The Solution

### Final Configuration (Locked In)

```c
// audio_config.h - ESP32-P4 section
constexpr uint16_t SAMPLE_RATE = 16000;  // 16 kHz (ES8311)
constexpr uint16_t HOP_SIZE = 160;       // 10ms hop @ 16kHz = 100 Hz frames
```

**Math:**
- 160 samples / 16000 Hz = **10 ms per hop**
- 10 ms = **1 FreeRTOS tick** at 100 Hz
- **Hop rate = Scheduler rate = 100 Hz** ✓

### Why This Works

1. **No timing mismatch:** One tick = one hop = one I2S read
2. **No catch-up needed:** Data is consumed at exactly the rate it's produced
3. **No starvation:** Normal blocking I2S reads with proper timeouts
4. **Stable metrics:** ~97-99 Hz actual rate (scheduler jitter only)

### Code Changes Made

| File | Change |
|------|--------|
| `audio_config.h` | Changed HOP_SIZE from 128 to 160 |
| `AudioActor.h` | Added `LW_MS_TO_TICKS_CEIL_MIN1()` macro for proper tick conversion |
| `AudioActor.cpp` | Reverted tickInterval to normal (was 0), raised spike warning threshold from 5.0 to 10.0 |
| `AudioCapture.cpp` | Fixed DC blocker coefficient (0.992176), added ES8311 microphone gain (24dB) |
| `ControlBus.cpp` | Added noise floor check to skip spike detection on quiet signals |
| `Actor.cpp` | Added tickInterval=0 self-clocked mode support (retained but not used) |

---

## Additional Fixes Applied

### 1. DC Blocker Coefficient Correction

**Before:** `DC_BLOCK_ALPHA = 0.9922f` (arbitrary)

**After:** `DC_BLOCK_ALPHA = 0.992176f` (calculated correctly)

**Formula:** `R = exp(-2π × fc / fs)` where fc=20Hz, fs=16000Hz

The DC blocker is a first-order IIR high-pass filter:
```
y[n] = x[n] - x[n-1] + R × y[n-1]
```

### 2. ES8311 Microphone Gain

Signal levels were observed at ~0.2% of full scale (`raw=[-45..64]`, `rms=0.0008`), approximately -54dB below full scale.

**Fix:** Added explicit gain setting after ES8311 initialization:
```c
es8311_microphone_gain_set(s_es8311, ES8311_MIC_GAIN_24DB);
```

Available gains: 0dB, 6dB, 12dB, 18dB, 24dB, 30dB, 36dB, 42dB

### 3. Spike Detection Noise Floor Check

The spike detector was counting direction changes in noise-floor signals, causing false "high spike rate" warnings.

**Fix:** Skip spike detection when all 3 frames in the lookahead buffer are below noise floor (0.005 normalized):
```c
constexpr float SPIKE_NOISE_FLOOR = 0.005f;  // ~-46dB

if (oldest_val < SPIKE_NOISE_FLOOR &&
    middle_val < SPIKE_NOISE_FLOOR &&
    newest_val < SPIKE_NOISE_FLOOR) {
    continue;  // Skip spike detection for noise-floor signals
}
```

### 4. Spike Warning Threshold Adjustment

**Before:** Warning at > 5.0 spikes/frame

**After:** Warning at > 10.0 spikes/frame

**Rationale:** With 20 bins checked per frame (8 bands + 12 chroma), 5 direction changes is normal, especially with transient audio content.

### 5. I2C Probe Fix for ES8311

The ES8311 I2C probe was failing intermittently because `i2c_master_write_read_device()` with 0-length read causes issues on some chips.

**Fix:** Changed to low-level ACK check using `i2c_cmd_link` API:
```c
auto i2c_probe = [](uint8_t addr) -> esp_err_t {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);
    return ret;
};
```

---

## Verification Metrics

After applying fixes, the system shows:

| Metric | Value | Status |
|--------|-------|--------|
| Capture success rate | 100% | ✓ |
| Actual hop rate | ~97-99 Hz | ✓ (expect 100 Hz) |
| DMA timeouts | 0 | ✓ |
| Sequence gaps | 0 | ✓ |
| Watchdog triggers | None | ✓ |
| Process time avg | ~7 ms | ✓ (10 ms budget) |

---

## Remaining Considerations

### 1. Signal Level Tuning
The 24dB gain may need adjustment based on actual microphone placement and audio source levels. Monitor `raw=[]` ranges in serial output.

### 2. 125 Hz Option (Future)
If 125 Hz hop rate is required for finer tempo resolution:
- **Proper solution:** Decouple audio capture from actor ticks
- Create dedicated high-priority capture task that blocks on I2S
- Use queue/ring-buffer to pass hops to processing actor
- This removes scheduler coupling entirely

For music visualization, 100 Hz is standard and adequate.

---

## Lessons Learned

1. **Match your data rate to your scheduler rate** - Fighting the OS tick granularity is futile. Align hop size to scheduler reality.

2. **Don't spin in real-time tasks** - Self-clocked tight loops starve system tasks. Always ensure yield points.

3. **Verify math end-to-end** - The DC blocker coefficient was wrong because someone copied a value without deriving it.

4. **Low signal levels cause cascading issues** - Spike detection false positives, AGC hunting, etc. Fix gain staging early.

5. **FreeRTOS tick conversion is tricky** - `pdMS_TO_TICKS(8)` returns 0 at 100Hz ticks because 8 < 10. Always use ceiling division with minimum of 1.

---

## Files Modified (Complete List)

```
firmware/v2/src/config/audio_config.h          # HOP_SIZE 128→160
firmware/v2/src/audio/AudioActor.h             # LW_MS_TO_TICKS_CEIL_MIN1 macro
firmware/v2/src/audio/AudioActor.cpp           # Spike threshold 5→10, debug prints
firmware/v2/src/audio/AudioCapture.cpp         # DC blocker 0.992176, ES8311 gain 24dB, I2C probe fix
firmware/v2/src/audio/contracts/ControlBus.cpp # Noise floor check in spike detection
firmware/v2/src/core/actors/Actor.cpp          # tickInterval=0 self-clocked mode support
firmware/v2/src/core/system/ValidationProfiler.cpp # Disabled periodic reports
firmware/v2/esp-idf-p4/main/main.cpp           # Removed printSerialCommands() call
```

---

---

## Runtime Microphone Gain API

The ES8311 microphone gain is now exposed as a runtime-tunable parameter via REST API and WebSocket.

### REST API

**GET** `/api/v1/audio/mic-gain`
```json
{
  "success": true,
  "data": {
    "gainDb": 24,
    "supported": true,
    "validValues": [0, 6, 12, 18, 24, 30, 36, 42]
  }
}
```

**POST** `/api/v1/audio/mic-gain`
```json
{
  "gainDb": 30
}
```
Response:
```json
{
  "success": true,
  "data": {
    "gainDb": 30
  }
}
```

### WebSocket Commands

**Get current gain:**
```json
{"cmd": "audio.mic-gain.get", "requestId": "123"}
```
Response:
```json
{"type": "audio.mic-gain.state", "requestId": "123", "data": {"gainDb": 24, "supported": true, "validValues": [0,6,12,18,24,30,36,42]}}
```

**Set gain:**
```json
{"cmd": "audio.mic-gain.set", "gainDb": 30, "requestId": "124"}
```
Response:
```json
{"type": "audio.mic-gain.updated", "requestId": "124", "data": {"gainDb": 30}}
```

### Valid Gain Values

| Value | Description |
|-------|-------------|
| 0 dB  | No gain (codec default) |
| 6 dB  | 2x amplification |
| 12 dB | 4x amplification |
| 18 dB | 8x amplification |
| 24 dB | 16x amplification (default) |
| 30 dB | 32x amplification |
| 36 dB | 64x amplification |
| 42 dB | 128x amplification |

### Platform Support

This API is only functional on ESP32-P4 with ES8311 codec. On ESP32-S3 (or other platforms), the API returns:
```json
{
  "gainDb": -1,
  "supported": false,
  "reason": "Microphone gain control only available on ESP32-P4 with ES8311 codec"
}
```

---

## References

- ESP-IDF I2S Standard Mode: https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32p4/api-reference/peripherals/i2s.html
- ES8311 Datasheet: Audio codec with programmable microphone gain
- DC Blocker Theory: Julius O. Smith, "Introduction to Digital Filters"
