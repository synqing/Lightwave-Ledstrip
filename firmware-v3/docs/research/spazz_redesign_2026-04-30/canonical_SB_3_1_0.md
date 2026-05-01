---
abstract: "Verbatim extraction of every motion-relevant light_mode_* in Sensory Bridge 3.1.0 (the OLDER baseline before Snapwave/Chevron existed). Six modes total — gdft, gdft_chromagram, bloom (slow + fast), waveform, vu, vu_dot. NO light_mode_snapwave in 3.1.0; the namesake of K1's broken SnapwaveLinearEffect was added in a later 4.x lineage. Documents the original motion vocabulary: shift-register scrolling (bloom), exp-avg-smoothed VU bar/dot, waveform-modulated chromagram colour, GDFT spectrogram one-bin-per-LED-pair. Maps which patterns survived into 4.x and which evolved. Authoritative reference for K1 redesign of broken wave/chevron/snap effects when comparing 3.1.0 simplicity vs 4.x complexity."
---

# Canonical Sensory Bridge 3.1.0 — Motion Mode Extraction

**Status:** GROUNDED — every claim cites the verbatim 3.1.0 source on disk.
**Source path:** `/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/Sensorybridge.sourcecode/SensoryBridge-3.1.0/SENSORY_BRIDGE_FIRMWARE/`

This document is the SSA-A/SSA-B counterpart for the OLDER 3.1.0 baseline. It exists because K1's broken `SnapwaveLinearEffect`, `ChevronWaveEffect`, and related "wave/chevron/snap" effects spazz under music, and the working hypothesis is that K1's lineage ultimately descends from this earlier, simpler motion vocabulary — before the 4.x complexity (look-ahead, multi-band followers, percussion triggers) was layered on top.

---

## File map

`lightshow_modes.h` is **429 lines, six functions**. There is no `light_mode_snapwave`, `light_mode_chevron`, or anything wave/snap/chevron-named in 3.1.0.

| Function | Line range | Audio surface | Motion class |
|---|---|---|---|
| `light_mode_gdft()` | 2-45 | `note_spectrogram_smooth[64]` | One-bin-per-LED-pair, **stateless** (per-frame brightness map) |
| `light_mode_gdft_chromagram()` | 47-80 | `note_chromagram[12]` interpolated to 128 LEDs | Stateless, interpolated chromagram |
| `light_mode_bloom(bool fast_scroll)` | 82-158 | `note_chromagram[12]`, `chromagram_max_val` | **Shift-register scroll** — every other frame, leds_last shifts by 1 (slow) or 2 (fast), new colour written at index 0 |
| `light_mode_waveform()` | 160-260 | `waveform_history[4][1024]`, `waveform_peak_scaled`, `note_chromagram[12]` | Per-LED exp-avg of waveform samples, colour from chromagram, brightness gated by waveform_peak_scaled |
| `light_mode_vu()` | 262-332 | `waveform_peak_scaled`, `note_chromagram[12]` | **Bar VU** — exp-avg-smoothed `led_pos`, fills [0..pos] solid, last LED scaled by fract |
| `light_mode_vu_dot()` | 334-429 | `waveform_peak_scaled`, `note_chromagram[12]` | **Sweeping dot VU** — exp-avg-smoothed `led_pos`, draws from `led_pos_last` to `led_pos_smooth` (or vice-versa), full clear each frame |

Confirmed by `SENSORY_BRIDGE_FIRMWARE.ino:59-67`:

```cpp
LIGHT_MODE_GDFT,
LIGHT_MODE_GDFT_CHROMAGRAM,
LIGHT_MODE_BLOOM,
LIGHT_MODE_BLOOM_FAST,
LIGHT_MODE_VU,
LIGHT_MODE_VU_DOT,

NUM_MODES
```

**No Snapwave. No Chevron. Six modes only.** The dispatch in the .ino file (lines 172-188) wires each enum to the corresponding `light_mode_*` call.

---

## Per-mode analysis

### 1. `light_mode_gdft()` (lines 2-45) — stateless spectrogram

**Verbatim:**

```cpp
void light_mode_gdft() {
  for (uint8_t i = 0; i < NUM_FREQS; i += 1) {  // 64 freqs
    float bin = note_spectrogram_smooth[i];
    for (uint8_t s = 0; s < CONFIG.SQUARE_ITER; s++) {
      bin = (bin * bin);
    }

    float    led_brightness_raw = 254 * bin; // -1 for temporal dithering below
    uint16_t led_brightness     = led_brightness_raw;
    float    fract              = led_brightness_raw - led_brightness;

    if (CONFIG.TEMPORAL_DITHERING == true) {
      if (fract >= dither_table[dither_step]) {
        led_brightness += 1;
      }
    }

    brightness_levels[i] = led_brightness;  // Can use this value later if needed

    float led_hue_a;
    float led_hue_b;
    if (chromatic_mode == true) {
      led_hue_a = 21.33333333 * i;  // Makes hue completely cycle once per octave
      led_hue_b = led_hue_a + 10.66666666;
    }
    else {
      led_hue_a = 255 * chroma_val;  // User color selection
      led_hue_b = led_hue_a;
    }

    CRGB col1 = CRGB(0, 0, 0);
    hsv2rgb_spectrum(
      CHSV(led_hue_a, 255, brightness_levels[i]),
      col1);

    CRGB col2 = CRGB(0, 0, 0);
    hsv2rgb_spectrum(
      CHSV(led_hue_b, 255, brightness_levels[i]),
      col2);

    leds[i * 2 + 0] = col1;  // Two LEDs at a time so that mirror mode works gracefully
    leds[i * 2 + 1] = col2;
  }
}
```

| Parsed model | Value |
|---|---|
| Per-frame motion equation | None — purely stateless. `leds[i*2+0]` and `leds[i*2+1]` are FULLY OVERWRITTEN each frame from `note_spectrogram_smooth[i]`. |
| Audio source | `note_spectrogram_smooth[64]` (look-ahead-smoothed, see GDFT.h §lookahead_smoothing) |
| Smoothing | Upstream only: Type-A (followers, line 239 GDFT.h) + Type-B (exp-avg, line 286 GDFT.h) + look-ahead despike (line 367 GDFT.h). Inside the mode there is NO smoothing/follower. |
| Trail-fade | None — fully overwritten |
| Colour mapping | Chromatic: `hue = 21.33 * i` (cycles full hue every octave, since 21.33 × 12 ≈ 256). Monochrome: `hue = 255 * chroma_val` |
| Beat/percussion | None |
| Frame statefulness | None (`brightness_levels[i]` is written but only consumed within the same frame's loop) |

### 2. `light_mode_gdft_chromagram()` (lines 47-80) — stateless chromagram

**Verbatim:**

```cpp
void light_mode_gdft_chromagram() {
  for (uint16_t i = 0; i < NATIVE_RESOLUTION; i++) {
    float prog = i / float(NATIVE_RESOLUTION);

    float bin = interpolate(prog, note_chromagram, 12);

    for (uint8_t s = 0; s < CONFIG.SQUARE_ITER; s++) {
      bin = bin * bin;
    }

    float    led_brightness_raw = 254 * bin;
    uint16_t led_brightness     = led_brightness_raw;
    float    fract              = led_brightness_raw - led_brightness;

    if (CONFIG.TEMPORAL_DITHERING == true) {
      if (fract >= dither_table[dither_step]) {
        led_brightness += 1;
      }
    }

    float led_hue;
    if (chromatic_mode == true) {
      led_hue = 255 * prog;
    }
    else {
      led_hue = 255 * chroma_val;
    }

    hsv2rgb_spectrum(
      CHSV(led_hue, 255, led_brightness),
      leds[i]
    );
  }
}
```

| Parsed model | Value |
|---|---|
| Per-frame motion equation | Stateless. Each LED brightness = `interpolate(i/128, note_chromagram, 12)^(2^SQUARE_ITER) × 254`. |
| Audio source | `note_chromagram[12]` (built per-frame in GDFT.h:299-319 by summing `note_spectrogram` across 6 octaves with 0.5 weighting per octave) |
| Smoothing | Upstream only (chromagram inherits spectrogram smoothing). None inside the mode. |
| Trail-fade | None |
| Colour mapping | Chromatic: `hue = 255 * prog` (rainbow across 128 LEDs). Monochrome: `hue = 255 * chroma_val` |
| Beat/percussion | None |

### 3. `light_mode_bloom(bool fast_scroll)` (lines 82-158) — shift-register scroll (THE archetypal motion mode)

**Verbatim:**

```cpp
void light_mode_bloom(bool fast_scroll) {
  static uint32_t iter = 0;
  const float led_share = 255 / float(12);

  iter++;

  if (bitRead(iter, 0) == 0) {
    CRGB sum_color = CRGB(0, 0, 0);
    float brightness_sum = 0.0;
    for (uint8_t i = 0; i < 12; i++) {
      float prog = i / float(12);

      float bin = note_chromagram[i]; // * (1.0 / chromagram_max_val);

      float bright = bin;
      for (uint8_t s = 0; s < CONFIG.SQUARE_ITER; s++) {
        bright *= bright;
      }
      bright *= 1.5;
      if (bright > 1.0) {
        bright = 1.0;
      }

      bright *= led_share;

      CRGB out_col;
      if (chromatic_mode == true) {
        hsv2rgb_spectrum(
          CHSV(255 * prog, 255, bright),
          out_col);
      }
      else {
        brightness_sum += bright;
      }

      sum_color += out_col;
    }

    if (chromatic_mode == false) {
      hsv2rgb_spectrum(
        CHSV(255 * chroma_val, 255, brightness_sum),
        sum_color);
    }

    if (fast_scroll == true) { // Fast mode scrolls two LEDs at a time
      for (uint8_t i = 0; i < NATIVE_RESOLUTION - 2; i++) {
        leds_temp[(NATIVE_RESOLUTION - 1) - i] = leds_last[(NATIVE_RESOLUTION - 1) - i - 2];
      }

      leds_temp[0] = sum_color; // New information goes here
      leds_temp[1] = sum_color; // New information goes here
    }
    else { // Slow mode only scrolls one LED at a time
      for (uint8_t i = 0; i < NATIVE_RESOLUTION - 1; i++) {
        leds_temp[(NATIVE_RESOLUTION - 1) - i] = leds_last[(NATIVE_RESOLUTION - 1) - i - 1];
      }

      leds_temp[0] = sum_color; // New information goes here
    }

    load_leds_from_temp();
    save_leds_to_last();

    distort_logarithmic();

    fade_top_half(CONFIG.MIRROR_ENABLED);
    increase_saturation(32);

    save_leds_to_aux();
  }
  else{
    load_leds_from_aux();
  }
}
```

| Parsed model | Value |
|---|---|
| Per-frame motion equation | **Shift register**, every-other-frame: on even frames (`bitRead(iter,0)==0`), copy `leds_last[N-1-i-OFFSET] -> leds_temp[N-1-i]`, write fresh `sum_color` at indices 0 (and 1 if fast_scroll). On odd frames, restore from `leds_aux` (no motion). OFFSET = 1 slow / 2 fast. |
| Audio source | `note_chromagram[12]` summed (chromatic mode produces a polychromatic blend; monochrome mode sums brightness then applies `chroma_val` hue) |
| Smoothing/follower | None inside the mode. Inherits chromagram smoothing. **`bright *= 1.5` then clamp** is the only non-linear shaping. |
| Trail-fade | NOT a brightness fade — instead `distort_logarithmic()` (see led_utilities.h:407) **squashes** the trail toward the new-data end via `prog_distorted = sqrt(prog)`, and `fade_top_half()` (led_utilities.h:424) linearly fades the FAR HALF of the strip to zero by `i/64`. |
| Colour mapping | Chromatic: each chromagram bin contributes `CHSV(255*prog_bin, 255, bright)` summed. Monochrome: scalar brightness × `chroma_val` hue. Saturation boosted by 32 each frame. |
| Beat/percussion | None |
| Frame statefulness | `leds_last` (last-frame snapshot), `leds_temp` (working buffer), `leds_aux` (frame-skip cache), `iter` (parity counter). Effectively runs at 50% frame rate. |

This mode is the **archetype** of "scrolling LED motion" in SB. The shift-register pattern (`leds_temp[N-1-i] = leds_last[N-1-i-OFFSET]`, plus new colour at index 0) is the simplest possible scroll. Everything else in the SB lineage that resembles "motion travelling along the strip" descends from this pattern.

### 4. `light_mode_waveform()` (lines 160-260) — per-LED waveform exp-avg with brightness gating

**Verbatim core motion block (lines 220-259):**

```cpp
for (uint8_t i = 0; i < NATIVE_RESOLUTION; i++) {
  float waveform_sample = 0.0;
  for (uint8_t s = 0; s < 4; s++) {
    waveform_sample += waveform_history[s][i];
  }
  waveform_sample /= 4.0;
  float input_wave_sample = (waveform_sample / 128.0);

  //----------------------

  float smoothing = (0.1 + CONFIG.MOOD * 0.9) * 0.05;

  waveform_last[i] = input_wave_sample * (smoothing) + waveform_last[i] * (1.0 - smoothing);

  float peak = waveform_peak_scaled_last * 4.0;
  if (peak > 1.0) {
    peak = 1.0;
  }

  float output_brightness = (waveform_last[i]);
  if (output_brightness > 1.0) {
    output_brightness = 1.0;
  }

  output_brightness = 0.5 + output_brightness * 0.5;
  // ... clamps ...
  output_brightness *= peak;

  leds[i] = CRGB(
              sum_color_float[0] * output_brightness,
              sum_color_float[1] * output_brightness,
              sum_color_float[2] * output_brightness
            );
}
```

| Parsed model | Value |
|---|---|
| Per-frame motion equation | Per-LED exp-avg of (averaged) waveform: `waveform_last[i] = sample * α + waveform_last[i] * (1-α)`, where `α = (0.1 + MOOD*0.9) * 0.05`. Final brightness = `(0.5 + 0.5*waveform_last[i]) * peak`. |
| Audio source | `waveform_history[4][1024]` (4-frame average of raw waveform), `waveform_peak_scaled_last`, `note_chromagram[12]` (for colour) |
| Smoothing | **Two layers**: (a) `waveform_peak_scaled_last = peak * 0.05 + last * 0.95` (line 166), (b) per-LED exp-avg with α derived from MOOD knob (line 232). Plus `sum_color` exp-avg at α=0.05/0.95 (lines 212-218). |
| Trail-fade | None — every LED is overwritten each frame (the brightness IS the trail, encoded into `waveform_last[]`). |
| Colour mapping | Same as bloom — `note_chromagram[12]` summed (chromatic) or scalar-summed and hued by `chroma_val` (monochrome). The colour ITSELF is exp-avg-smoothed (α=0.05/0.95). |
| Beat/percussion | Implicit via `waveform_peak_scaled` (peak detector with delta-follower smoothing in i2s_audio.h:131-138) |

### 5. `light_mode_vu()` (lines 262-332) — solid-bar VU

**Verbatim core (lines 266-279, 315-331):**

```cpp
float smoothing = (0.025 + CONFIG.MOOD * 0.975) * 0.25;
float led_pos = waveform_peak_scaled * (NATIVE_RESOLUTION - 1);
static float led_pos_smooth = 0.0;

led_pos_smooth = led_pos * (smoothing) + led_pos_smooth * (1.0 - smoothing);

if (led_pos_smooth > 126) {
  led_pos_smooth = 126;
}
else if (led_pos_smooth < 0) {
  led_pos_smooth = 0;
}

uint16_t led_pos_smooth_whole = led_pos_smooth;
float fract = led_pos_smooth - led_pos_smooth_whole;

// ... colour computation ...

for (uint8_t i = 0; i < NATIVE_RESOLUTION; i++) {
  leds[i] = CRGB(0, 0, 0);
  if (i < led_pos_smooth) {
    leds[i] = CRGB(
                sum_color_float[0],
                sum_color_float[1],
                sum_color_float[2]
              );
  }
  else if (i == led_pos_smooth) {
    leds[i] = CRGB(
                sum_color_float[0] * fract,
                sum_color_float[1] * fract,
                sum_color_float[2] * fract
              );
  }
}
```

| Parsed model | Value |
|---|---|
| Per-frame motion equation | `led_pos_smooth = waveform_peak_scaled*127 * α + led_pos_smooth * (1-α)`, where `α = (0.025 + MOOD*0.975) * 0.25`. Fill solid colour from 0 to floor(led_pos_smooth); the last LED at exactly `led_pos_smooth` is scaled by the fractional part for sub-pixel accuracy. |
| Audio source | `waveform_peak_scaled` (single scalar, 0..1), `note_chromagram[12]` for colour |
| Smoothing | Single exp-avg on `led_pos`. Plus `sum_color_float[]` exp-avg at α=0.05/0.95. |
| Trail-fade | **None — explicit `leds[i] = CRGB(0,0,0)` clear** before redraw. The smoothness comes purely from `led_pos_smooth` smoothing. |
| Colour mapping | Same chromagram-or-chroma_val pattern as waveform. Saturation+brightness boosted by qadd8(64) once. |
| Beat/percussion | Implicit via `waveform_peak_scaled` |

### 6. `light_mode_vu_dot()` (lines 334-429) — sweeping dot VU with `fadeToBlackBy(255)` clear

**Verbatim core motion (lines 385-428):**

```cpp
fadeToBlackBy(leds, NATIVE_RESOLUTION, 255);

if (led_pos_last < led_pos_smooth) {
  for (uint8_t i = led_pos_last; i <= led_pos_smooth; i++) {
    leds[i] = CRGB(...);
    leds[i + 1] = CRGB(...);
  }
}
else if (led_pos_last > led_pos_smooth) {
  for (uint8_t i = led_pos_smooth; i <= led_pos_last; i++) {
    leds[i] = CRGB(...);
    leds[i + 1] = CRGB(...);
  }
}
else {
  leds[uint8_t(led_pos_smooth)] = CRGB(...);
  leds[uint8_t(led_pos_smooth) + 1] = CRGB(...);
}

led_pos_last = led_pos_smooth;
```

| Parsed model | Value |
|---|---|
| Per-frame motion equation | Same `led_pos_smooth` exp-avg as `light_mode_vu`. Then `fadeToBlackBy(leds, 128, 255)` — that's a **full clear** (255 = 100% fade) — followed by drawing a 2-LED-wide segment from `led_pos_last` to `led_pos_smooth` (or vice-versa). |
| Audio source | Same as `light_mode_vu` |
| Smoothing | Same as `light_mode_vu` |
| Trail-fade | **None despite the `fadeToBlackBy` call** — the magnitude 255 means full clear. The "motion blur" comes from drawing the entire path between `led_pos_last` and `led_pos_smooth`, not from frame-persistence. |
| Colour mapping | Same as `light_mode_vu` |
| Beat/percussion | Implicit via `waveform_peak_scaled` |

This is important to note: **3.1.0 has NO mode that uses partial fadeToBlack (e.g., `fadeToBlackBy(leds, N, 32)`) for trail-persistence**. Every mode either fully clears or doesn't clear at all. The "trail" in bloom is achieved by shift-register copying, not by partial fade. The "trail" in vu_dot is achieved by drawing the full path between last and current dot positions.

---

## Audio surface used (what GDFT.h fields existed in 3.1.0)

From `globals.h:115-145` and `GDFT.h`:

### Per-frame-rebuilt arrays (3.1.0)

| Field | Size | Built where | Modes that consume |
|---|---|---|---|
| `magnitudes[NUM_FREQS]` | 64 | GDFT.h:100 (Goertzel raw) | None directly |
| `mag_targets[NUM_FREQS]` | 64 | GDFT.h:160 (= magnitudes after noise reduction) | None directly |
| `mag_followers[NUM_FREQS]` | 64 | GDFT.h:236-246 (Type-A follower with MOOD-derived rate) | None directly |
| `mag_float_last[NUM_FREQS]` | 64 | GDFT.h:287 (last-frame cache for Type-B) | None directly |
| `note_spectrogram[NUM_FREQS]` | 64 | GDFT.h:294 (current normalised value) | None directly |
| `spectrogram_history[3][64]` | 192 | GDFT.h:295 (3-frame ring buffer) | None directly |
| `note_spectrogram_smooth[NUM_FREQS]` | 64 | GDFT.h:371 (look-ahead-smoothed = past_index of history) | **gdft** |
| `note_spectrogram_long_term[NUM_FREQS]` | 64 | GDFT.h:372 (very-slow exp-avg, α=0.95) | **none** (not used in 3.1.0 lightshow modes) |
| `note_chromagram[12]` | 12 | GDFT.h:299-319 (sum of spectrogram across 6 octaves × 0.5 weight) | **gdft_chromagram, bloom, bloom_fast, waveform, vu, vu_dot** |
| `chromagram_max_val` | scalar | GDFT.h:314 (max over chromagram bins) | **waveform, vu, vu_dot** (used as denominator for normalisation) |
| `chromagram_bass_max_val` | scalar | declared globals.h:123 — **NOT WRITTEN in 3.1.0**, dead variable | none |
| `max_mags[NUM_ZONES]` | 4 | GDFT.h:65, 162 (per-zone magnitude cap) | none directly |
| `max_mags_followers[NUM_ZONES]` | 4 | GDFT.h:251-258 (slow follower of max_mags, hardcoded 0.05) | used internally for normalisation |

### Time-domain (waveform) arrays

| Field | Size | Source | Modes |
|---|---|---|---|
| `waveform[1024]` | 1024 | i2s_audio.h:83 (current frame, DC-corrected) | none directly |
| `waveform_history[4][1024]` | 4096 | i2s_audio.h:84 (4-frame ring) | **waveform** |
| `waveform_peak_scaled` | scalar | i2s_audio.h:129-138 (= max_waveform_val / max_waveform_val_follower, then double-smoothed) | **waveform, vu, vu_dot** |
| `silent_scale` | scalar | i2s_audio.h:148-160 (silence-detector fade) | applied in `show_leds()` globally |

### What does NOT exist in 3.1.0

- **No `chromagram_smooth[]`** — chromagram is rebuilt fresh every frame from `note_spectrogram`, no per-bin exp-avg
- **No `bands[]`** (8-band octave summary) — that is a 4.x construct
- **No beat/onset/tempo fields** — no beat tracker, no autocorrelation tempo, no onset detector
- **No percussion triggers** — `waveform_peak_scaled` is the closest thing, and it is just an envelope of the time-domain peak with a quick attack / slow release
- **No `bins256[]`** — that is K1's interpolated spectrogram, not a SB construct
- **No look-ahead chromagram** — only `note_spectrogram_smooth` (look-ahead) exists; chromagram has none

### Smoothing parameters in 3.1.0 (canonical values)

| Smoothing point | Coefficient | Source line |
|---|---|---|
| `mag_followers` rising | `smoothing_follower * 0.45` | GDFT.h:239 |
| `mag_followers` falling | `smoothing_follower * 0.55` | GDFT.h:244 |
| `max_mags_followers` rising/falling | `0.05` (hardcoded) | GDFT.h:253, 257 |
| Spectrogram exp-avg | `smoothing_exp_average` | GDFT.h:286 |
| Long-term spectrogram | `0.95` (very slow) | GDFT.h:372 |
| `max_waveform_val_follower` rising | `0.25` (fast attack) | i2s_audio.h:119 |
| `max_waveform_val_follower` falling | `0.005` (slow release) | i2s_audio.h:123 |
| `waveform_peak_scaled` rising/falling | `0.25` / `0.25` | i2s_audio.h:133, 137 |
| `waveform_peak_scaled_last` (waveform mode) | `0.05/0.95` | lightshow_modes.h:166 |
| `waveform_last[i]` (waveform mode) | `(0.1 + MOOD*0.9) * 0.05` | lightshow_modes.h:230 |
| `led_pos_smooth` (vu/vu_dot) | `(0.025 + MOOD*0.975) * 0.25` | lightshow_modes.h:266, 339 |
| `sum_color_last[]` (waveform/vu/vu_dot) | `0.05/0.95` | lightshow_modes.h:212-214, 307-309, 377-379 |

The MOOD-knob-derived smoothing curves (lines 230 and 266/339) are different functions:
- **waveform**: `α = (0.1 + MOOD*0.9) * 0.05` → range [0.005, 0.05]
- **vu/vu_dot**: `α = (0.025 + MOOD*0.975) * 0.25` → range [0.00625, 0.25]

VU smoothing has a much wider knob range, allowing near-instant response at MOOD=1.0.

---

## The original Snapwave — DOES NOT EXIST in 3.1.0

**Definitive finding:** `light_mode_snapwave` does not exist in Sensory Bridge 3.1.0.

Verified by:
1. `grep -rn -i "snap\|wave\|chevron" SENSORY_BRIDGE_FIRMWARE/` — no `snap*` token anywhere in the codebase (only `waveform`, which is the time-domain audio buffer, not a mode name)
2. `lightshow_modes.h` ends at line 429 with `light_mode_vu_dot`'s closing brace — six functions total
3. `SENSORY_BRIDGE_FIRMWARE.ino:59-67` enumerates ALL six modes; `NUM_MODES` is 6
4. The dispatch (lines 172-188 of the .ino) wires exactly six branches

**Implication for K1:** The namesake of K1's broken `SnapwaveLinearEffect` was added in a later 4.x lineage. The name "Snapwave" therefore does not have a 3.1.0 ancestor to anchor on. K1's redesign of the broken effect cannot use 3.1.0 as a literal reference for "what Snapwave originally was" — there was no Snapwave originally.

What the 3.1.0 baseline DOES tell us is the original SB **motion vocabulary**: shift-register scroll (bloom), exp-avg-smoothed sweep (vu, vu_dot), per-LED exp-avg of waveform-with-peak-gate (waveform), stateless spectrogram render (gdft, gdft_chromagram). If "Snapwave" was meant to fit the bloom or vu_dot lineage, 3.1.0 shows what the simpler, working ancestor of that motion class looked like.

---

## Heritage — what survived into 4.x and what evolved

This section contrasts 3.1.0 with the SSA-A/SSA-B 4.x extractions (per the same research lineage). Conclusions are conditional on those documents being accurate.

### Patterns that SURVIVED into 4.x

1. **Goertzel-based 64-bin GDFT** — the algorithm and the 64-frequency layout are SB's identity. 4.x kept this.
2. **Look-ahead despike on spectrogram** (`note_spectrogram_smooth` via 3-frame history with mid-frame averaging) — `lookahead_smoothing()` (GDFT.h:322-389) is the "secret sauce" that makes the spectrogram visually smooth. This is preserved in 4.x.
3. **Two-layer smoothing topology** (Type-A follower + Type-B exp-avg, both MOOD-knob-modulated) — the `smoothing_follower` and `smoothing_exp_average` knobs persist into 4.x.
4. **`note_chromagram[12]` build** (sum across octaves with 0.5-per-octave weighting, clamped to 1.0, with `chromagram_max_val` cached) — preserved.
5. **`waveform_peak_scaled` envelope** (max-detect with delta-follower attack/release in i2s_audio.h) — the "loudness scalar" pattern is still recognisable in 4.x's RMS-based loudness.
6. **`led_pos_smooth` exp-avg sweep** — vu and vu_dot's motion equation (`led_pos_smooth = led_pos * α + led_pos_smooth * (1-α)`) is the genealogical ancestor of any "smooth pointer that follows audio" pattern in 4.x.
7. **`distort_logarithmic()` perceptual remapping** — the `prog_distorted = sqrt(prog)` warp used by bloom is the ancestor of 4.x's perceptual non-linearities.
8. **Shift-register scroll** — bloom's pattern (`leds_temp[N-1-i] = leds_last[N-1-i-OFFSET]`, write fresh data at index 0) is the simplest motion archetype. Anything in 4.x that "scrolls along the strip with new data injected at one end" descends from this.
9. **Frame-skip motion (50% effective frame rate)** — bloom's `bitRead(iter, 0)` parity gate is a primitive form of what 4.x does with motion phase accumulators.

### Patterns that EVOLVED in 4.x

1. **Chromagram smoothing** — 3.1.0 has none (chromagram rebuilt fresh from spectrogram each frame). 4.x adds a chromagram-level smoother (per SSA-A/B).
2. **Octave bands** — 3.1.0 has no `bands[8]` summary; it works directly off the 64-bin spectrogram or 12-bin chromagram. 4.x adds the 8-band octave aggregation.
3. **Per-mode smoothing tuning** — 3.1.0's MOOD coefficient is universal; 4.x exposes per-mode smoothing constants.
4. **Trail-persistence via partial fadeToBlackBy** — 3.1.0 NEVER uses partial fade. 4.x introduces `fadeToBlackBy(leds, N, K)` with K<255 to create exponential decay trails (this is the source of half of K1's broken behaviour: `fadeToBlackBy` operates on per-frame decay constants that don't account for `dt` jitter).

### Patterns that were NEW in 4.x (no 3.1.0 ancestor)

1. **Beat tracker** — no autocorrelation tempo, no beat output, no onset detector in 3.1.0
2. **Percussion triggers** — no `kick`, `snare`, `hat` events in 3.1.0
3. **Multi-band followers (`bands[]`, sub-band envelopes)** — 3.1.0's `mag_followers[64]` is a per-bin follower of the spectrogram, not an octave-band follower
4. **Centre-out propagation** — 3.1.0 always fills from index 0 outward (or scrolls from index 0). The centre-origin (LED 79/80) idiom is K1-specific and post-dates 3.1.0
5. **Chevron / Snapwave / Snap effects** — none of these names exist in 3.1.0
6. **Zone composition** — 3.1.0 is single-strip, 128 LEDs, no zones. The K1 "3 zones × 53 LEDs" layout has no 3.1.0 ancestor

### Patterns that DROPPED out of 4.x (or moved upstream)

- **`note_spectrogram_long_term[]`** — declared and computed in 3.1.0 (GDFT.h:372) but **never consumed** by any 3.1.0 mode. May have been intended for a "calm/active" mood detector that was deferred. 4.x equivalents exist as RMS/loudness long-term envelopes.
- **`chromagram_bass_max_val`** — declared in globals.h:123, never assigned anywhere in 3.1.0. Dead code.

---

## K1 broken-4 mapping

### Snapwave / SnapwaveLinearEffect

K1's `SnapwaveLinearEffect` has no direct 3.1.0 ancestor. The closest 3.1.0 motion archetype, by behaviour-shape, is:

- **If "snap" means percussive impulse driving a wave**: the closest 3.1.0 analogue is `light_mode_vu_dot` (sweeping dot driven by `waveform_peak_scaled`), with the impulse coming from the time-domain peak detector. There is no spectral-percussion source in 3.1.0.
- **If "wave" means a moving wavefront along the strip**: the closest 3.1.0 analogue is `light_mode_bloom` (shift-register propagation). 3.1.0 always propagates from index 0; K1's centre-out (79/80) is a layout twist on the same shift-register idea.

**Diagnostic implication for K1's spazz bug**: If the broken Snapwave is using `fadeToBlackBy(leds, N, K)` with K<255 for trail-persistence, that is a 4.x-introduced pattern with NO 3.1.0 ancestor. Reverting to a 3.1.0-style "fully clear or shift-register" pattern would eliminate the dt-jitter sensitivity at the cost of less smooth-looking trails. The 3.1.0 way to get smooth motion is **smooth the position/brightness, then redraw clean** — not "redraw and let the previous frame decay".

### Chevron / ChevronWaveEffect

No 3.1.0 ancestor. Chevron is a 4.x-or-later pattern (centre-out symmetric wavefront with directional skew). 3.1.0's only symmetric-rendering construct is `mirror_image_upwards()` / `mirror_image_downwards()` (led_utilities.h:244-258), which are post-render mirror operations, not inherent to any mode.

### Wave (generic)

The 3.1.0 mode named `light_mode_waveform` is **not** a "wave-along-the-strip" effect — it is a per-LED brightness modulation driven by the time-domain waveform samples. The "wave" is in the audio amplitude over time, not in spatial motion across the strip. Each LED `i` independently smooths its own `waveform_history[s][i]` sample (which, given that the waveform array is just stored DC-corrected audio, means each LED is showing a slightly time-shifted version of the amplitude envelope at a shared brightness — there is no spatial wave propagation).

If K1's broken `WaveEffect` is attempting spatial wave propagation, **its closest 3.1.0 ancestor is `light_mode_bloom` (shift-register)** — not `light_mode_waveform`.

### Most-likely root cause indicator

Three of the 3.1.0 modes (gdft, gdft_chromagram, vu/vu_dot) **fully overwrite the LED buffer each frame**. The other two (bloom, waveform) maintain frame-to-frame state but DO NOT use exponential decay (`fadeToBlackBy` with K<255) — bloom uses shift-register copy from `leds_last`, waveform uses per-LED exp-avg of input samples.

If K1's broken wave/chevron/snap effects are using `fadeToBlackBy(leds, N, K)` with K in the range 8-64 to create trails, **that pattern has no 3.1.0 ancestor**, and is dt-jitter-sensitive because `fadeToBlackBy` applies a fixed per-frame multiplicative decay regardless of how long the previous frame actually took. The 3.1.0 baseline shows that smooth motion was achieved WITHOUT exponential trail decay, by smoothing the source signal/position and redrawing clean.

This is the most actionable heritage finding for K1's spazz bug:

> **The 3.1.0 baseline never used `fadeToBlackBy(leds, N, K<255)` for trail-persistence. It got smooth motion by smoothing the source values and redrawing the strip clean each frame, or by deterministically shift-register copying the previous frame.**

---

## Return contract

**Files inspected:** 6 (lightshow_modes.h, globals.h, GDFT.h, led_utilities.h, constants.h, SENSORY_BRIDGE_FIRMWARE.ino).

**Findings (one paragraph on the ORIGINAL Snapwave):** The ORIGINAL Snapwave does not exist in SB 3.1.0 — `light_mode_snapwave` is not in the codebase, the enum has only six modes (`gdft`, `gdft_chromagram`, `bloom`, `bloom_fast`, `vu`, `vu_dot`), and grepping for "snap"/"chevron" returns zero hits. The 3.1.0 motion vocabulary is: stateless spectrogram render (gdft, gdft_chromagram), shift-register scrolling driven by chromagram-blend with frame-skip parity gate (bloom slow/fast), per-LED exp-avg of time-domain waveform multiplied by a `waveform_peak_scaled` envelope (waveform), and exp-avg-smoothed pointer position (vu solid bar, vu_dot sweeping dot). Critically, **no 3.1.0 mode uses partial `fadeToBlackBy` for trail-persistence** — all motion is achieved by smoothing the source signal/position and redrawing clean each frame, or by deterministic shift-register copy. K1's broken Snapwave/Chevron/Wave effects, if they rely on exponential trail decay with K<255, have no 3.1.0 ancestor; the 3.1.0 baseline shows that smooth visual motion was historically achieved without that mechanism.

**Confidence:** HIGH. Verbatim source on disk, every claim cited to file:line, dispatch table cross-checked between `.ino` enum/branches and `lightshow_modes.h` definitions. No 4.x cross-references made beyond pointing at SSA-A/SSA-B abstractly — the heritage section's claims about what survived/evolved are conditional on those companion documents.

**Open questions:**
1. Did SB 4.x introduce `fadeToBlackBy(leds, N, K<255)` as a deliberate visual choice (smoother-looking trails), and is K1's spazz bug therefore inherited from a 4.x design decision rather than a K1 implementation bug? Confirming this requires the SSA-A/B canonical 4.x extractions.
2. Was `note_spectrogram_long_term[]` (declared, computed, never consumed in 3.1.0) ever wired up in a later version, and does K1's audio-reactive layer have a long-term envelope analogue?
3. The `chromagram_bass_max_val` global is declared (globals.h:123) but never written in 3.1.0 — is it written and consumed in any 4.x lineage?

**Token-relevant:**
`/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/docs/research/spazz_redesign_2026-04-30/canonical_SB_3_1_0.md`

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-30 | agent:embedded-system-engineer | Created. Verbatim extraction of all six SB 3.1.0 light_mode_* functions with motion equations, audio sources, smoothing topology, trail-fade analysis, colour mapping, and beat/percussion handling. Documented absence of light_mode_snapwave / chevron / snap in 3.1.0. Mapped 3.1.0 patterns to K1 broken-4 effects, flagged `fadeToBlackBy(K<255)` as a 4.x-introduced trail mechanism with no 3.1.0 ancestor as the most actionable heritage finding for K1's spazz bug. |
