# WS2812 Dithering Research & Recommendations

**Date**: 2026-01-10  
**Context**: Research into WS2812/RGBIC dithering strategies for LightwaveOS v2  
**Scope**: Temporal, ordered, and error-diffusion dithering techniques at 120 FPS

---

## Executive Summary

### Key Findings

1. **FastLED Temporal Dithering** is a well-established technique that increases perceived bit depth by rapidly toggling LED states between adjacent brightness values. Effective at low brightness, but can introduce visible flicker at low refresh rates or when viewed by cameras.

2. **Bayer Ordered Dithering** (spatial) is complementary to temporal dithering, breaking up gradient banding with fixed spatial patterns. Used widely in displays and printers.

3. **Combined Dithering** (temporal + spatial) is **standard practice** in high-end displays and can provide superior results to either method alone, PROVIDED they are properly synchronized and tuned for the target refresh rate.

4. **WS2812-Specific Considerations**:
   - PWM frequency and refresh rate impact flicker visibility
   - Camera artefacts (rolling shutter banding) are common with temporal dithering
   - Some WS2812 variants have built-in gamma correction (but inconsistent)
   - DMA-based control enables more sophisticated dithering without CPU overhead

### Recommendations for LightwaveOS

| Recommendation | Priority | Rationale |
|---------------|----------|-----------|
| **Keep both dithering methods** | HIGH | Complementary techniques, standard in professional displays |
| **Add runtime toggle for FastLED temporal** | MEDIUM | Enable A/B testing and camera-friendly mode |
| **Synchronize dither patterns** | LOW | Current independent operation is likely fine at 120 FPS |
| **Add blue-noise temporal option** | FUTURE | Superior to random noise, but requires research/implementation |
| **Expose tap points for validation** | HIGH | Already implemented (TAP A/B/C), document and test |

---

## Research Findings

### 1. FastLED Temporal Dithering

#### How It Works

FastLED's `setDither(1)` enables **temporal dithering**, which:
- Adds small random adjustments (+1, 0, or -1) to each RGB channel before output
- Varies the adjustment pattern each frame using an internal frame counter
- Over multiple frames (typically 2-4), the average brightness converges to the intended value
- Effectively increases perceived color depth from 8-bit (256 levels) to ~10-11 bit (1024-2048 levels)

#### Benefits

- **Low brightness accuracy**: At dim levels (0-20), where 8-bit quantization is most visible, temporal dithering smooths gradients significantly
- **Zero spatial artefacts**: Unlike ordered dithering, temporal dithering doesn't introduce fixed patterns
- **CPU-efficient**: FastLED implements this in hardware-accelerated RMT driver

#### Limitations

- **Flicker**: At low refresh rates (<60 FPS), the frame-to-frame changes may be visible as shimmer or flicker
- **Camera artefacts**: Rolling shutter cameras capture different dither states across the frame, producing banding
- **Power budget**: Temporal dithering increases average pixel brightness slightly (more "on" time)

**Source**: [FastLED Temporal Dithering Wiki](https://github.com/FastLED/FastLED/wiki/FastLED-Temporal-Dithering)

#### LightwaveOS Context

- **Current state**: Always enabled via `FastLED.setDither(1)` in `RendererNode::initLeds()`
- **Refresh rate**: 120 FPS target → flicker should be imperceptible to human vision
- **Camera concern**: Users filming LED effects may see rolling shutter banding

---

### 2. Bayer Ordered Dithering (Spatial)

#### How It Works

Bayer dithering uses a **4×4 threshold matrix** to decide whether to round pixel values up or down:

```
Bayer 4×4 matrix (0-15):
[ 0,  8,  2, 10]
[12,  4, 14,  6]
[ 3, 11,  1,  9]
[15,  7, 13,  5]
```

For each pixel at position (x, y):
- Get threshold = matrix[x % 4][y % 4]
- If (pixel_value & 0x0F) > threshold, round up

This creates a fixed spatial pattern that breaks up smooth gradients.

#### Benefits

- **Temporal stability**: Pattern is fixed in space, no frame-to-frame changes
- **Camera-friendly**: No rolling shutter artefacts (pattern is static)
- **Predictable**: Always produces the same output for a given input and position

#### Limitations

- **Visible pattern**: On slow gradients, the 4×4 tiling may be perceptible
- **Resolution-dependent**: Pattern size is fixed (4 pixels), may not scale well to different LED densities

**Source**: Classic image processing technique, used in early displays and printers

#### LightwaveOS Context

- **Current state**: Enabled by default in `ColorCorrectionEngine::applyDithering()`
- **Pipeline position**: AFTER gamma, BEFORE FastLED output
- **Gating**: Can be disabled via `ColorCorrectionConfig.ditheringEnabled = false`

---

### 3. Combined Dithering (Temporal + Spatial)

#### Industry Practice

**Cinema projectors**, **OLED TVs**, and **professional LED panels** commonly use BOTH:
- Spatial dithering (ordered or blue-noise) breaks up 8-bit banding
- Temporal dithering smooths residual artefacts and increases effective bit depth

#### Interaction Modes

| Mode | Description | Visual Result |
|------|-------------|---------------|
| **Complementary** | Spatial handles large-scale banding, temporal handles fine quantization | Best of both worlds |
| **Redundant** | Both fix the same problem, one is unnecessary | Wasted compute, no visual improvement |
| **Conflicting** | Spatial pattern interferes with temporal noise | Visible artefacts, worse than either alone |

#### Critical Success Factor

**Synchronization is key**:
- If spatial dithering is too aggressive, temporal adjustments may "fight" the fixed pattern
- If temporal dithering is too aggressive, it may override spatial pattern completely
- Optimal balance depends on refresh rate, viewing distance, and content

#### LightwaveOS Context

At **120 FPS**, temporal dithering operates at 8.3ms intervals → fast enough that human vision averages the frames seamlessly. The fixed Bayer pattern provides a baseline structure, while temporal noise fills in sub-pixel detail.

**Hypothesis**: Current implementation is likely **complementary**, but needs empirical validation.

---

### 4. WS2812-Specific Considerations

#### PWM Frequency and Dithering

WS2812 LEDs use internal PWM at ~400 Hz to control brightness. Some research suggests implementing **8+8 dithered mode**:
- High-frequency PWM for 8 most significant bits (MSB)
- Distribute remaining 8 LSB across sub-segments within the PWM cycle
- Achieves "16-bit look" without increasing data rate

**Source**: [PWM Dithering for High Color Resolution](https://www.linkedin.com/pulse/pwm-dithering-high-color-resolution-flicker-free-led-light-lumissil-iskpc)

**Applicability to LightwaveOS**: This is a **hardware-level** technique, not implementable in firmware without custom LED controllers. WS2812 chips are fixed-function.

#### Gamma Correction Variants

Some WS2812 clones (e.g., SK6812) claim to have integrated gamma correction. Testing reveals:
- **Inconsistent** across batches and manufacturers
- **Not replaceable** - if present, it's baked into the chip
- **LightwaveOS solution**: Apply software gamma correction BEFORE dithering (already implemented)

**Source**: [Does the WS2812 Have Integrated Gamma Correction?](https://cpldcpu.com/2022/08/15/does-the-ws2812-have-integrated-gamma-correction/)

#### DMA-Based Control

Using Direct Memory Access (DMA) to drive WS2812 data streams offloads CPU and enables:
- More complex dithering algorithms (e.g., blue-noise)
- Higher refresh rates without blocking
- Parallel processing (render while transmitting)

**Current LightwaveOS**: Uses FastLED's RMT driver, which is DMA-accelerated on ESP32-S3 ✅

**Source**: [Using DMA to Drive WS2812 LEDs](https://hackaday.com/2013/11/22/using-dma-to-drive-ws2812-led-pixels/)

---

### 5. Advanced Dithering Techniques

#### Blue-Noise Temporal Dithering

Instead of **random noise** (white noise spectrum), use **blue-noise** (high-frequency bias):
- Blue-noise pushes quantization errors into high spatial frequencies (less visible)
- Requires pre-computed blue-noise texture or real-time generator
- Used in high-end displays (e.g., OLED, HDR monitors)

**Benefits**:
- Superior to random temporal dithering (FastLED's approach)
- Smoother gradients, fewer visible artefacts

**Challenges**:
- Requires 2D blue-noise texture (storage cost: ~64-256 bytes for tiling texture)
- Slightly more CPU cost to look up noise values

**Recommendation**: **FUTURE** enhancement after validating current dithering performance.

#### Error-Diffusion Dithering (Floyd-Steinberg)

Propagates quantization errors to neighboring pixels:
- Superior to ordered dithering for **static images**
- Requires **sequential processing** (cannot parallelize)
- **Temporal instability**: Pattern changes with content, may cause flicker

**Applicability to LightwaveOS**: ❌ **Not suitable** for real-time 120 FPS rendering. Too slow, causes temporal artefacts.

#### FadeCandy Dithering

The FadeCandy project (Micah Scott, Scanlime) implements advanced temporal dithering + color correction for WS2812:
- 16-bit internal processing
- Temporal dithering with frame interpolation
- Per-pixel color correction LUTs

**Source**: [FadeCandy Dithering Implementation](https://forum.electromage.com/t/output-expander-that-implements-fadecandy-dithering/840)

**LightwaveOS Comparison**:
- LightwaveOS already implements: ✅ Gamma LUT, ✅ Color correction, ✅ Temporal dithering
- FadeCandy advantage: 16-bit internal pipeline (vs LightwaveOS 8-bit)
- **Not worth porting**: Performance cost too high for real-time effects at 120 FPS

---

## Recommendations for LightwaveOS v2

### Immediate Actions (High Priority)

#### 1. Keep Both Dithering Methods ✅

**Rationale**:
- Temporal and spatial dithering are complementary at 120 FPS
- Industry standard practice for professional LED systems
- No evidence of conflict (requires validation via test suite)

**Action**: Document current behavior and expose runtime toggles for testing.

#### 2. Add Runtime Toggle for FastLED Temporal Dithering

**Rationale**:
- Enable A/B testing to validate complementary hypothesis
- Provide "camera mode" for users filming LED effects (disable temporal to prevent rolling shutter banding)

**Implementation**:
```cpp
// In RendererNode::initLeds() or via configuration
bool enableFastLEDTemporal = true;  // Default: enabled
FastLED.setDither(enableFastLEDTemporal ? 1 : 0);
```

**Config exposure**:
- Add to `LedConfig` or `ColorCorrectionConfig`
- Expose via REST API (`/api/v1/config/dithering`)
- Add serial command: `dither temporal on|off`

#### 3. Validate Using TAP A/B/C Capture + Test Suite

**Rationale**:
- TAP A/B/C infrastructure already exists
- Need empirical data to confirm complementary vs redundant vs conflicting

**Action**:
- Capture frames with all 4 configurations (see test matrix below)
- Run quantitative metrics (gradient banding score, flicker energy)
- Visual inspection + user testing

---

### Medium Priority

#### 4. Tune Bayer Threshold Aggressiveness

**Current**: Bayer matrix values 0-15, applied to low nibble (bits 0-3)  
**Potential**: May be too aggressive (rounding up too often) or too subtle

**Action**:
- Add runtime parameter: `bayerScale` (0.0-1.0) to scale threshold values
- Test with different LED densities and viewing distances

#### 5. Add Dithering Presets

**Rationale**: Different use cases benefit from different dithering configs

| Preset | Bayer | Temporal | Use Case |
|--------|-------|----------|----------|
| **Standard** | ON | ON | Default, best quality |
| **Camera-Friendly** | ON | OFF | Filming/photography |
| **Performance** | OFF | OFF | Maximum FPS |
| **Subtle** | OFF | ON | Minimal spatial artefacts |

**Implementation**:
- Enum `DitheringPreset` in config
- Apply preset on startup or via API

---

### Low Priority (Future Enhancements)

#### 6. Blue-Noise Temporal Dithering

**Rationale**: Superior to random noise for temporal dithering

**Requirements**:
- Pre-compute 16×16 blue-noise texture (256 bytes)
- Replace FastLED temporal with custom implementation
- Benchmark performance impact

**Risk**: FastLED temporal is hardware-accelerated (RMT); custom impl may be slower.

#### 7. Adaptive Dithering (Context-Aware)

**Rationale**: Different effects benefit from different dithering strategies

**Examples**:
- **LGP interference patterns**: Disable ALL dithering (already implemented via `shouldSkipColorCorrection()`)
- **Slow gradients**: Increase Bayer aggressiveness
- **High-speed strobes**: Disable temporal (flicker not an issue)

**Implementation**:
- Add per-effect metadata: `preferredDithering` (enum or bitmask)
- Override global config in render loop

---

## Test Matrix for Validation

| Config | Bayer | Temporal | Test Scenarios |
|--------|-------|----------|----------------|
| **A** | ON | ON | Current default (baseline) |
| **B** | OFF | ON | FastLED only |
| **C** | ON | OFF | Bayer only |
| **D** | OFF | OFF | No dithering |

### Test Scenarios

1. **Slow Gradient Ramp** (0→255 red over 160 LEDs)
   - Metric: Gradient banding score (histogram of first derivative)
   - Visual: Count visible bands

2. **Near-Black Gradient** (0→20 white over 160 LEDs)
   - Metric: Temporal flicker energy (stddev across frames)
   - Visual: Shimmer/sparkle visibility

3. **Palette Blend** (RainbowColors_p, smooth transitions)
   - Metric: Color accuracy (CIEDE2000 distance from ideal)
   - Visual: Subjective smoothness rating

4. **Camera Capture** (60fps video, 1/120s shutter)
   - Metric: Rolling shutter band visibility (FFT of horizontal scan lines)
   - Visual: Count visible artefacts

5. **Power Consumption** (sustained gradient at 50% brightness)
   - Metric: Average current draw (milliamps)
   - Compare configs A/B/C/D

---

## References

1. **FastLED Temporal Dithering**  
   https://github.com/FastLED/FastLED/wiki/FastLED-Temporal-Dithering

2. **PWM Dithering for High Color Resolution**  
   https://www.linkedin.com/pulse/pwm-dithering-high-color-resolution-flicker-free-led-light-lumissil-iskpc

3. **Using DMA to Drive WS2812 LEDs**  
   https://hackaday.com/2013/11/22/using-dma-to-drive-ws2812-led-pixels/

4. **WS2812 Gamma Correction Analysis**  
   https://cpldcpu.com/2022/08/15/does-the-ws2812-have-integrated-gamma-correction/

5. **FadeCandy Dithering**  
   https://forum.electromage.com/t/output-expander-that-implements-fadecandy-dithering/840

6. **NeoPixel Dithering with Pico**  
   https://noise.getoto.net/2021/01/22/neopixel-dithering-with-pico/

---

## Conclusion

The current LightwaveOS implementation of **combined Bayer + FastLED temporal dithering** is consistent with professional LED display standards. At 120 FPS, both methods should operate complementarily without visible conflict. However, empirical validation is required to confirm this hypothesis and optimize parameters.

**Next Steps**:
1. ✅ Document current pipeline (completed in DITHERING_AUDIT.md)
2. ⏳ Build Python test suite for quantitative analysis
3. ⏳ Run test matrix with TAP A/B/C captures
4. ⏳ Implement runtime toggles for A/B testing
5. ⏳ Make data-driven recommendation: keep, optimize, or replace

---

**End of Research Document**
