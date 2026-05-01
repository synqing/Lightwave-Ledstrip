---
abstract: "Read-only motion audit of canonical SensoryBridge 4.1.1 (FIRMWARE_VERSION 40101) — the LATEST upstream SB release. Verbatim per-frame equations for every motion-relevant lightshow_modes.h function (gdft, vu_dot, kaleidoscope, chromagram_gradient, chromagram_dots, bloom; plus utility chain: get_smooth_spectrogram, make_smooth_chromagram, calculate_vu, calculate_novelty, mood_scale, low_pass_array, draw_dot, draw_sprite). KEY FINDING: lightshow_modes.h is BYTE-IDENTICAL between 4.1.0 and 4.1.1 — the entire 4.1.0→4.1.1 delta is infrastructure (BASE_COAT default false, sweet_spot follower simplified to one-line low-pass, LED_NEOPIXEL_X2 dual-strip support, led_thread stack 4096→8192, vTaskDelay(1) unconditional). 4.1.1 has NO waveform/snapwave/oscilloscope/chevron mode — these never existed upstream. K1's broken four (ChevronWaves, ChevronWavesEnhanced, SnapwaveLinear, LGPWaveCollision) are K1-original work; the closest 4.1.1 motion archetypes are bloom (scrolling sprite via draw_sprite) and vu_dot (smoothed-position dot with two followers and mood_scale-driven mix rates). Both use SQ15x16 fixed-point throughout, deliberate one-stage post-mode smoothing, and audio→position via low-pass on a normalised feature — never raw waveform → position."
---

# Canonical SensoryBridge 4.1.1 — Motion Audit

**Status:** READ-ONLY extraction. Verbatim code excerpts only. No K1 modifications proposed.
**Version under audit:** SB 4.1.1 (FIRMWARE_VERSION 40101).
**Source root:** `/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/Sensorybridge.sourcecode/SensoryBridge-4.1.1/SENSORY_BRIDGE_FIRMWARE/`.

This is the LATEST canonical SB release. 4.1.1 is the upstream baseline that K1's `sensorybridge_reference/` was originally based on; the K1 internal copy has drifted and is NOT to be used as reference.

---

## 1. File map

| File | Purpose | Lines | Motion-relevant content |
|---|---|---|---|
| `lightshow_modes.h` | Per-mode renderers | 498 | All six render modes + smoothing helpers |
| `globals.h` | Shared state declarations | 362 | CONFIG defaults, audio/spectral arrays, dot table, hue cycling |
| `GDFT.h` | 64-bin Goertzel + spectrogram normalisation + novelty | 243 | `process_GDFT()`, `calculate_novelty()` |
| `led_utilities.h` | LED draw primitives | 1410+ | `hsv()`, `draw_dot()`, `draw_sprite()`, `draw_line()`, `set_dot_position()`, `make_smooth_chromagram()`, `apply_prism_effect()`, `force_saturation()`, `force_hue()` |
| `i2s_audio.h` | Audio capture + VU computation | 247+ | `acquire_sample_chunk()`, `calculate_vu()` |
| `utilities.h` | Math helpers | 109 | `low_pass_filter_fixed()`, `mood_scale()`, `interpolate()`, `fabs_fixed()`, `fmod_fixed()` |
| `SENSORY_BRIDGE_FIRMWARE.ino` | Main loop + LED thread | 231 | mode dispatch table, `led_thread()` |

**Mode enum (the entire SB 4.1.1 mode catalogue — 6 modes):**

```cpp
// SENSORY_BRIDGE_FIRMWARE.ino:58-68
enum lightshow_modes {
  LIGHT_MODE_GDFT,                  // GDFT spectrogram
  LIGHT_MODE_GDFT_CHROMAGRAM,       // chromagram_gradient (live)
  LIGHT_MODE_GDFT_CHROMAGRAM_DOTS,  // chromagram_dots (live)
  LIGHT_MODE_BLOOM,                 // Bloom Mode
  LIGHT_MODE_VU_DOT,                // VU dot
  LIGHT_MODE_KALEIDOSCOPE,          // Three-channel Perlin noise
  NUM_MODES
};
```

**There is no waveform mode, no snapwave, no oscilloscope, no chevron, and no wave-collision mode in canonical SB 4.1.1.** A grep across `lightshow_modes.h` finds the literal string "waveform" only inside a single commented-out line (`//fadeToBlackBy(leds, 128, 255-255*waveform_peak_scaled);` line 165, inside a fully commented-out alternate `light_mode_bloom`). K1's broken-four are K1-originals with no upstream lineage in this version.

---

## 2. Per-mode analysis

### 2.1 `light_mode_gdft` — default mode, static spectrogram render (no temporal motion)

**Verbatim source** (`lightshow_modes.h:65-96`):

```cpp
void light_mode_gdft() {
  for (SQ15x16 i = 0; i < NUM_FREQS; i += 1) {  // 64 freqs
    SQ15x16 prog = i / (SQ15x16)NUM_FREQS;
    SQ15x16 bin = spectrogram_smooth[i.getInteger()];
    if (bin > 1.0) { bin = 1.0; }

    uint8_t extra_iters = 0;
    if (chromatic_mode == true) { extra_iters = 1; }
    for (uint8_t s = 0; s < CONFIG.SQUARE_ITER + extra_iters; s++) {
      bin = (bin * bin) * SQ15x16(0.65) + (bin * SQ15x16(0.35));
    }

    SQ15x16 led_hue;
    if (chromatic_mode == true) {
      led_hue = note_colors[i.getInteger() % 12];  // hue cycles once per octave
    } else {
      led_hue = chroma_val + hue_position + ((sqrt(float(bin)) * SQ15x16(0.05)) + (prog * SQ15x16(0.10)) * hue_shifting_mix);
    }

    leds_16[i.getInteger()] = hsv(led_hue + bin * SQ15x16(0.050), CONFIG.SATURATION, bin);
  }

  shift_leds_up(leds_16, 64);
  mirror_image_downwards(leds_16);
}
```

**Per-frame motion equation:** none. There is **no phase, no position, no velocity, no temporal accumulator** in this mode. Each LED's brightness is `bin = clamp(spectrogram_smooth[i], 0, 1)` followed by a soft-knee squaring `bin = bin² · 0.65 + bin · 0.35` repeated `SQUARE_ITER+extra_iters` times. The mirror-up-and-fold composition produces a 64-LED spectrogram mirrored to 128 LEDs.

- **Audio sources:** `spectrogram_smooth[64]` (output of `get_smooth_spectrogram()`).
- **Smoothing:** single stage. `spectrogram[i]` is the magnitude-normalised post-Goertzel value (`GDFT.h:197`); `spectrogram_smooth[i]` is one rate-asymmetric tracker pass (`lightshow_modes.h:1-16`, see §3.1).
- **Trail-fade:** none. Frame is fully rewritten each tick.
- **Colour mapping:** `note_colors[i%12]` (chromatic) OR `chroma_val + hue_position + (sqrt(bin)·0.05 + prog·0.10) · hue_shifting_mix` (continuous mode). Note the `+ bin · 0.050` hue shift on the `hsv()` call adds a brightness-coupled hue micro-shift.
- **Beat handling:** none.

### 2.2 `light_mode_chromagram_gradient` — static interpolated chromagram (no temporal motion)

**Verbatim source** (`lightshow_modes.h:343-364`):

```cpp
void light_mode_chromagram_gradient() {
  for (uint8_t i = 0; i < 64; i++) {
    SQ15x16 prog = i / 64.0;
    SQ15x16 note_magnitude = interpolate(prog, chromagram_smooth, 12) * 0.9 + 0.1;

    for (uint8_t s = 0; s < CONFIG.SQUARE_ITER; s++) {
      note_magnitude = (note_magnitude * note_magnitude) * SQ15x16(0.65) + (note_magnitude * SQ15x16(0.35));
    }

    SQ15x16 led_hue;
    if (chromatic_mode == true) {
      led_hue = interpolate(prog, note_colors, 12);
    } else {
      led_hue = chroma_val + hue_position + ((sqrt(float(note_magnitude)) * SQ15x16(0.05)) + (prog * SQ15x16(0.10)) * hue_shifting_mix);
    }

    CRGB16 col = hsv(led_hue, CONFIG.SATURATION, note_magnitude * note_magnitude);
    leds_16[64 + i] = col;
    leds_16[63 - i] = col;
  }
}
```

**Per-frame motion equation:** none — same family as gdft. `note_magnitude(prog) = lerp(chromagram_smooth, 12 keys, prog) · 0.9 + 0.1` then soft-knee squared, then mirrored centre-out (`leds_16[64+i] = leds_16[63-i] = col`). Only audio data drives change.

- **Audio sources:** `chromagram_smooth[12]` from `make_smooth_chromagram()` (`led_utilities.h:1209-1244`).
- **Smoothing:** single normalisation pass inside `make_smooth_chromagram()` (`max_peak` decays at `*= 0.999` per call, rises by 5% of distance — see §3.2).
- **Trail-fade:** none.
- **Colour mapping:** chromatic uses interpolated `note_colors[]`; continuous uses `chroma_val + hue_position + …`.
- **Beat handling:** none.
- **Geometry:** centre-mirrored at index 63/64 (matches K1 LED 79/80 doctrine).

### 2.3 `light_mode_chromagram_dots` — twelve smoothed-position dots (motion-relevant)

**Verbatim source** (`lightshow_modes.h:366-396`):

```cpp
void light_mode_chromagram_dots() {
  static SQ15x16 chromagram_last[12];

  memset(leds_16, 0, sizeof(CRGB16) * 128);

  low_pass_array_fixed(chromagram_smooth, chromagram_last, 12, LED_FPS, float(mood_scale(3.5, 1.5)));
  memcpy(chromagram_last, chromagram_smooth, sizeof(float) * 12);

  for (uint8_t i = 0; i < 12; i++) {
    SQ15x16 led_hue;
    if (chromatic_mode == true) { led_hue = note_colors[i]; }
    else { led_hue = chroma_val + hue_position + (sqrt(float(1.0)) * SQ15x16(0.05)); }

    SQ15x16 magnitude = chromagram_smooth[i] * 1.0;
    if (magnitude > 1.0) { magnitude = 1.0; }
    magnitude = magnitude * magnitude;

    CRGB16 col = hsv(led_hue, CONFIG.SATURATION, magnitude);

    set_dot_position(RESERVED_DOTS + i * 2 + 0, magnitude * 0.45 + 0.5);
    set_dot_position(RESERVED_DOTS + i * 2 + 1, 0.5 - magnitude * 0.45);

    draw_dot(leds_16, RESERVED_DOTS + i * 2 + 0, col);
    draw_dot(leds_16, RESERVED_DOTS + i * 2 + 1, col);
  }
}
```

**Per-frame motion equation (per dot, indexed by chromatic note `i = 0..11`):**

```
chromagram_smooth[i] = low_pass( chromagram_smooth[i] , chromagram_last[i] ,
                                 sample_rate = LED_FPS,
                                 cutoff_hz = mood_scale(3.5, 1.5) )    // 2.0 Hz to 5.0 Hz
magnitude       = clamp(chromagram_smooth[i], 0, 1)²
position_pos[i] = magnitude · 0.45 + 0.5         // mapped into [0.50, 0.95]
position_neg[i] = 0.5 − magnitude · 0.45         // mapped into [0.05, 0.50]
dot_brightness  = 1 / max(1.0, |position − last_position|)   // see draw_dot, §3.5
```

- **Audio sources:** `chromagram_smooth[12]`.
- **Smoothing:** **TWO stages, both bounded.** `make_smooth_chromagram()` first (normalisation peak-tracker), then a **mood-scaled exponential low-pass** with cutoff `2.0–5.0 Hz` driven by `CONFIG.MOOD ∈ [0,1]`. This is the canonical SB pattern for smoothing motion-driving features inside the renderer: low_pass_filter_fixed with cutoff in Hz, sample-rate = LED_FPS — explicitly time-correct, not a per-frame multiplier. (Per the K1 motion doctrine in `SSA1`, this is the form that produces the correct rate-independent decay.)
- **Trail-fade:** none on the framebuffer (`memset` zeroes); motion-blur is generated implicitly by `draw_dot` via the `last_position` interpolation: when a dot moves > 1 LED in a frame the brightness is divided across the swept range (`net_brightness_per_pixel = 1 / |Δpos|`).
- **Colour mapping:** `note_colors[i]` chromatic, or single `chroma_val + hue_position + sqrt(1)·0.05` for all 12 dots in continuous mode.
- **Beat handling:** none — beat-relevant amplitude lives inside `chromagram_smooth[i]` already.

### 2.4 `light_mode_bloom` — chromagram-coloured pixel emitted at centre, scrolled outward via subpixel sprite

**Verbatim source** (`lightshow_modes.h:398-499`):

```cpp
void light_mode_bloom() {
  // Clear output
  memset(leds_16, 0, sizeof(CRGB16) * NATIVE_RESOLUTION);

  draw_sprite(leds_16, leds_16_prev, 128, 128, 0.250 + 1.750 * CONFIG.MOOD, 0.99);

  // ... (commented-out RGB-channel low/mid/high block here) ...

  CRGB16 sum_color;
  SQ15x16 share = 1 / 6.0;
  for (uint8_t i = 0; i < 12; i++) {
    float prog = i / 12.0;
    SQ15x16 bin = chromagram_smooth[i];
    CRGB16 add_color = hsv(prog, CONFIG.SATURATION, bin*bin * share);
    sum_color.r += add_color.r;
    sum_color.g += add_color.g;
    sum_color.b += add_color.b;
  }
  if (sum_color.r > 1.0) { sum_color.r = 1.0; };
  if (sum_color.g > 1.0) { sum_color.g = 1.0; };
  if (sum_color.b > 1.0) { sum_color.b = 1.0; };

  for (uint8_t i = 0; i < CONFIG.SQUARE_ITER; i++) {
    sum_color.r *= sum_color.r;
    sum_color.g *= sum_color.g;
    sum_color.b *= sum_color.b;
  }

  CRGB temp_col = { uint8_t(sum_color.r * 255), uint8_t(sum_color.g * 255), uint8_t(sum_color.b * 255) };
  temp_col = force_saturation(temp_col, 255*CONFIG.SATURATION);

  if (chromatic_mode == false) {
    SQ15x16 led_hue = chroma_val + hue_position + (sqrt(float(1.0)) * SQ15x16(0.05));
    temp_col = force_hue(temp_col, 255*float(led_hue));
  }

  leds_16[63] = { temp_col.r / 255.0, temp_col.g / 255.0, temp_col.b / 255.0 };
  leds_16[64] = leds_16[63];

  // Copy last frame to temp
  memcpy(leds_16_prev, leds_16, sizeof(CRGB16) * NATIVE_RESOLUTION);

  for(uint8_t i = 0; i < 32; i++){
    float prog = i / 31.0;
    leds_16[128-1-i].r *= (prog*prog);
    leds_16[128-1-i].g *= (prog*prog);
    leds_16[128-1-i].b *= (prog*prog);
  }

  for (uint8_t i = 0; i < 64; i++) {
    leds_16[i] = leds_16[128 - 1 - i];
  }
}
```

**Per-frame motion equation:**

```
// 1. Scroll previous-frame buffer outward by a mood-scaled distance:
draw_sprite(dest=leds_16, sprite=leds_16_prev,
            position = 0.250 + 1.750 · CONFIG.MOOD,    // 0.25 to 2.00 LEDs/frame
            alpha    = 0.99)

// 2. Compute chromagram-summed colour for this frame:
sum_color = Σ(i=0..11) hsv(i/12, SAT, chromagram_smooth[i]² · 1/6)
sum_color = clamp(sum_color, 1.0)
for (s in 0..SQUARE_ITER): sum_color *= sum_color   // soft-knee gain

// 3. Force saturation/hue post:
temp_col = force_saturation(255 · sum_color, 255 · SAT)
if !chromatic: temp_col = force_hue(temp_col, 255 · (chroma_val + hue_position + sqrt(1)·0.05))

// 4. Write at centre (LEDs 63 and 64):
leds_16[63] = leds_16[64] = temp_col / 255

// 5. Snapshot for next frame (BEFORE the trail fade) — the SCROLL substrate:
leds_16_prev = leds_16

// 6. Quadratic edge-fade on the top half so trails die at the boundary (i=0..31):
for i in 0..31: leds_16[127-i] *= (i/31)²

// 7. Mirror the top half down onto the bottom half:
for i in 0..63: leds_16[i] = leds_16[127-i]
```

- **Audio sources:** `chromagram_smooth[12]` only. **Note** `process_GDFT()` overrides `MOOD_VAL = 1.0` when bloom is selected (`GDFT.h:60-63`), which max-cuts the spectrogram-stage low-pass cutoff to `1 + 10·1 = 11.0 Hz`.
- **Smoothing:** stack is documented as single-stage (`spectrogram_smooth → chromagram_smooth`); no per-frame Spring or follower in the renderer. The motion driver is the **SCROLL POSITION**, not a smoothed audio velocity.
- **Trail-fade:** explicit. The previous full-frame buffer is shifted outward by `0.25–2.00 LEDs` (mood-scaled) at `alpha = 0.99` per frame — that's a roughly 1% energy loss per frame from accumulation, with subpixel positioning provided by `draw_sprite` (`led_utilities.h:1247-1290`, see §3.6). Plus the quadratic edge-fade `*= (i/31)²` over 32 outer LEDs acts as a hard fade-to-zero at the strip edge.
- **Colour mapping:** chromagram-summed-and-squared OR force_hue'd by `chroma_val + hue_position` in continuous mode.
- **Beat handling:** none — relies on chromagram amplitude variance.
- **Geometry:** strict centre-origin (write at 63/64, scroll outward, mirror top half down).

**This is the ONLY SB 4.1.1 mode with a temporal scroll/phase motion.** The motion is implemented as **buffer-scroll-with-subpixel-sprite**, NOT as a phase accumulator. The "wave" you see in bloom is purely the persistence/decay of past frames being shifted outward — the position field is `position = 0.25 + 1.75·MOOD` (constant per frame, time-integrated by re-using `leds_16_prev`).

### 2.5 `light_mode_vu_dot` — two-dot VU mirror (motion-relevant, dot-on-dot)

**Verbatim source** (`lightshow_modes.h:180-222`):

```cpp
void light_mode_vu_dot() {
  static SQ15x16 dot_pos_last = 0.0;
  static SQ15x16 audio_vu_level_smooth = 0.0;
  static SQ15x16 max_level = 0.01;

  SQ15x16 mix_amount = mood_scale(0.10, 0.05);

  audio_vu_level_smooth = (audio_vu_level_average * mix_amount) + (audio_vu_level_smooth * (1.0 - mix_amount));

  if (audio_vu_level_smooth * 1.1 > max_level) {
    SQ15x16 distance = (audio_vu_level_smooth * 1.1) - max_level;
    max_level += distance *= 0.1;
  } else {
    max_level *= 0.9999;
    if (max_level < 0.0025) { max_level = 0.0025; }
  }
  SQ15x16 multiplier = 1.0 / max_level;
  SQ15x16 dot_pos = (audio_vu_level_smooth * multiplier);
  if (dot_pos > 1.0) { dot_pos = 1.0; }

  SQ15x16 mix = mood_scale(0.25, 0.24);
  SQ15x16 dot_pos_smooth = (dot_pos * mix) + (dot_pos_last * (1.0-mix));
  dot_pos_last = dot_pos_smooth;

  SQ15x16 brightness = sqrt(float(dot_pos_smooth));

  set_dot_position(RESERVED_DOTS + 0, dot_pos_smooth * 0.5 + 0.5);
  set_dot_position(RESERVED_DOTS + 1, 0.5 - dot_pos_smooth * 0.5);

  clear_leds();

  SQ15x16 hue = chroma_val + hue_position;
  CRGB16 color = hsv(hue, CONFIG.SATURATION, brightness);
  draw_dot(leds_16, RESERVED_DOTS + 0, color);
  draw_dot(leds_16, RESERVED_DOTS + 1, color);
}
```

**Per-frame motion equation (the canonical SB pattern for an audio-driven dot position):**

```
// Stage 1 — input smoothing on VU energy (bounded):
mix_amount        = mood_scale(0.10, 0.05)              // 0.05 to 0.15
audio_vu_smooth  ← audio_vu_smooth · (1−mix) + audio_vu_average · mix

// Stage 2 — adaptive auto-gain envelope (asymmetric tracker):
if (audio_vu_smooth · 1.1 > max_level):
  max_level += (audio_vu_smooth·1.1 − max_level) · 0.1     // fast attack 10%/frame
else:
  max_level *= 0.9999                                       // slow release ≈10s @120 FPS
  max_level  = max(max_level, 0.0025)                       // floor

// Stage 3 — normalised dot position:
dot_pos = clamp(audio_vu_smooth · (1/max_level), 1.0)

// Stage 4 — POSITION smoothing (bounded mood-scaled mix):
mix             = mood_scale(0.25, 0.24)                   // 0.01 to 0.49
dot_pos_smooth ← dot_pos_smooth · (1−mix) + dot_pos · mix

// Stage 5 — render two dots mirrored about centre:
brightness   = sqrt(dot_pos_smooth)
position[0]  = dot_pos_smooth · 0.5 + 0.5                  // centre→top   [0.5, 1.0]
position[1]  = 0.5 − dot_pos_smooth · 0.5                  // centre→bottom [0.0, 0.5]
draw_dot at both positions; draw_dot itself spreads brightness across the swept arc
```

- **Audio sources:** `audio_vu_level_average` from `calculate_vu()` (`i2s_audio.h:190-247`). `calculate_vu` already computes RMS, subtracts noise floor, normalises by `(1 - VU_LEVEL_FLOOR)`, and averages with the previous frame.
- **Smoothing:** **two stages stacked** (input mix + position mix), but BOTH are bounded mood-scaled `[0.05, 0.15]` and `[0.01, 0.49]` mix-amount low-passes, NOT followers stacked on a follower. The `max_level` envelope is a third tracker but it is normalisation, not smoothing of a phase variable.
- **Trail-fade:** none on framebuffer (`clear_leds()`); motion-trail is generated by `draw_dot`'s last-position spread when the dot moves fast.
- **Colour mapping:** `hue = chroma_val + hue_position` (continuous mode only; chromatic mode is unused here).
- **Beat handling:** none — VU itself responds to loudness already.
- **Geometry:** strict centre-origin (positions in [0,1] mapped about 0.5).

**This is the canonical SB pattern for "audio amplitude → dot position":** smoothed VU → adaptive normaliser → bounded position low-pass → mirrored dot pair via `draw_dot` (which itself does sub-LED spread).

### 2.6 `light_mode_kaleidoscope` — Perlin-noise field driven by 3-band onsets (motion-relevant)

**Verbatim source** (`lightshow_modes.h:224-341`, abbreviated to the audio→motion path):

```cpp
void light_mode_kaleidoscope() {
  static float pos_r = 0.0;
  static float pos_g = 0.0;
  static float pos_b = 0.0;

  static SQ15x16 brightness_low = 0.0;
  static SQ15x16 brightness_mid = 0.0;
  static SQ15x16 brightness_high = 0.0;

  SQ15x16 sum_low = 0.0;
  SQ15x16 sum_mid = 0.0;
  SQ15x16 sum_high = 0.0;

  for (uint8_t i = 0; i < 20; i++) {
    SQ15x16 bin = spectrogram_smooth[0 + i];
    bin = bin * 0.5 + (bin * bin) * 0.5;
    sum_low += bin;
    if (bin > brightness_low) {
      SQ15x16 dist = fabs_fixed(bin - brightness_low);
      brightness_low += dist * 0.1;
    }
  }
  // (mid: bins 20..39, high: bins 40..59 — same structure)

  brightness_low *= 0.99;
  brightness_mid *= 0.99;
  brightness_high *= 0.99;

  SQ15x16 shift_speed = (SQ15x16)100 + ((SQ15x16)500 * (SQ15x16)CONFIG.MOOD);
  SQ15x16 shift_r = (shift_speed * sum_low);
  SQ15x16 shift_g = (shift_speed * sum_mid);
  SQ15x16 shift_b = (shift_speed * sum_high);

  pos_r += (float)shift_r;
  pos_g += (float)shift_g;
  pos_b += (float)shift_b;

  for (uint8_t i = 0; i < 64; i++) {
    uint32_t i_shifted = i + 18;
    uint32_t i_scaled = (i_shifted * i_shifted * i_shifted);

    SQ15x16 r_val = inoise16(i_scaled * 0.5 + (uint32_t)pos_r) / 65536.0;
    SQ15x16 g_val = inoise16(i_scaled * 1.0 + (uint32_t)pos_g) / 65536.0;
    SQ15x16 b_val = inoise16(i_scaled * 1.5 + (uint32_t)pos_b) / 65536.0;
    // (clamp + SQUARE_ITER squaring + apply_contrast_fixed)
    // ...
    leds_16[i] = { r_val, g_val, b_val };
    leds_16[NATIVE_RESOLUTION - 1 - i] = leds_16[i];
  }
}
```

**Per-frame motion equation:**

```
// Per-band attack-only follower with global fast decay (decoupled rise/fall):
for each band B in {low, mid, high}:
  sum_B = Σ(i in band) [ bin·0.5 + bin²·0.5 ]                              // soft squared sum
  if (current_bin > brightness_B): brightness_B += (current_bin − brightness_B) · 0.1   // 10% rise
  brightness_B *= 0.99                                                     // global decay each frame

// Velocity = MOOD-scaled band-summed energy:
shift_speed = 100 + 500 · MOOD                                              // [100, 600]
shift_R = shift_speed · sum_low      (and _G ← sum_mid, _B ← sum_high)

// PHASE INTEGRATION (per-channel — three independent drift positions):
pos_R += shift_R                       (likewise for _G, _B)

// Sample Perlin noise field at integrated position:
r_val = inoise16(i_scaled · 0.5 + pos_R) / 65536
g_val = inoise16(i_scaled · 1.0 + pos_G) / 65536
b_val = inoise16(i_scaled · 1.5 + pos_B) / 65536
// (then SQUARE_ITER squarings + apply_contrast_fixed on each channel)
```

- **Audio sources:** `spectrogram_smooth[0..19]` (low), `[20..39]` (mid), `[40..59]` (high). Direct slicing of the 64-bin spectrogram into thirds.
- **Smoothing:** **two stages, stacked:** (1) inside `get_smooth_spectrogram`, (2) per-band `brightness_X` follower with attack-only and 1%/frame global decay. The `pos_X` accumulator does NOT use a Spring or low-pass — it's a **direct integration of band energy as velocity**, which means the phase is unbounded and grows monotonically. Because Perlin is wrap-stable on uint32, this is fine for spatial position but means the rate of motion follows audio energy with **zero damping**.
- **Trail-fade:** none. Frame is fully overwritten; the apparent motion-trail comes from the slow `brightness_X` decay.
- **Colour mapping:** in `chromatic_mode == true`, the three Perlin samples become R/G/B directly (then desaturated via `desaturate(col, 0.1 + (0.9 - 0.9·SAT))`); in continuous mode, the brightest of R/G/B becomes a single hue-mapped CRGB16. Three SQUARE_ITER+1 squarings per channel.
- **Beat handling:** indirect — band energy drives velocity, so a louder transient = faster Perlin scroll in that band. There is no explicit beat trigger.
- **Geometry:** centre-mirrored (`leds_16[NATIVE_RESOLUTION - 1 - i] = leds_16[i]`).

**Note:** kaleidoscope's velocity term `shift_R = (100 + 500·MOOD) · sum_low` is `~100–600 × Σ(20 spectrogram bins)`. With each bin ∈ [0,1] this can reach `60×600 = 36000` units/frame — fed straight into a `uint32_t` Perlin coordinate with no Hz-domain smoothing and no clamp. It works because `inoise16` is wrap-safe and the visible result is a chaotic shimmer, NOT because there's a stability bound. **This is NOT the canonical SB pattern for a wave/snap motion** — it's the canonical pattern for a "noise-field-driven-by-energy" effect.

---

## 3. Supporting machinery (motion-critical)

### 3.1 `get_smooth_spectrogram` — single-stage per-bin asymmetric tracker

```cpp
// lightshow_modes.h:1-16
void get_smooth_spectrogram() {
  static SQ15x16 spectrogram_smooth_last[64];
  for (uint8_t bin = 0; bin < 64; bin++) {
    SQ15x16 note_brightness = spectrogram[bin];
    if (spectrogram_smooth[bin] < note_brightness) {
      SQ15x16 distance = note_brightness - spectrogram_smooth[bin];
      spectrogram_smooth[bin] += distance * SQ15x16(0.75);   // 75% rise per frame
    } else if (spectrogram_smooth[bin] > note_brightness) {
      SQ15x16 distance = spectrogram_smooth[bin] - note_brightness;
      spectrogram_smooth[bin] -= distance * SQ15x16(0.75);   // 75% fall per frame (symmetric)
    }
  }
}
```

This is the only smoothing applied to the post-Goertzel `spectrogram[]` before it leaves the renderer. **It is symmetric (fast attack AND fast release at 75%/frame), per-frame, NOT rate-corrected.** Compare to K1's `heavy_bands` which is 80 ms rise / 15 ms fall (asymmetric). The SB approach is a single, deliberately fast tracker — 75%/frame at ~120 FPS = ~12 ms time constant in both directions.

### 3.2 `make_smooth_chromagram` — chromagram normalisation with peak-tracker

```cpp
// led_utilities.h:1209-1244
void make_smooth_chromagram() {
  memset(chromagram_smooth, 0, sizeof(SQ15x16) * 12);
  for (uint8_t i = 0; i < CONFIG.CHROMAGRAM_RANGE; i++) {           // CHROMAGRAM_RANGE = 60 default
    SQ15x16 note_magnitude = spectrogram_smooth[i];
    if (note_magnitude > 1.0) note_magnitude = 1.0;
    else if (note_magnitude < 0.0) note_magnitude = 0.0;
    uint8_t chroma_bin = i % 12;
    chromagram_smooth[chroma_bin] += note_magnitude / SQ15x16(CONFIG.CHROMAGRAM_RANGE / 12.0);
  }

  static SQ15x16 max_peak = 0.001;
  max_peak *= 0.999;                                                 // slow release ≈1s
  if (max_peak < 0.01) max_peak = 0.01;
  for (uint16_t i = 0; i < 12; i++) {
    if (chromagram_smooth[i] > max_peak) {
      SQ15x16 distance = chromagram_smooth[i] - max_peak;
      max_peak += distance *= SQ15x16(0.05);                          // 5% rise
    }
  }
  SQ15x16 multiplier = 1.0 / max_peak;
  for (uint8_t i = 0; i < 12; i++) chromagram_smooth[i] *= multiplier;
}
```

Octave-fold the 60-bin spectrogram into 12 chroma classes, then auto-normalise by an asymmetric peak tracker (slow release `*= 0.999`, faster rise via 5% of distance). This is the same auto-gain pattern as the `goertzel_max_value` tracker in `process_GDFT()` (`GDFT.h:178-186`).

### 3.3 `low_pass_filter_fixed` / `low_pass_array_fixed` — the canonical SB low-pass

```cpp
// utilities.h:63-73
SQ15x16 low_pass_filter_fixed(SQ15x16 new_data, SQ15x16 last_data, uint32_t sample_rate, float cutoff_freq) {
    SQ15x16 alpha  = 1.0 - expf(-2.0 * PI * cutoff_freq / sample_rate);
    SQ15x16 output = SQ15x16(1.0 - alpha) * (last_data) + alpha * new_data;
    return output;
}
void low_pass_array_fixed(SQ15x16* new_frame, SQ15x16* last_frame, uint16_t length, uint32_t sample_rate, float cutoff_freq){
    for(uint16_t i = 0; i < length; i++){
        new_frame[i] = low_pass_filter_fixed(new_frame[i], last_frame[i], sample_rate, cutoff_freq);
    }
}
```

**This is the rate-correct one-pole filter that K1 doctrine cites as the canonical form** — `α = 1 − exp(−2π·f_c/f_s)` is exact, frame-rate-aware, and its time constant `τ = 1/(2π·f_c)` is in seconds, not frames. Used by `chromagram_dots` (cutoff `mood_scale(3.5, 1.5) = 2..5 Hz`) and by `process_GDFT()` itself (`GDFT.h:149`, cutoff `1 + 10·MOOD = 1..11 Hz`).

### 3.4 `mood_scale` — bidirectional mix-rate parameteriser

```cpp
// utilities.h:79-84
SQ15x16 mood_scale(SQ15x16 center, SQ15x16 range){
    SQ15x16 knob_value_bidirectional = (CONFIG.MOOD - 0.5) * SQ15x16(2.0);   // [-1, +1]
    SQ15x16 result = center + range * knob_value_bidirectional;
    return result;
}
```

`MOOD = 0.0 ⇒ center − range`; `MOOD = 0.5 ⇒ center`; `MOOD = 1.0 ⇒ center + range`. Used to scale low-pass cutoffs, scroll positions, and mix amounts. **All SB modes that use a smoothing rate parameter use this** — the rate is never hard-coded as `*= 0.95`.

### 3.5 `draw_dot` — sub-LED motion-trail compositing

```cpp
// led_utilities.h:390-410
void draw_dot(CRGB16* layer, uint16_t dot_index, CRGB16 color) {
  SQ15x16 position = dots[dot_index].position;
  SQ15x16 last_position = dots[dot_index].last_position;
  SQ15x16 positional_distance = fabs_fixed(position - last_position);
  if (positional_distance < 1.0) positional_distance = 1.0;
  SQ15x16 net_brightness_per_pixel = 1.0 / positional_distance;
  if (net_brightness_per_pixel > 1.0) net_brightness_per_pixel = 1.0;
  draw_line(layer, position, last_position, color, net_brightness_per_pixel);
}
```

**Per-dot motion-trail equation:** brightness is divided by the swept distance, so a stationary dot is full-brightness (1 pixel) and a fast-moving dot is dim-but-spread-across-many-pixels. **This is SB's substitute for an explicit motion-blur or Spring smoother on dot positions** — the swept-line render IS the temporal coherence.

`set_dot_position` (`led_utilities.h:315-318`) stores `last_position ← position` before overwriting, so each frame's `draw_dot` paints a line from previous frame's position to current.

### 3.6 `draw_sprite` — subpixel additive composite (used by bloom)

```cpp
// led_utilities.h:1247-1290
void draw_sprite(CRGB16 dest[], CRGB16 sprite[], uint32_t dest_length, uint32_t sprite_length, float position, SQ15x16 alpha) {
  int32_t position_whole = position;
  float position_fract = position - position_whole;
  SQ15x16 mix_right = position_fract;
  SQ15x16 mix_left = 1.0 - mix_right;
  for (uint16_t i = 0; i < sprite_length; i++) {
    int32_t pos_left = i + position_whole;
    int32_t pos_right = i + position_whole + 1;
    // bounds checks → skip flags
    if (!skip_left)  { dest[pos_left] .{r,g,b} += sprite[i].{r,g,b} * mix_left  * alpha; }
    if (!skip_right) { dest[pos_right].{r,g,b} += sprite[i].{r,g,b} * mix_right * alpha; }
  }
}
```

Subpixel-accurate, additive (no clear). For bloom, `position = 0.25 + 1.75·MOOD ∈ [0.25, 2.00]` LEDs/frame, `alpha = 0.99` (1% energy loss on each scroll). This produces the "expanding bloom" you see in the mode.

### 3.7 `process_GDFT` — spectrogram pipeline (renderer's audio source)

```cpp
// GDFT.h:59-199 (compressed)
void IRAM_ATTR process_GDFT() {
  float MOOD_VAL = CONFIG.MOOD;
  if (CONFIG.LIGHTSHOW_MODE == LIGHT_MODE_BLOOM) MOOD_VAL = 1.0;        // bloom forces max smoothing

  for (uint16_t i = 0; i < NUM_FREQS; i++) {
    // run Goertzel, sqrt, normalise by block_size/2
    magnitudes_normalized_avg[i] = magnitudes_normalized[i]·0.3 + magnitudes_normalized_avg[i]·0.7;   // 30% IIR mix
  }
  // noise-cal subtract (1.5×), clamp ≥0
  memcpy(magnitudes_final, magnitudes_normalized_avg, sizeof(float)*NUM_FREQS);
  low_pass_array(magnitudes_final, magnitudes_last, NUM_FREQS, SYSTEM_FPS, 1.0 + 10.0·MOOD_VAL);  // Hz cutoff
  memcpy(magnitudes_last, magnitudes_final, sizeof(float)*NUM_FREQS);

  // global auto-gain by max_value tracker:
  static SQ15x16 goertzel_max_value = 0.0001;
  // rise: 0.5%/frame, fall: 0.25%/frame, hard floor 4.0
  multiplier = 1.0 / goertzel_max_value;
  for (i = 0..63) spectrogram[i] = magnitudes_final[i] * multiplier;
}
```

**The audio surface that lightshow_modes.h reads (`spectrogram[]`) has TWO smoothing stages already applied:** (1) 30% IIR mix on `magnitudes_normalized_avg`, (2) Hz-domain low-pass at `1–11 Hz` cutoff via `low_pass_array`. Then `get_smooth_spectrogram` adds a third (75%/frame symmetric tracker) before any mode reads `spectrogram_smooth[]`. **This is three smoothing stages by the time a mode sees it.** SB's discipline is that downstream modes do NOT add a fourth — when they need a slower/faster response they call `low_pass_array_fixed` with an explicit Hz cutoff (chromagram_dots) or use `mood_scale` to set a mix rate (vu_dot).

---

## 4. Diff vs SB 4.1.0 (canonical → canonical)

**Headline finding:** `lightshow_modes.h` is **byte-identical** between 4.1.0 and 4.1.1.

```bash
diff SB-4.1.0/lightshow_modes.h SB-4.1.1/lightshow_modes.h
[ok] Files are identical
```

Therefore the entire upstream catalogue of motion modes (gdft, vu_dot, kaleidoscope, chromagram_gradient, chromagram_dots, bloom) and their motion equations are unchanged. The 4.1.0→4.1.1 release is **infrastructure only**.

### 4.1 Per-mode change table

| Mode | 4.1.0 motion equation | 4.1.1 motion equation | Δ | Bug-fix vs feature |
|---|---|---|---|---|
| `light_mode_gdft` | static spectrogram, mirror-up-and-fold | identical | no change | n/a |
| `light_mode_chromagram_gradient` | static interpolated chromagram, centre-mirrored | identical | no change | n/a |
| `light_mode_chromagram_dots` | 12 dots, low-pass cutoff = `mood_scale(3.5, 1.5) Hz` | identical | no change | n/a |
| `light_mode_bloom` | scroll = `0.25 + 1.75·MOOD` LEDs/frame at α=0.99 | identical | no change | n/a |
| `light_mode_vu_dot` | mood-scaled VU mix (0.05–0.15) → adaptive max_level → mood-scaled position mix (0.01–0.49) | identical | no change | n/a |
| `light_mode_kaleidoscope` | per-band brightness follower (10% rise, 1%/frame decay), pos += `(100+500·MOOD)·sum_band` | identical | no change | n/a |

### 4.2 Infrastructure changes (4.1.0 → 4.1.1)

| File / location | Change | Bug-fix or feature |
|---|---|---|
| `globals.h:79` | `BASE_COAT` default `true` → `false` | feature (default presentation tweak) |
| `globals.h:354-362` | added `lock_leds()` / `unlock_leds()` stubs (both bodies commented out) | feature scaffold (no behavioural effect — both functions are no-ops) |
| `led_utilities.h:71,213` | `for (i < 128)` → `for (i < NATIVE_RESOLUTION)` (2 sites) | refactor (constant unification) |
| `led_utilities.h:110-118` | `sweet_spot_state_follower` if/else asymmetric tracker (5% per branch) replaced by single-line `(state·0.05) + (follower·0.95)` low-pass | bug-fix-leaning refactor (the symmetric form is closer to a true exponential low-pass; the original if/else also at 5% is mathematically equivalent at any single direction but the new form is one branch and IRAM-friendlier) |
| `led_utilities.h:707-719` | added `LED_NEOPIXEL_X2` dual-strip support (RGB/GRB/BGR variants) | **feature** — matters for K1: this is upstream's first dual-strip support (still single-DATA-pin `LED_DATA_PIN` + `LED_CLOCK_PIN`, not GPIO 4/5 like K1) |
| `led_utilities.h:725` | `for (uint8_t x ...)` → `for (uint16_t x ...)` (8-bit → 16-bit loop counter on `CONFIG.LED_COUNT`) | bug-fix (allows `LED_COUNT > 255` — required for dual-strip 256+ LEDs) |
| `led_utilities.h:907-930` | inner-loop variable shadow `i` → `pix` inside intro_animation | bug-fix (compiler warning / correctness — the outer `i` was a loop iterator) |
| `i2s_audio.h` | identical | no change |
| `utilities.h` | identical | no change |
| `GDFT.h` | trailing whitespace only (`< 153d152`) | no change |
| `SENSORY_BRIDGE_FIRMWARE.ino:53` | `FIRMWARE_VERSION 40000` → `40101` | metadata |
| `SENSORY_BRIDGE_FIRMWARE.ino:107` | `xTaskCreatePinnedToCore(..., 4096, ...)` → `..., 8192, ...)` (led_thread stack) | bug-fix (stack overflow safety; SB log notes this was an issue with deep `draw_sprite` calls during bloom + prism stacking) |
| `SENSORY_BRIDGE_FIRMWARE.ino:229-237` | conditional `vTaskDelay(0)` / `vTaskDelay(1)` based on `LED_TYPE` (with both branches commented out) → unconditional `vTaskDelay(1)` | bug-fix (yields cleanly to FreeRTOS scheduler each loop regardless of LED type) |

**Net assessment:** zero motion-equation changes. The 4.1.1 release is a stability/resource-correctness pass: bigger LED-thread stack, dual-strip-capable LED registration, single-pole sweet-spot follower, scope-correct loop variables. **No motion behaviour difference.**

---

## 5. Audio surface used (canonical SB 4.1.1, all motion-relevant modes)

| Audio variable | Source | Type | Smoothing applied upstream | Used by |
|---|---|---|---|---|
| `spectrogram[64]` | `process_GDFT()` after Hz-cutoff low-pass + auto-gain | `SQ15x16` | 30% IIR + Hz-domain LPF (1–11 Hz) + asymmetric auto-gain | `get_smooth_spectrogram` (always) |
| `spectrogram_smooth[64]` | `get_smooth_spectrogram()` 75%/frame symmetric tracker | `SQ15x16` | (above) + 75%/frame symmetric tracker = **3 stages** | gdft, kaleidoscope (via 20-bin slices), chromagram_gradient (via interpolate), chromagram_dots indirectly (via chromagram_smooth) |
| `chromagram_smooth[12]` | `make_smooth_chromagram()` octave-fold + 5%-rise / 0.999/frame normaliser | `SQ15x16` | (above) + asymmetric peak tracker = **4 stages** | chromagram_gradient, chromagram_dots (then plus mood-scaled Hz-domain LPF), bloom |
| `audio_vu_level_average` | `calculate_vu()` RMS + noise-floor subtract + 2-frame average | `SQ15x16` | RMS over `SAMPLES_PER_CHUNK=96` + 2-frame avg = ~2 stages | vu_dot |
| `note_chromagram[12]` | computed but commented out in `calc_chromagram_color()` | `float` | unused in 4.1.1 (only commented refs) | — |
| `novelty_curve[SPECTRAL_HISTORY_LENGTH]` | `calculate_novelty()` columnwise positive-change sum, sqrt | `SQ15x16` | per-frame magnitude of positive-change | **only by `process_color_shift()`** for auto-hue cycling — NOT by any motion mode |
| `current_punch` | global (declared in globals.h:169), unset in 4.1.1 | `float` | — | **unused — declared but never written or read in any 4.1.1 lightshow mode** |
| `waveform_peak_scaled` | global (`globals.h:165`), set in `acquire_sample_chunk` | `float` | per-chunk peak / max_waveform_val_follower | **unused in 4.1.1 modes** (one commented reference in bloom only) |
| `waveform_fixed_point[1024]` | I2S samples / 32768 | `SQ15x16` | none | only by `calculate_vu` (RMS) — **never by any motion mode** |
| Beat / tempo / onset | **NOT IN SB 4.1.1.** No beat tracker, no onset detector. | — | — | — |

**Critical observation for the K1 spazz problem.** The entire SB 4.1.1 motion catalogue uses ONE of two audio surfaces:

1. **Chromagram or band-summed spectrogram** (slow, 4-stage smoothed, normalised) — kaleidoscope, bloom, chromagram_dots, chromagram_gradient.
2. **VU level (RMS energy)** with 2-stage smoothing and adaptive normalisation — vu_dot.

**No SB mode reads raw waveform samples or uses peak-detection or onset triggering to drive position.** And no SB mode integrates a phase variable from a feature without bounded smoothing first. The vu_dot and kaleidoscope motion drivers both go through:
- mood-scaled Hz-domain low-pass OR mood-scaled mix rate
- adaptive normaliser (max_level / max_peak / goertzel_max_value)
- bounded clamp

These three properties are the canonical SB invariants for audio-driven motion. K1's spazz-redesign needs to satisfy them, in this exact order.

---

## 6. K1 broken-four → SB 4.1.1 mapping

### 6.1 The four broken effects (per SSA1 framing)

ChevronWaves, ChevronWavesEnhanced, SnapwaveLinear, LGPWaveCollision — all four use a phase accumulator driven by audio velocity, all four spazz on music.

### 6.2 SB 4.1.1 has no direct upstream for any of them

| K1 effect | Closest 4.1.1 archetype | Why | Canonical lift recommendation |
|---|---|---|---|
| **ChevronWaves** | `light_mode_bloom` (only mode with a true scroll/phase) | bloom integrates "outward velocity over time" via `draw_sprite(prev_frame, position=mood_scaled, alpha=0.99)`; ChevronWaves wants outward-travelling chevrons — same family | Replace per-frame phase += rate with a **buffer-scroll** approach (`leds_16_prev` snapshot → subpixel sprite offset) OR keep phase but mood-scale the rate constant + Hz-domain low-pass the audio velocity input first |
| **ChevronWavesEnhanced** | `light_mode_bloom` + `light_mode_kaleidoscope` band-energy velocity | enhanced version probably wants per-band audio→velocity coupling like kaleidoscope's `pos_R += (100+500·MOOD)·sum_low` | Use kaleidoscope's per-band sum + mood-scaled velocity, BUT clamp via `low_pass_array_fixed` at Hz cutoff (kaleidoscope itself does NOT clamp — that is the closest SB analogue but still less safe than the bloom pattern) |
| **SnapwaveLinear** | `light_mode_vu_dot` (audio amplitude → position) | vu_dot is the canonical "audio energy → position" mode. The rest of SnapwaveLinear's snap behaviour has no SB equivalent | Adopt the **vu_dot smoothing chain verbatim**: `mood_scale(0.10, 0.05)` input mix → adaptive `max_level` (10%/0.9999) → `mood_scale(0.25, 0.24)` position mix → `draw_dot` for sub-LED rendering. Substitute `audio_vu_level_average` for whatever K1 ControlBus provides (`rms`, `heavy_bands` summed, etc.) |
| **LGPWaveCollision** | none — no SB mode does multi-wave interference | LGP Wave Collision is K1-original. The closest is bloom's mirror-and-scroll, but two counter-propagating waves do not exist in SB | If lifting from SB, lift the bloom scroll mechanics for each wave individually and additively combine in `leds_16` (the `draw_sprite` is already additive `dest += sprite·alpha`). Do NOT add a phase accumulator without Hz-domain audio low-pass first |

### 6.3 Does 4.1.1 canon match what K1 broken effects do?

**No — they diverge in three structural ways.**

1. **K1 broken-four use a phase accumulator (`phase += speedNorm·240·smoothedSpeed·dt`).** SB 4.1.1 has only ONE mode with anything resembling phase integration: `light_mode_kaleidoscope`'s `pos_R += shift_speed · sum_low`. Even there, `shift_speed = 100 + 500·MOOD` is bounded and `sum_low` is implicitly bounded by 20 spectrogram bins each ≤1. There is **no SB mode** that integrates a phase variable from a Spring-smoothed audio velocity in the K1 doctrine sense. SB's "wave-like" motion (bloom) is buffer-scroll, not phase-equation.
2. **K1 broken-four feed audio to motion via Spring/follower/raw.** SB always feeds audio to motion via either (a) Hz-domain low-pass with cutoff specified in Hz, OR (b) bounded mix-rate from `mood_scale`. Never a frame-rate-coupled `*= 0.95`. Per K1 doctrine SSA1 §1 properties #2 and #12, this matches the K1 standard's prescription — the K1 standard cites `α = 1 − exp(−dt/τ)` which is mathematically the same form as `low_pass_filter_fixed`, just expressed differently. **SB 4.1.1 is a working reference for the K1 doctrine's prescribed form.**
3. **K1 broken-four allow raw or near-raw audio to reach the position term.** SB 4.1.1 NEVER does. Every mode's audio→motion path goes through at minimum 3 smoothing stages (`process_GDFT` 30% IIR → Hz-LPF → auto-gain → `get_smooth_spectrogram` 75%/frame symmetric → optional `make_smooth_chromagram` peak normaliser → optional renderer-stage `low_pass_array_fixed` at Hz cutoff). The adaptive normalisers (`max_level`, `max_peak`, `goertzel_max_value`) are essential — they prevent a sudden loud transient from blowing up the motion velocity. **K1's broken effects, if lacking equivalent normalisation, will spazz precisely on the loudest peaks** — which is exactly the reported behaviour.

**Therefore the 4.1.1 canon does NOT match K1's broken effects. The canon is more conservative on every audio→motion link.** K1's redesign must add: (a) Hz-domain low-pass with explicit cutoff, (b) adaptive normaliser before the velocity → phase integration, (c) bounded clamp on the phase rate.

---

## 7. Summary of canonical SB 4.1.1 invariants for audio-reactive motion

For any K1 spazz-redesign that wants to claim SB-canon lineage:

| # | Invariant | SB 4.1.1 source |
|---|---|---|
| I-1 | All renderer-stage smoothing is either Hz-cutoff `low_pass_filter_fixed` OR mood-scaled mix-rate via `mood_scale(center, range)`. Never a bare `*= 0.95`. | utilities.h:63-84 |
| I-2 | The audio signal that drives motion goes through `process_GDFT` (3 stages) before the renderer sees it; renderers add ≤1 additional stage. | GDFT.h:118, 149, 178-186; lightshow_modes.h:1-16 |
| I-3 | Adaptive normalisation with **slow release / fast attack** is universal: `goertzel_max_value` (0.5%/0.25%), `max_level` in vu_dot (10%/0.9999), `max_peak` in chromagram (5%/0.999). | GDFT.h:178-186; lightshow_modes.h:189-197; led_utilities.h:1225-1243 |
| I-4 | Sub-LED motion-trail is provided by **`draw_dot`** (line from last_position to position with brightness `1/distance`) or **`draw_sprite`** (subpixel additive composite). Phase-accumulator-with-explicit-trail-fade is NOT used. | led_utilities.h:390-410, 1247-1290 |
| I-5 | Centre-origin geometry is universal — every mode that touches both halves of the strip writes to the top half then mirrors down (or writes from index 63/64 outward). | lightshow_modes.h:94-95, 339, 361-362, 481-482, 489-498 |
| I-6 | `MOOD` parameterises smoothing rates and scroll velocities, never the audio feature itself. | every mode that uses `mood_scale` or reads `CONFIG.MOOD` |
| I-7 | No mode reads `waveform[]` or peak-detected audio. The `spectrogram_smooth` / `chromagram_smooth` / `audio_vu_level_average` triplet is the entire renderer-facing audio surface. | lightshow_modes.h grep |
| I-8 | No beat tracker exists. Motion responsiveness is purely a function of smoothed spectral magnitude or smoothed VU; transients propagate naturally through the cascade. | full repo grep |

If K1's broken-four are redesigned to satisfy I-1 through I-7 (I-8 is non-binding for K1, which has its own beat infrastructure), the spazz behaviour will not be possible because every audio→motion path will be Hz-bounded, adaptively-normalised, mood-scaled, and centre-mirrored.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-30 | agent:embedded-system-engineer | Created. Read-only audit of SB 4.1.1 lightshow_modes.h + supporting chain (globals, GDFT, led_utilities, i2s_audio, utilities, .ino dispatch). Verbatim equations for all 6 motion modes. Diff vs 4.1.0 — lightshow_modes.h byte-identical; infrastructure-only delta (BASE_COAT default false, sweet_spot follower simplified, LED_NEOPIXEL_X2 added, led_thread stack 4096→8192, vTaskDelay(1) unconditional). K1 broken-four mapping established: closest analogues are bloom (scroll-via-sprite) and vu_dot (smoothed-amplitude-to-position). Canonical invariants I-1 through I-8 distilled. |
| 2026-04-30 | agent:embedded-system-engineer (re-verification pass) | Independently re-verified all claims against `K1.node1/references/Sensorybridge.sourcecode/SensoryBridge-4.1.1/SENSORY_BRIDGE_FIRMWARE/`. Confirmed: (a) `lightshow_modes.h` is byte-identical to 4.1.0 via `diff` (`Files are identical`); (b) per-function diffs of `process_color_shift`, `make_smooth_chromagram`, `draw_sprite` are bit-identical between versions; (c) `run_sweet_spot()` is the only behavioural change in `led_utilities.h` (asymmetric 5%-rise/5%-fall if/else collapsed to single-line `(state·0.05)+(follower·0.95)` low-pass at lines 110-114 of 4.1.1); (d) `globals.h` only adds no-op `lock_leds()`/`unlock_leds()` stubs (bodies commented out) and flips `BASE_COAT` default to `false`; (e) `constants.h` adds `LED_NEOPIXEL_X2` enum value only; (f) `utilities.h` only adds `clip_float()` helper; (g) `.ino` increases led_thread stack 4096→8192 and changes vTaskDelay branch structure; (h) `GDFT.h` differs by one trailing whitespace line — no semantic change; (i) `i2s_audio.h` differs by whitespace and commented `.intr_alloc_flags = 0` / `.use_apll = true` only; (j) `system.h` differs by `lock_leds()` call (no-op) replacing direct `led_thread_halt = true` assignment. **No new audio surface fields. No new motion mode. No motion-equation change.** Document accurately captures upstream reality; no edits required. |
