# Serial Frame Capture System

## Overview

The Serial Frame Capture system enables real-time extraction of LED frame data at three pipeline stages for analysis and validation of dithering algorithms.

## Architecture

### Capture Taps

```
Effect Render → TAP_A → Color Correction → TAP_B → FastLED.show() → TAP_C
                 ↓                           ↓                        ↓
              PRE-CORRECTION           POST-CORRECTION           PRE-WS2812
           (before gamma/Bayer)       (after gamma/Bayer)    (after all processing)
```

| Tap | Name | Location | Purpose |
|-----|------|----------|---------|
| **TAP_A** | PRE_CORRECTION | After `renderFrame()` | Raw effect output |
| **TAP_B** | POST_CORRECTION | After `ColorCorrectionEngine` | After gamma/Bayer |
| **TAP_C** | PRE_WS2812 | Before `FastLED.show()` | Before temporal dither |

**Note**: TAP_C captures the buffer state just before FastLED applies its temporal dithering at the driver level.

---

## Firmware Implementation

### 1. Add Serial Output Header

**File**: `firmware/v2/src/core/actors/SerialFrameOutput.h` (created)

This header provides:
- `SerialFrameOutput::sendFrame()` - Send single frame
- `SerialFrameOutput::sendCapturedFrame()` - Send from RendererNode buffer
- `SerialFrameOutput::streamFrames()` - Continuous streaming

### 2. Serial Commands (Add to main.cpp)

```cpp
#include "core/actors/SerialFrameOutput.h"

// In serial command handler:
else if (cmd.startsWith("capture ")) {
    String subcmd = cmd.substring(8);
    subcmd.trim();
    
    if (subcmd == "on") {
        // Enable all taps
        renderer->setCaptureMode(true, 0x07);
        Serial.println("Frame capture enabled (all taps)");
    }
    else if (subcmd.startsWith("on ")) {
        // Enable specific taps
        int tapMask = subcmd.substring(3).toInt();
        renderer->setCaptureMode(true, tapMask);
        Serial.printf("Frame capture enabled (tap mask: 0x%02X)\n", tapMask);
    }
    else if (subcmd == "off") {
        renderer->setCaptureMode(false, 0);
        Serial.println("Frame capture disabled");
    }
    else if (subcmd == "status") {
        Serial.printf("Capture mode: %s\n", 
                     renderer->isCaptureModeEnabled() ? "ENABLED" : "DISABLED");
        auto meta = renderer->getCaptureMetadata();
        Serial.printf("Effect: %d, Palette: %d, Frame: %u\n",
                     meta.effectId, meta.paletteId, meta.frameIndex);
    }
    else if (subcmd.startsWith("send ")) {
        // Send single captured frame
        int tapId = subcmd.substring(5).toInt();
        auto tap = static_cast<RendererNode::CaptureTap>(tapId);
        SerialFrameOutput::sendCapturedFrame(renderer, tap);
        Serial.println("Frame sent");
    }
    else if (subcmd.startsWith("stream ")) {
        // Stream frames continuously
        int tapId = subcmd.substring(7).toInt();
        auto tap = static_cast<RendererNode::CaptureTap>(tapId);
        
        Serial.println("Streaming frames (press 'q' to stop)...");
        SerialFrameOutput::streamFrames(renderer, tap, 0);  // Infinite
    }
    else {
        Serial.println("Usage:");
        Serial.println("  capture on [tap_mask] - Enable (0x01=A, 0x02=B, 0x04=C)");
        Serial.println("  capture off           - Disable");
        Serial.println("  capture status        - Show state");
        Serial.println("  capture send <tap>    - Send single frame (0=A, 1=B, 2=C)");
        Serial.println("  capture stream <tap>  - Stream continuously");
    }
}
```

### 3. Integration Points

The `captureFrame()` method in `RendererNode.cpp` already stores frames in buffers. To add serial output, modify it:

```cpp
void RendererNode::captureFrame(CaptureTap tap, const CRGB* sourceBuffer) {
    if (sourceBuffer == nullptr) {
        return;
    }
    
    // Update metadata
    m_captureMetadata.effectId = m_currentEffect;
    m_captureMetadata.paletteId = m_paletteIndex;
    m_captureMetadata.brightness = m_brightness;
    m_captureMetadata.speed = m_speed;
    m_captureMetadata.frameIndex = m_frameCount;
    m_captureMetadata.timestampUs = micros();
    
    // Copy frame data (existing code)
    switch (tap) {
        case CaptureTap::TAP_A_PRE_CORRECTION:
            memcpy(m_captureTapA, sourceBuffer, sizeof(CRGB) * LedConfig::TOTAL_LEDS);
            m_captureTapAValid = true;
            break;
        case CaptureTap::TAP_B_POST_CORRECTION:
            memcpy(m_captureTapB, sourceBuffer, sizeof(CRGB) * LedConfig::TOTAL_LEDS);
            m_captureTapBValid = true;
            break;
        case CaptureTap::TAP_C_PRE_WS2812:
            memcpy(m_captureTapC, sourceBuffer, sizeof(CRGB) * LedConfig::TOTAL_LEDS);
            m_captureTapCValid = true;
            break;
    }
    
    // NEW: Send via serial if enabled
    if (m_serialOutputEnabled) {
        SerialFrameOutput::sendFrame(tap, sourceBuffer, m_captureMetadata);
    }
}
```

Add member variable to `RendererNode.h`:
```cpp
bool m_serialOutputEnabled = false;  // Control via command
```

---

## Python Receiver Usage

### Basic Capture

```bash
cd tools/dither_bench

# Capture 10 seconds of frames
python serial_frame_capture.py \
  --port /dev/ttyUSB0 \
  --output captured_frames/test_run \
  --duration 10

# Captured frames saved as:
# captured_frames/test_run/frame_000001_TAP_A_PRE_CORRECTION.npz
# captured_frames/test_run/frame_000001_TAP_B_POST_CORRECTION.npz
# captured_frames/test_run/session_info.json
```

### Firmware Serial Commands

```
# Enable capture (all taps)
capture on

# Enable specific taps (bitmask: 0x01=A, 0x02=B, 0x04=C)
capture on 3    # Enable TAP_A and TAP_B only

# Check status
capture status

# Send single frame manually
capture send 0  # Send TAP_A
capture send 1  # Send TAP_B

# Stream continuously (Ctrl+C to stop)
capture stream 1  # Stream TAP_B

# Disable capture
capture off
```

---

## Frame Format Details

### Binary Header (16 bytes)

```python
# Python unpacking:
sync1, sync2 = struct.unpack('BB', data[0:2])     # 0xFF, 0xFE
tap_id, reserved = struct.unpack('BB', data[2:4])
frame_count = struct.unpack('<I', data[4:8])[0]   # Little-endian uint32
led_count = struct.unpack('<H', data[8:10])[0]    # Little-endian uint16

# Metadata (6 bytes)
effect_id = data[10]
palette_id = data[11]
brightness = data[12]
speed = data[13]
timestamp_us = struct.unpack('<I', data[12:16])[0]
```

### RGB Data (960 bytes for 320 LEDs)

```python
# Sequential RGB triplets
rgb_data = data[16:16+led_count*3]
frame = np.frombuffer(rgb_data, dtype=np.uint8).reshape((led_count, 3))
```

---

## Performance Considerations

### Serial Bandwidth

**At 115200 baud**:
- Theoretical: 11,520 bytes/sec
- Frame size: 16 (header) + 960 (RGB) = 976 bytes
- Max FPS: ~11.8 FPS

**At 921600 baud** (if supported):
- Theoretical: 92,160 bytes/sec
- Max FPS: ~94 FPS

### Recommendations

1. **Use selective tap capture**: Only enable TAP_B (post-correction) for most analysis
2. **Reduce frame rate**: Set `TARGET_FPS` to 30 during capture to avoid buffer overruns
3. **Increase baud rate**: Add to `platformio.ini`:
   ```ini
   monitor_speed = 921600
   ```

4. **Triggered capture**: Only capture N frames on command, not continuously

---

## Analysis Workflow

### 1. Capture Frames from Hardware

```bash
# On firmware serial console:
> capture on 2      # Enable TAP_B only
> <switch to effect you want to capture>

# On Python receiver:
python serial_frame_capture.py \
  --port /dev/ttyUSB0 \
  --baudrate 115200 \
  --output hardware_captures/effect_42 \
  --duration 5

# Back to firmware:
> capture off
```

### 2. Generate Simulation Frames

```bash
# Run DitherBench simulation
python run_bench.py \
  --output simulation_runs/effect_42_sim \
  --frames 150 \
  --seed 123
```

### 3. Compare Hardware vs Simulation

```python
import numpy as np

# Load hardware capture
hw_frame = np.load('hardware_captures/effect_42/frame_000100_TAP_B_POST_CORRECTION.npz')
hw_rgb = hw_frame['rgb']

# Load simulation result
sim_frame = np.load('simulation_runs/effect_42_sim/frames.npz')
sim_rgb = sim_frame['lwos_bayer']

# Compute difference
diff = np.abs(hw_rgb.astype(np.int16) - sim_rgb.astype(np.int16))
mae = np.mean(diff)
max_diff = np.max(diff)

print(f"MAE: {mae:.2f}")
print(f"Max difference: {max_diff}")
```

---

## Troubleshooting

### "No frames received"
- Check baud rate matches (115200 default)
- Verify `capture on` command was sent
- Ensure firmware is rendering (not paused)
- Check serial port permissions

### "Sync errors"
- Serial buffer overrun (reduce FPS or increase baud rate)
- Other serial output interfering (disable debug prints)
- Cable/connection issues

### "Frame corruption"
- Baud rate mismatch
- Flow control issues (try adding CTS/RTS)
- Buffer overrun (add delays between frames)

---

## Example: Validate Bayer Dithering

```bash
# 1. Capture hardware frames
python serial_frame_capture.py \
  --port /dev/ttyUSB0 \
  --output hw_bayer_test \
  --duration 2

# 2. Extract TAP_A and TAP_B for comparison
python -c "
import numpy as np
import matplotlib.pyplot as plt

# Load pre and post correction
pre = np.load('hw_bayer_test/frame_000050_TAP_A_PRE_CORRECTION.npz')['rgb']
post = np.load('hw_bayer_test/frame_000050_TAP_B_POST_CORRECTION.npz')['rgb']

# Compute Bayer delta
delta = post.astype(np.int16) - pre.astype(np.int16)

# Visualize
plt.figure(figsize=(12, 4))
plt.subplot(131)
plt.imshow(pre[:160, :].T, aspect='auto', interpolation='nearest')
plt.title('TAP_A (Pre-Correction)')
plt.subplot(132)
plt.imshow(post[:160, :].T, aspect='auto', interpolation='nearest')
plt.title('TAP_B (Post-Correction)')
plt.subplot(133)
plt.imshow(delta[:160, :].T, aspect='auto', interpolation='nearest', cmap='RdBu')
plt.title('Bayer Delta')
plt.savefig('bayer_validation.png')
print('✓ Saved to bayer_validation.png')
"
```

---

## Next Steps

1. **Add SerialFrameOutput.h to firmware** (file created above)
2. **Add serial commands to main.cpp** (implementation provided above)
3. **Test with Python receiver**: `serial_frame_capture.py` (ready to use)
4. **Compare hardware vs simulation** using DitherBench

The system is designed to be **non-blocking** in capture mode (buffers internally) and **blocking** in stream mode (for controlled analysis).