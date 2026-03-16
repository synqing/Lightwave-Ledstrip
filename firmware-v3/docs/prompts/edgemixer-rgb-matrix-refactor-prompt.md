# EdgeMixer RGB Matrix Refactor — CC Agent Prompt

## Context

You are working in `firmware-v3/` of an ESP32-S3 LED controller (LightwaveOS). The project uses PlatformIO with two build environments — **these are the only builds used:**

- `esp32dev_audio_esv11_32khz` — V1 device
- `esp32dev_audio_esv11_k1v2_32khz` — K1v2 device

Do NOT build or reference `esp32dev_audio_pipelinecore`. It is not used.

### The Problem

`EdgeMixer.h` applies per-pixel colour transforms to Strip 2 for Light Guide Plate depth differentiation. The current implementation uses a lossy HSV round-trip:

```
rgb2hsv_approximate(pixel) → modify hue/sat → hsv2rgb_rainbow(result)
```

This destroys colour fidelity for effects that build colours via additive float-precision accumulation (K1 Waveform, K1 Bloom). Specifically:

1. **`rgb2hsv_approximate`** produces unreliable hue on blended/non-primary colours and garbage hue on low-saturation pixels
2. **`hsv2rgb_rainbow`** uses piecewise linear interpolation that doesn't perfectly invert `rgb2hsv_approximate`
3. The COMPLEMENTARY mode's `scale8(hsv.sat, 217)` compounds with the round-trip to produce 42% saturation loss instead of the intended 15%

Evaluation data (13,200 frames across 2 devices) confirms: saturation ratio 0.58 in COMPLEMENTARY mode against a 0.70 acceptance threshold.

### The Solution

Replace the HSV round-trip with a **3×3 linear RGB transformation matrix**. Hue rotation in RGB space is a rotation around the (1,1,1) grey axis. Desaturation is a linear blend toward the luminance-weighted grey point. Both operations compose into a single 3×3 matrix — pre-computed at config time, applied per-pixel as 9 integer multiply-adds.

**Benefits:**
- Deterministic: same input always produces same output (no approximate conversion noise)
- Faster: ~33 cycles/pixel vs ~86 cycles/pixel (HSV round-trip)
- Preserves luminance: RGB desaturation maintains perceived brightness (HSV desaturation doesn't)
- Composable: hue rotation × desaturation = single matrix, computed once

## Mathematics

### Hue Rotation Matrix

A hue rotation by angle θ around the (1,1,1) axis in RGB space:

```
        ⎡ cosθ + (1−cosθ)/3          (1−cosθ)/3 − √(1/3)·sinθ   (1−cosθ)/3 + √(1/3)·sinθ ⎤
H(θ) =  ⎢ (1−cosθ)/3 + √(1/3)·sinθ   cosθ + (1−cosθ)/3          (1−cosθ)/3 − √(1/3)·sinθ ⎥
        ⎣ (1−cosθ)/3 − √(1/3)·sinθ   (1−cosθ)/3 + √(1/3)·sinθ   cosθ + (1−cosθ)/3         ⎦
```

Where √(1/3) ≈ 0.57735.

### Desaturation Matrix

Desaturation by factor s (1.0 = full colour, 0.0 = greyscale) using perceived luminance weights (Lr=0.299, Lg=0.587, Lb=0.114):

```
        ⎡ s + (1−s)·Lr    (1−s)·Lg       (1−s)·Lb     ⎤
D(s) =  ⎢ (1−s)·Lr        s + (1−s)·Lg   (1−s)·Lb     ⎥
        ⎣ (1−s)·Lr        (1−s)·Lg       s + (1−s)·Lb  ⎦
```

### Combined Matrix

For modes that apply both hue rotation AND desaturation (COMPLEMENTARY, SPLIT_COMPLEMENTARY):

```
M = D(s) × H(θ)
```

Standard 3×3 matrix multiplication. Computed once at config time with floating point, stored as Q8.8 fixed-point (`int16_t[9]`).

### Pre-Computed Verification Values

Use these to validate your implementation. All values are the float matrix entries (before Q8.8 conversion):

**ANALOGOUS at spread=30° (θ = 30°):**
```
cos(30°) = 0.86603,  sin(30°) = 0.50000
H = [[ 0.9107, -0.2440,  0.3333],
     [ 0.3333,  0.9107, -0.2440],
     [-0.2440,  0.3333,  0.9107]]
```
No desaturation. M = H.

Test vector: (255, 0, 0) → (232, 85, 0). Warm orange-red shift. ✓

**COMPLEMENTARY (θ = 180°, s = 217/255 = 0.8510):**
```
cos(180°) = -1.0,  sin(180°) = 0.0
H = [[-0.3333,  0.6667,  0.6667],
     [ 0.6667, -0.3333,  0.6667],
     [ 0.6667,  0.6667, -0.3333]]

D(0.851) = [[ 0.8955,  0.0875,  0.0170],
            [ 0.0446,  0.9385,  0.0170],
            [ 0.0446,  0.0875,  0.8680]]

M = D × H =
    [[-0.2289,  0.5792,  0.6497],
     [ 0.6221, -0.2718,  0.6497],
     [ 0.6221,  0.5792, -0.2013]]
```

Test vector: (255, 0, 0) → (0, 159, 159). Desaturated cyan. ✓

**SPLIT_COMPLEMENTARY (θ = 150°, s = 230/255 = 0.9020):**
```
cos(150°) = -0.86603,  sin(150°) = 0.50000
H = [[-0.2440,  0.3333,  0.9107],
     [ 0.9107, -0.2440,  0.3333],
     [ 0.3333,  0.9107, -0.2440]]

D(0.902) = [[ 0.9313,  0.0575,  0.0112],
            [ 0.0293,  0.9596,  0.0112],
            [ 0.0293,  0.0575,  0.9133]]

M = D × H =
    [[-0.1711,  0.3066,  0.8646],
     [ 0.8704, -0.2142,  0.3438],
     [ 0.3496,  0.8273, -0.1769]]
```

Test vector: (255, 0, 0) → (0, 222, 89). Green-teal shift. ✓

**SATURATION_VEIL at spread=30 (m_satScale=140, s = 140/255 = 0.5490):**
```
No hue rotation. M = D(0.549) =
    [[ 0.6839,  0.2647,  0.0514],
     [ 0.1348,  0.8137,  0.0514],
     [ 0.1348,  0.2647,  0.6004]]
```

Test vector: (255, 0, 0) → (174, 34, 34). Desaturated red. ✓
Test vector: (0, 255, 0) → (68, 208, 68). Hue preserved, desaturated. ✓

### Q8.8 Fixed-Point Encoding

Store each matrix entry as `int16_t`: `entry_q8 = (int16_t)roundf(float_entry * 256.0f)`

Per-pixel application:
```cpp
int16_t r_out = (m[0] * r + m[1] * g + m[2] * b + 128) >> 8;
int16_t g_out = (m[3] * r + m[4] * g + m[5] * b + 128) >> 8;
int16_t b_out = (m[6] * r + m[7] * g + m[8] * b + 128) >> 8;

// Clamp to [0, 255]
result.r = (r_out < 0) ? 0 : ((r_out > 255) ? 255 : (uint8_t)r_out);
```

The `+ 128` before the shift is the rounding bias (equivalent to adding 0.5 before truncation).

**Intermediate overflow check:** Maximum intermediate value = 256 × 255 × 3 = 195,840. This exceeds int16_t range (32,767). **You MUST use int32_t for the accumulation**, then shift and clamp:

```cpp
int32_t r_out = ((int32_t)m[0] * r + (int32_t)m[1] * g + (int32_t)m[2] * b + 128) >> 8;
```

This is critical. Using int16_t for the accumulation WILL overflow and produce garbage.

## Exact Changes Required

### File: `src/effects/enhancement/EdgeMixer.h`

This is the ONLY file you modify. Nothing else.

#### Change 1: Remove FastLED HSV includes if no longer needed

Check whether `rgb2hsv_approximate` or `hsv2rgb_rainbow` are used anywhere else in the file. If not, no include change is needed (FastLED.h provides both RGB and HSV types).

#### Change 2: Add matrix storage and computation

Add to the private member section (after `m_rmsSmooth`):

```cpp
// Pre-computed 3×3 colour transform matrix in Q8.8 fixed-point.
// Combines hue rotation + desaturation for the current mode/spread.
// Computed at config time (setMode/setSpread), applied per-pixel.
int16_t m_matrix[9];  // Row-major: [r_from_r, r_from_g, r_from_b, g_from_r, ...]
bool m_matrixDirty;   // True when mode or spread changed, matrix needs recomputing
```

Add a private method to compute the matrix:

```cpp
/**
 * @brief Recompute the 3×3 RGB transform matrix for current mode and spread
 *
 * Called when mode or spread changes. Uses float maths (acceptable —
 * config changes are human-interaction-rate, not per-frame).
 */
void recomputeMatrix() {
    // Start with identity
    float mat[9] = {1,0,0, 0,1,0, 0,0,1};

    // Hue rotation angle in radians
    float theta = 0.0f;
    float satRetain = 1.0f;  // 1.0 = no desaturation

    switch (m_mode) {
        case EdgeMixerMode::ANALOGOUS:
            theta = static_cast<float>(m_spreadDegrees) * (M_PI / 180.0f);
            break;
        case EdgeMixerMode::COMPLEMENTARY:
            theta = M_PI;  // 180°
            satRetain = 217.0f / 255.0f;  // 0.851
            break;
        case EdgeMixerMode::SPLIT_COMPLEMENTARY:
            theta = 150.0f * (M_PI / 180.0f);
            satRetain = 230.0f / 255.0f;  // 0.902
            break;
        case EdgeMixerMode::SATURATION_VEIL:
            // No hue rotation, desaturation only
            satRetain = static_cast<float>(m_satScale) / 255.0f;
            break;
        case EdgeMixerMode::MIRROR:
        default:
            // Identity — but MIRROR early-returns in process(), so this
            // shouldn't be reached. Set identity anyway for safety.
            break;
    }

    // Build hue rotation matrix H(θ)
    if (theta != 0.0f) {
        const float cosT = cosf(theta);
        const float sinT = sinf(theta);
        const float oneMinusCos = 1.0f - cosT;
        const float third = oneMinusCos / 3.0f;
        const float sqrt13 = 0.57735026919f;  // √(1/3)
        const float sinTerm = sqrt13 * sinT;

        mat[0] = cosT + third;           // H[0][0]
        mat[1] = third - sinTerm;        // H[0][1]
        mat[2] = third + sinTerm;        // H[0][2]
        mat[3] = third + sinTerm;        // H[1][0]
        mat[4] = cosT + third;           // H[1][1]
        mat[5] = third - sinTerm;        // H[1][2]
        mat[6] = third - sinTerm;        // H[2][0]
        mat[7] = third + sinTerm;        // H[2][1]
        mat[8] = cosT + third;           // H[2][2]
    }

    // Apply desaturation: M = D(s) × H
    if (satRetain < 1.0f) {
        // Perceived luminance weights (ITU-R BT.601)
        const float Lr = 0.299f;
        const float Lg = 0.587f;
        const float Lb = 0.114f;
        const float inv = 1.0f - satRetain;

        // D(s) matrix entries
        float d[9] = {
            satRetain + inv * Lr,   inv * Lg,               inv * Lb,
            inv * Lr,               satRetain + inv * Lg,   inv * Lb,
            inv * Lr,               inv * Lg,               satRetain + inv * Lb
        };

        // M = D × mat (3×3 multiply)
        float combined[9];
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                combined[row * 3 + col] =
                    d[row * 3 + 0] * mat[0 * 3 + col] +
                    d[row * 3 + 1] * mat[1 * 3 + col] +
                    d[row * 3 + 2] * mat[2 * 3 + col];
            }
        }
        memcpy(mat, combined, sizeof(mat));
    }

    // Convert to Q8.8 fixed-point
    for (int i = 0; i < 9; ++i) {
        m_matrix[i] = static_cast<int16_t>(roundf(mat[i] * 256.0f));
    }
    m_matrixDirty = false;
}
```

#### Change 3: Replace `computeTransform()`

Remove the entire current `computeTransform()` method (lines 318-348) and replace with:

```cpp
/**
 * @brief Apply pre-computed 3×3 RGB transform matrix to a pixel
 *
 * No HSV round-trip. Deterministic, luminance-preserving, ~33 cycles/pixel.
 * Matrix is pre-computed at config time via recomputeMatrix().
 */
CRGB computeTransform(const CRGB& pixel) const {
    const int32_t r = pixel.r;
    const int32_t g = pixel.g;
    const int32_t b = pixel.b;

    // Q8.8 matrix multiply with rounding
    int32_t r_out = (m_matrix[0] * r + m_matrix[1] * g + m_matrix[2] * b + 128) >> 8;
    int32_t g_out = (m_matrix[3] * r + m_matrix[4] * g + m_matrix[5] * b + 128) >> 8;
    int32_t b_out = (m_matrix[6] * r + m_matrix[7] * g + m_matrix[8] * b + 128) >> 8;

    // Clamp to [0, 255]
    CRGB result;
    result.r = (r_out < 0) ? 0 : ((r_out > 255) ? 255 : static_cast<uint8_t>(r_out));
    result.g = (g_out < 0) ? 0 : ((g_out > 255) ? 255 : static_cast<uint8_t>(g_out));
    result.b = (b_out < 0) ? 0 : ((b_out > 255) ? 255 : static_cast<uint8_t>(b_out));
    return result;
}
```

#### Change 4: Trigger matrix recomputation on config changes

Modify `setMode()`:
```cpp
void setMode(EdgeMixerMode mode) {
    m_mode = mode;
    recomputeMatrix();
}
```

Modify `setSpread()` — after the existing `m_satScale` computation, add:
```cpp
recomputeMatrix();
```

#### Change 5: Initialise matrix in constructor and loadFromNVS

In the constructor initialiser list, add:
```cpp
, m_matrixDirty(true)
```
And add to the constructor body (or at the end of the initialiser):
```cpp
recomputeMatrix();
```

In `loadFromNVS()`, add `recomputeMatrix();` at the end (after all fields are loaded, before `prefs.end()`).

#### Change 6: Lazy recompute guard in process()

At the top of `process()`, after the MIRROR early-return but before the per-pixel loop, add:

```cpp
if (m_matrixDirty) recomputeMatrix();
```

This is a safety net — `recomputeMatrix()` should already have been called by setMode/setSpread/loadFromNVS, but the lazy guard costs one branch per frame (negligible) and prevents stale matrix usage.

### What You Remove

1. The entire body of the old `computeTransform()` (the `rgb2hsv_approximate` → switch → `hsv2rgb_rainbow` path)
2. The `#include <Preferences.h>` stays (still needed for NVS)
3. The `#include <FastLED.h>` stays (still needed for CRGB, blend, scale8)

### What You Do NOT Change

1. **The `process()` loop structure** — the per-pixel loop, near-black skip (maxC < 2), spatial mask, temporal gating, and blend logic are all untouched
2. **The temporal stage** (`computeTemporal`, `kTemporalFloor`) — untouched
3. **The spatial stage** (`computeSpatial`, `kCentreGradient` LUT) — untouched
4. **NVS field layout** — same 5 fields, same keys, same namespace
5. **The enum definitions** — all 5 modes, 2 spatial, 2 temporal unchanged
6. **The singleton pattern** — unchanged
7. **No other files.** Do NOT modify RendererActor.cpp, ActorSystem.cpp, Actor.h, AudioActor.cpp, any effect files, or any WebSocket/REST handlers. The external interface is identical.
8. **Do NOT modify AudioActor.cpp.** The smoothing coefficients are correct v3 pipeline calibration. Read `docs/prompts/CORRECTION-audioactor-revert-cancelled.md` if you need context on why.

## Verification

### 1. Build Check

```bash
cd firmware-v3
pio run -e esp32dev_audio_esv11_32khz 2>&1 | tail -5
pio run -e esp32dev_audio_esv11_k1v2_32khz 2>&1 | tail -5
```

Both must compile with zero errors.

### 2. Matrix Value Verification

Add a temporary serial print in `recomputeMatrix()` (remove before final commit) that prints the 9 matrix entries for each mode. Verify against these expected Q8.8 values:

| Mode | m[0] | m[1] | m[2] | m[3] | m[4] | m[5] | m[6] | m[7] | m[8] |
|------|------|------|------|------|------|------|------|------|------|
| ANALOGOUS (30°) | 233 | -62 | 85 | 85 | 233 | -62 | -62 | 85 | 233 |
| COMPLEMENTARY | -59 | 148 | 166 | 159 | -70 | 166 | 159 | 148 | -52 |
| SPLIT_COMP | -44 | 78 | 221 | 223 | -55 | 88 | 90 | 212 | -45 |
| SAT_VEIL (spread=30) | 175 | 68 | 13 | 35 | 208 | 13 | 35 | 68 | 154 |

Tolerance: ±2 on any entry (float rounding). If any entry differs by more than 2, the matrix computation is wrong.

### 3. Test Vector Verification

Apply the matrix to known input colours and verify output:

**COMPLEMENTARY mode:**
- (255, 0, 0) → (0, 159, 159) ±3 per channel
- (0, 255, 0) → (148, 0, 148) ±3 per channel
- (0, 0, 255) → (166, 166, 0) ±3 per channel
- (128, 128, 128) → (128, 128, 128) ±1 (grey preserved)
- (255, 255, 255) → (255, 255, 255) ±1 (white preserved)
- (0, 0, 0) → (0, 0, 0) (black preserved exactly)

**SATURATION_VEIL mode (spread=30):**
- (255, 0, 0) → (174, 34, 34) ±3 per channel
- (0, 255, 0) → (68, 208, 68) ±3 per channel (should NOT shift hue — green stays green)
- (128, 128, 128) → (128, 128, 128) ±1 (grey preserved)

### 4. Diff Review

Run `git diff src/effects/enhancement/EdgeMixer.h` and verify:
- `computeTransform()` body is entirely replaced (no `rgb2hsv_approximate` or `hsv2rgb_rainbow` calls remain)
- `m_matrix[9]` and `m_matrixDirty` added as members
- `recomputeMatrix()` added as private method
- `setMode()` and `setSpread()` call `recomputeMatrix()`
- Constructor and `loadFromNVS()` call `recomputeMatrix()`
- No changes to `process()` loop, temporal, spatial, or blend logic
- No other files changed

### 5. No HSV Functions Remain

```bash
grep -n "rgb2hsv_approximate\|hsv2rgb_rainbow" src/effects/enhancement/EdgeMixer.h
```

Must return zero lines. The entire HSV round-trip path is eliminated.

### 6. Performance Estimate

The new per-pixel cost is 9 int32 multiplies + 6 int32 adds + 3 clamps ≈ 33 cycles on ESP32-S3. For 160 pixels: ~5,280 cycles = ~22μs at 240 MHz. The old path was ~86 cycles/pixel = ~57μs. This is a 2.6× speedup.

## Pass/Fail Criteria

- **PASS:** Both builds clean. No `rgb2hsv_approximate` or `hsv2rgb_rainbow` in EdgeMixer.h. Matrix values match reference table ±2. Test vectors match ±3 per channel. Grey and white preserved ±1. No files modified other than EdgeMixer.h.
- **FAIL:** Any build error. Any HSV function remaining. Any matrix value off by >2. Any test vector off by >3 per channel. Grey/white not preserved. Any file other than EdgeMixer.h modified.

## Deliverables

1. The refactored `src/effects/enhancement/EdgeMixer.h` with the 3×3 RGB matrix engine
2. Build output confirming both `esp32dev_audio_esv11_32khz` and `esp32dev_audio_esv11_k1v2_32khz` compile clean
3. `git diff` output for review
4. Matrix value printout for the 4 non-MIRROR modes (can be from serial log or computed inline)
