---
abstract: "Canonical extraction of every motion-relevant light_mode_* function from upstream SensoryBridge 4.1.0 (SENSORY_BRIDGE_FIRMWARE/lightshow_modes.h). Documents the six modes that actually exist in 4.1.0 (gdft, chromagram_gradient, chromagram_dots, vu_dot, kaleidoscope, bloom), with verbatim per-frame motion equations, audio surface used (spectrogram_smooth/chromagram_smooth/audio_vu_level_*/note_chromagram), smoothing taus, persistence/trail mechanisms, and palette mappings. Critical finding: SB 4.1.0 contains NO light_mode_snapwave, NO light_mode_waveform, NO chevron_waves, NO lgp_wave_collision — the K1 broken-4 effects (ChevronWaves, ChevronWavesEnhanced, Snapwave, LGPWaveCollision) have no canonical upstream ancestor by name. Read when redesigning K1 motion effects to match SB 4.1.0 doctrine: dt-independent fixed alphas at 100 FPS, no acceleration physics, no springs, no per-frame phase accumulators driven by raw audio, single instantaneous follower with separate attack/decay, persistence via leds_16_prev sub-pixel scroll (bloom) or full-clear+redraw (others)."
---

# Canonical SensoryBridge 4.1.0 Motion Modes — Verbatim Source Analysis

**Source:** `K1.node1/references/Sensorybridge.sourcecode/SensoryBridge-4.1.0/SENSORY_BRIDGE_FIRMWARE/`

**Files inspected:** `lightshow_modes.h`, `globals.h`, `GDFT.h`, `led_utilities.h`, `utilities.h`, `constants.h`, `presets.h`, `SENSORY_BRIDGE_FIRMWARE.ino`.

**Captain directive 2026-04-30:** "STOP guessing. READ THE ACTUAL UPSTREAM. K1's internal `sensorybridge_reference/` files are K1-team mimics that drifted." This document is that read.

---

## 1. File map — every `light_mode_*` in SB 4.1.0

Source: `lightshow_modes.h` (1–499) and `SENSORY_BRIDGE_FIRMWARE.ino:58–68` (mode enum).

| Enum value | Function | Lines (lightshow_modes.h) | Status | Motion type |
|---|---|---|---|---|
| `LIGHT_MODE_GDFT` | `light_mode_gdft()` | 65–96 | active | spectrogram bar — no motion, just spectral height |
| `LIGHT_MODE_GDFT_CHROMAGRAM` | `light_mode_chromagram_gradient()` | 343–364 | active | mirrored gradient — no motion, just chromagram-driven hue ramp |
| `LIGHT_MODE_GDFT_CHROMAGRAM_DOTS` | `light_mode_chromagram_dots()` | 366–396 | active | 12 dots positioned by chromagram amplitude (mirror about centre) |
| `LIGHT_MODE_BLOOM` | `light_mode_bloom()` | 398–499 | active | sub-pixel scroll outward from centre (the canonical "bloom" / scroll-and-fade) |
| `LIGHT_MODE_VU_DOT` | `light_mode_vu_dot()` | 180–222 | active | single VU-driven dot pair, mirror about centre |
| `LIGHT_MODE_KALEIDOSCOPE` | `light_mode_kaleidoscope()` | 224–341 | active | 3D-noise field with per-channel position drift driven by low/mid/high band sums |

Plus three commented-out / dead functions:

| Function | Lines | Status |
|---|---|---|
| `light_mode_gdft_chromagram` (old) | 99–133 | **commented out**, replaced by `light_mode_chromagram_gradient` |
| `light_mode_bloom(bool fast_scroll)` (old) | 136–178 | **commented out**, replaced by current `light_mode_bloom()` |
| `test_mode()` | 55–62 | dev helper, single sine-driven dot, not in mode enum |

**There are NO other `light_mode_*` functions in SB 4.1.0.** Specifically there is **no `light_mode_snapwave`, no `light_mode_waveform`, no `light_mode_chevron_waves`, no `light_mode_lgp_wave_collision`, no `light_mode_oscilloscope`** in the canonical source. See §6 for what this means for the K1 broken-4 mapping.

The dispatch loop (`SENSORY_BRIDGE_FIRMWARE.ino:192–204`) wires the six enum values to the six functions above and nothing else.

---

## 2. Per-mode motion analysis

### 2.1 `light_mode_vu_dot()` — VU-driven dot pair (motion-relevant)

**Lines:** `lightshow_modes.h:180–222`

**Verbatim motion code:**

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
  ...
  set_dot_position(RESERVED_DOTS + 0, dot_pos_smooth * 0.5 + 0.5);
  set_dot_position(RESERVED_DOTS + 1, 0.5 - dot_pos_smooth * 0.5);
  ...
  draw_dot(leds_16, RESERVED_DOTS + 0, color);
  draw_dot(leds_16, RESERVED_DOTS + 1, color);
}
```

**Per-frame motion equation (parsed):**

1. `audio_vu_level_smooth ← audio_vu_level_average · α + audio_vu_level_smooth · (1-α)` where `α = mood_scale(0.10, 0.05) ∈ [0.05, 0.15]`. **Single-pass exponential smoothing of the VU input.**
2. `max_level` follower: attack `+= 0.1·distance` when audio rises 10 % above max, decay `*= 0.9999` per frame otherwise, floored at 0.0025.
3. `dot_pos = audio_vu_level_smooth / max_level` (saturating at 1.0). **This is the raw position — there is no `vel`, no `accel`, no spring.**
4. `dot_pos_smooth ← dot_pos · β + dot_pos_last · (1-β)` where `β = mood_scale(0.25, 0.24) ∈ [0.01, 0.49]`. **Second-pass exponential smoothing of position.**
5. Two mirrored dots at `0.5 ± 0.5·dot_pos_smooth` (centre origin = LED 64 at native 128, equivalent to K1's 79/80).
6. `draw_dot()` (led_utilities.h:393) computes coverage from `|position - last_position|` and fades trail by `1.0 / positional_distance` — **inherent motion-blur trail, no separate fade-to-black pass**.

**Audio sources:** `audio_vu_level_average` only (declared `globals.h:334`). No spectrogram, no chromagram, no beat.

**Smoothing taus:** Two cascaded EMAs with α scaled by MOOD knob. At 100 FPS effective render, α = 0.10 ≈ 10-frame time constant, β = 0.25 ≈ 4-frame time constant. **Both are dt-naïve fixed alphas — no `dt` term anywhere in this function.**

**Persistence/trail mechanism:** `clear_leds()` at line 215 wipes the buffer every frame; the trail is drawn implicitly by `draw_dot()` interpolating between `dot_pos_last` (stored in the DOT struct via `set_dot_position`) and `dot_pos`. No `leds_16_prev` involvement.

**Palette mapping:**

```cpp
SQ15x16 hue = chroma_val + hue_position;
CRGB16 color = hsv(hue, CONFIG.SATURATION, brightness);
```

Single hue per frame, sourced from chromatic-mode root `chroma_val` plus auto-shift `hue_position`. Brightness is `sqrt(dot_pos_smooth)`.

**Beat/percussion handling:** None. Pure VU-follower.

---

### 2.2 `light_mode_bloom()` — sub-pixel scroll outward (the canonical motion mode)

**Lines:** `lightshow_modes.h:398–499`

**Verbatim motion core (lines 398–402, 487–498):**

```cpp
void light_mode_bloom() {
  // Clear output
  memset(leds_16, 0, sizeof(CRGB16) * NATIVE_RESOLUTION);

  draw_sprite(leds_16, leds_16_prev, 128, 128, 0.250 + 1.750 * CONFIG.MOOD, 0.99);
  ...
  // (centre LEDs 63 and 64 set to chromagram-derived colour)
  leds_16[63] = { temp_col.r / 255.0, temp_col.g / 255.0, temp_col.b / 255.0 };
  leds_16[64] = leds_16[63];
  ...
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

`draw_sprite` from `led_utilities.h:1237–1280`:

```cpp
void draw_sprite(CRGB16 dest[], CRGB16 sprite[], uint32_t dest_length, uint32_t sprite_length, float position, SQ15x16 alpha) {
  int32_t position_whole = position;
  float position_fract = position - position_whole;
  SQ15x16 mix_right = position_fract;
  SQ15x16 mix_left = 1.0 - mix_right;

  for (uint16_t i = 0; i < sprite_length; i++) {
    int32_t pos_left = i + position_whole;
    int32_t pos_right = i + position_whole + 1;
    ...
    if (skip_left == false) {
      dest[pos_left].r += sprite[i].r * mix_left * alpha;
      ...
    }
    if (skip_right == false) {
      dest[pos_right].r += sprite[i].r * mix_right * alpha;
      ...
    }
  }
}
```

**Per-frame motion equation (parsed):**

1. **Clear** the output buffer every frame (`memset` line 400).
2. **Scroll the previous frame outward** by `offset = 0.25 + 1.75·MOOD` LEDs (range 0.25 → 2.00 LEDs/frame) using sub-pixel `draw_sprite` with `alpha = 0.99`. The sub-pixel split (`mix_left`/`mix_right`) is the smoothness mechanism — no integer-rounded jumps.
3. **Inject new colour at centre** (LEDs 63, 64) from `chromagram_smooth[]` summed over 12 notes (chromatic mode) or single hue (non-chromatic).
4. **Snapshot to `leds_16_prev`** before the edge-fade and mirror — so next frame's scroll input is clean.
5. **Hard edge fade** (lines 489–494): the outermost 32 LEDs of the right half are multiplied by `prog² = (i/31)²`, ensuring `leds_16[127]` → 0 and `leds_16[96]` → ~1.0. This is what guarantees old colour decays at the strip edge instead of accumulating forever.
6. **Mirror left from right** (lines 496–498): `leds_16[i] = leds_16[127-i]` for i ∈ [0, 64), making the bloom symmetrical about LED 64 (centre origin).

**Audio sources (centre colour computation, lines 451–479):**

```cpp
CRGB16 sum_color;
SQ15x16 share = 1 / 6.0;
for (uint8_t i = 0; i < 12; i++) {
  float prog = i / 12.0;
  SQ15x16 bin = chromagram_smooth[i];
  CRGB16 add_color = hsv(prog, CONFIG.SATURATION, bin*bin * share);
  sum_color.r += add_color.r; ... sum_color.g, .b
}
...
for (uint8_t i = 0; i < CONFIG.SQUARE_ITER; i++) {
  sum_color.r *= sum_color.r; ... .g, .b   // contrast curve
}
```

`chromagram_smooth[12]` is built in `make_smooth_chromagram()` (`led_utilities.h:1199–1234`) from `spectrogram_smooth[i % 12]` summed over `CONFIG.CHROMAGRAM_RANGE` bins, then peak-normalised by a `0.999`-decay max-follower with `0.05`-attack rise.

**Smoothing taus:**
- `chromagram_smooth` itself is normalised by a `0.999` decay / `0.05`-rate attack max follower (`led_utilities.h:1217–1227`).
- `spectrogram_smooth` is built in `get_smooth_spectrogram()` (`lightshow_modes.h:1–16`) with a fixed `0.75`-blend on both attack and decay — **a very fast EMA** (single-frame settle).
- `process_GDFT()` runs an additional low-pass on raw magnitudes (`GDFT.h:148–149`): `low_pass_array(..., SYSTEM_FPS, 1.0 + 10.0·MOOD_VAL)` — note this uses `SYSTEM_FPS` (not a fixed rate) so the cutoff frequency *scales with system FPS*, but inside `light_mode_bloom` `MOOD_VAL` is forced to 1.0 (`GDFT.h:61–63`), giving cutoff = 11 Hz.
- The bloom scroll itself has **no per-frame smoothing of `offset`** — it is a direct function of `MOOD` knob, which the user only changes manually.

**Persistence/trail mechanism:** `leds_16_prev` is the explicit prior-frame buffer (declared `globals.h:199`). `draw_sprite(leds_16, leds_16_prev, ..., alpha=0.99)` blends 99 % of the prior frame back into the new frame after a sub-pixel shift outward. Because the centre is freshly written every frame, there is no infinite accumulation; because `alpha = 0.99` (not 1.0), there is a tiny per-frame fade. The hard edge multiplier (`prog²` on the last 32 LEDs) is the second persistence brake — it forces decay near the strip ends so old colour never wraps or saturates.

**Palette mapping:** Sum of 12 HSV colours, one per chroma bin, weighted by `chromagram_smooth[i]² · (1/6)` (= `bin² / 6`). Saturation forced via `force_saturation(temp_col, 255·SATURATION)`. In non-chromatic mode the hue is overridden to `chroma_val + hue_position` via `force_hue` (lines 476–479).

**Beat/percussion handling:** None. Bloom is driven entirely by the chromagram envelope. `process_color_shift()` (called separately by the main loop, `led_utilities.h:1140–1197`) does derive `hue_shift_speed` from `novelty_curve[]` — but that affects `hue_position` globally, not the bloom geometry.

---

### 2.3 `light_mode_kaleidoscope()` — perlin-noise band-driven flow

**Lines:** `lightshow_modes.h:224–341`

**Verbatim motion core (lines 273–299):**

```cpp
SQ15x16 shift_speed = (SQ15x16)100 + ((SQ15x16)500 * (SQ15x16)CONFIG.MOOD);

SQ15x16 shift_r = (shift_speed * sum_low);
SQ15x16 shift_g = (shift_speed * sum_mid);
SQ15x16 shift_b = (shift_speed * sum_high);
...
pos_r += (float)shift_r;
pos_g += (float)shift_g;
pos_b += (float)shift_b;
...
for (uint8_t i = 0; i < 64; i++) {
  uint32_t y_pos_r = pos_r;
  ...
  uint32_t i_shifted = i + 18;
  uint32_t i_scaled = (i_shifted * i_shifted * i_shifted);

  SQ15x16 r_val = inoise16(i_scaled * 0.5 + y_pos_r) / 65536.0;
  SQ15x16 g_val = inoise16(i_scaled * 1.0 + y_pos_g) / 65536.0;
  SQ15x16 b_val = inoise16(i_scaled * 1.5 + y_pos_b) / 65536.0;
```

**Per-frame motion equation (parsed):**

1. Three band sums computed by integrating `spectrogram_smooth[0..19]` (low), `[20..39]` (mid), `[40..59]` (high) — each bin shaped as `bin·0.5 + bin²·0.5` (line 243, soft contrast).
2. Three followers: `brightness_low/mid/high` rise by `0.1·distance` when current sum exceeds them (asymmetric attack), then decay `*= 0.99` every frame (lines 245–271). **This is a single attack/decay follower per band — no spring, no second-order.**
3. `shift_r/g/b = (100 + 500·MOOD) · sum_low/mid/high`. **The phase accumulator velocity is a direct function of the audio sum.** Higher audio energy = faster scroll.
4. `pos_r += shift_r` (and g/b). **Phase accumulators per channel.** No ceiling, no wrap (relies on `uint32_t` wrap of `inoise16`).
5. Each LED i ∈ [0, 64) reads three perlin-noise samples at `i_cubed · {0.5, 1.0, 1.5} + pos_{r/g/b}`. The cube spreads spatial frequency.
6. Symmetric mirror: `leds_16[NATIVE_RESOLUTION - 1 - i] = leds_16[i]` (line 339).

**Audio sources:** `spectrogram_smooth[0..59]` partitioned into three 20-bin bands.

**Smoothing taus:**
- Per-band attack: `+= 0.1·distance` (≈ 10-frame time constant).
- Per-band decay: `*= 0.99` per frame (≈ 100-frame time constant — slow release).
- Phase accumulators have **no smoothing at all** — `pos_r += shift_r` directly. Smoothness comes from `inoise16` being continuous.

**Persistence/trail mechanism:** None. Buffer is fully recomputed every frame from noise; previous frame is discarded.

**Palette mapping:** Direct RGB from the three noise channels (gated by the three brightness followers). In non-chromatic mode (line 328) the colour is reduced to a single hue derived from max(r,g,b) brightness — see line 334: `led_hue = chroma_val + hue_position + sqrt(brightness)·0.05 + prog·0.10·hue_shifting_mix`.

**Beat/percussion handling:** None directly. The asymmetric attack/decay on band sums emulates onset response.

---

### 2.4 `light_mode_chromagram_dots()` — 12 dots positioned by amplitude (motion-relevant)

**Lines:** `lightshow_modes.h:366–396`

**Verbatim motion core:**

```cpp
void light_mode_chromagram_dots() {
  static SQ15x16 chromagram_last[12];

  memset(leds_16, 0, sizeof(CRGB16) * 128);

  low_pass_array_fixed(chromagram_smooth, chromagram_last, 12, LED_FPS, float(mood_scale(3.5, 1.5)));
  memcpy(chromagram_last, chromagram_smooth, sizeof(float) * 12);

  for (uint8_t i = 0; i < 12; i++) {
    ...
    SQ15x16 magnitude = chromagram_smooth[i] * 1.0;
    if (magnitude > 1.0) { magnitude = 1.0; }
    magnitude = magnitude * magnitude;
    ...
    set_dot_position(RESERVED_DOTS + i * 2 + 0, magnitude * 0.45 + 0.5);
    set_dot_position(RESERVED_DOTS + i * 2 + 1, 0.5 - magnitude * 0.45);

    draw_dot(leds_16, RESERVED_DOTS + i * 2 + 0, col);
    draw_dot(leds_16, RESERVED_DOTS + i * 2 + 1, col);
  }
}
```

**Per-frame motion equation:**

1. `low_pass_array_fixed(chromagram_smooth, chromagram_last, 12, LED_FPS, cutoff = 2.0..5.0 Hz)` — see `utilities.h:63–73`: the filter alpha is `1 - exp(-2π·f_c / LED_FPS)`. **This is the only place SB 4.1.0 uses an FPS-aware low-pass on a per-bin audio signal.**
2. Per chroma bin i: `dot position = 0.5 ± 0.45·magnitude²` (mirror about centre).
3. `draw_dot` interpolates between `position_last` and `position` over the line (motion-blur trail).

**Audio sources:** `chromagram_smooth[12]`.

**Smoothing taus:** `mood_scale(3.5, 1.5)` ⇒ cutoff ∈ [2.0 Hz, 5.0 Hz]. At LED_FPS = 100 this gives α ∈ [0.118, 0.272]. **Note this uses `LED_FPS` not `SYSTEM_FPS`** — i.e. it tracks the effective render frame rate.

**Persistence/trail:** Frame cleared; trail is the inherent `draw_dot` line-fill between last/current position.

**Palette mapping:** In chromatic mode each bin uses `note_colors[i]` (the canonical 12-note hue table from `constants.h:106–119`, evenly spaced 0.0833 increments). In non-chromatic mode all 12 dots share `chroma_val + hue_position + sqrt(1.0)·0.05`.

**Beat/percussion handling:** None.

---

### 2.5 `light_mode_chromagram_gradient()` — static spatial gradient (no motion)

**Lines:** `lightshow_modes.h:343–364`

```cpp
void light_mode_chromagram_gradient() {
  for (uint8_t i = 0; i < 64; i++) {
    SQ15x16 prog = i / 64.0;
    SQ15x16 note_magnitude = interpolate(prog, chromagram_smooth, 12) * 0.9 + 0.1;

    for (uint8_t s = 0; s < CONFIG.SQUARE_ITER; s++) {
      note_magnitude = (note_magnitude * note_magnitude) * SQ15x16(0.65) + (note_magnitude * SQ15x16(0.35));
    }
    ...
    CRGB16 col = hsv(led_hue, CONFIG.SATURATION, note_magnitude * note_magnitude);
    leds_16[64 + i] = col;
    leds_16[63 - i] = col;
  }
}
```

**No motion** — purely a position-to-chromagram-bin mapping each frame. Mirror about centre. Brightness = `note_magnitude²·²`. Included here for completeness; not motion-relevant.

---

### 2.6 `light_mode_gdft()` — spectrogram bar (no motion)

**Lines:** `lightshow_modes.h:65–96`

Per LED i ∈ [0, 64) directly reads `spectrogram_smooth[i]`, applies `SQUARE_ITER` contrast iterations, maps to HSV with `note_colors[i % 12]` (chromatic) or `chroma_val + hue_position` (non-chromatic). `shift_leds_up(64)` then `mirror_image_downwards`. **No motion** — bins occupy fixed LED positions. Included for completeness.

---

## 3. Audio surface used across modes

Source: `globals.h:130–169` (declarations), `GDFT.h:59–200` (`process_GDFT()` is the producer).

| Field | Type / size | Producer | Consumers (motion-relevant) | Notes |
|---|---|---|---|---|
| `spectrogram[64]` | `SQ15x16` | `process_GDFT()` | `get_smooth_spectrogram()` | Raw normalised Goertzel output |
| `spectrogram_smooth[64]` | `SQ15x16` | `get_smooth_spectrogram()` (lightshow_modes.h:1–16) | gdft, kaleidoscope, gradient, bloom (via chromagram_smooth) | Fast EMA `α = 0.75` both ways |
| `chromagram_smooth[12]` | `SQ15x16` | `make_smooth_chromagram()` (led_utilities.h:1199–1234) | bloom, gradient, dots | Built by binning `spectrogram_smooth[i % 12]`, then peak-normalised by `0.05`-attack / `0.999`-decay max follower |
| `note_chromagram[12]` | `float` | `process_GDFT()` (assigned elsewhere — declared `globals.h:143`) | `calc_chromagram_color()` (deprecated path) | Older API; current modes use `chromagram_smooth` |
| `audio_vu_level_average` | `SQ15x16` | i2s_audio.h pipeline | `light_mode_vu_dot` only | Single broadband VU |
| `novelty_curve[5]` | `SQ15x16` | `calculate_novelty()` (GDFT.h:202–243) | `process_color_shift()` only | Drives global `hue_position` cycling, NOT mode geometry |
| `note_colors[12]` | `SQ15x16` (constants.h:106) | static table | gdft, dots, gradient | Fixed hue table — even 1/12 spacing |
| `chroma_val` | `SQ15x16` | knobs.h | all modes (in non-chromatic) | User chroma knob position |
| `hue_position` | `SQ15x16` (globals.h:325) | `process_color_shift` | all modes (in non-chromatic) | Auto-shift driven by novelty |
| `CONFIG.MOOD` | `float` | knobs.h | bloom (scroll speed), kaleidoscope (band-shift gain), all dot modes (smoothing α) | Single user knob |

**Critical surface absent in SB 4.1.0:**
- No `beat`, `onset`, `tempo`, `bpm`, `confidence`, or `percussion` field anywhere.
- No `bands[8]` octave-band split (kaleidoscope re-derives 3 bands inline from `spectrogram_smooth`).
- No `rms` distinct from `audio_vu_level_average`.
- No `bins256[]` — SB 4.1.0 has only 64 Goertzel bins.

The K1 effect framework adds all of these via ControlBus, which is *new* — not from SB 4.1.0.

---

## 4. Cross-mode patterns — what is COMMON across SB 4.1.0 motion modes

This is the core finding for the K1 spazz/jerk redesign.

### 4.1 Smoothing approach

1. **Single instantaneous EMA per signal**, not cascaded chains. The deepest cascade in 4.1.0 is `light_mode_vu_dot` with two stages (input → `audio_vu_level_smooth`, then position → `dot_pos_smooth`). Bloom uses one stage (`chromagram_smooth` already smoothed upstream). Kaleidoscope uses one stage per band (`brightness_low/mid/high` followers).
2. **Asymmetric attack/decay** is the canonical SB 4.1.0 follower shape: `if (sig > follower) follower += distance · α_attack` else `follower *= decay_factor` (or `follower -= distance · α_decay`). Examples: `max_level` in vu_dot (0.1 attack, 0.9999 decay), `max_peak` in chromagram (0.05 attack, 0.999 decay), `goertzel_max_value` in `process_GDFT` (0.0050 attack, 0.0025 decay), `brightness_low/mid/high` in kaleidoscope (0.1 attack, 0.99 decay).
3. **Fixed alphas at the rendering frame rate.** None of the per-mode smoothing uses `dt`. The canonical assumption is "we render at LED_FPS ≈ 100, so a fixed α maps to a fixed time constant". The two exceptions are `low_pass_array_fixed(..., LED_FPS, cutoff)` (chromagram_dots) and `low_pass_array(magnitudes, ..., SYSTEM_FPS, ...)` (process_GDFT) — both convert cutoff Hz to alpha using the *current* sample rate, but they are OUTSIDE `render()` proper.
4. **MOOD knob scales smoothing α**, not motion velocity directly (with one exception: kaleidoscope multiplies shift speed by `100 + 500·MOOD`). The `mood_scale(centre, range)` helper (utilities.h:79–84) is `centre + range · (MOOD·2 - 1)` — symmetric ±range about centre.

### 4.2 dt usage

**There is NO `dt` term anywhere in `lightshow_modes.h`.** Not in followers, not in phase accumulators, not in fades. SB 4.1.0 is fundamentally a *frame-synchronous* renderer that assumes fixed-rate dispatch from the dedicated LED task (`led_task` declared `globals.h:215`, runs `process_color_shift()` and `light_mode_*` in a tight loop). Frame jitter is handled implicitly by the FastLED rate cap, not explicitly by per-effect dt scaling.

This is a deliberate design choice. The fixed-α taus only "make sense" because the loop rate is bounded.

### 4.3 Audio→motion mapping

Two patterns exist and only two:

**Pattern A — Direct amplitude → position** (vu_dot, chromagram_dots):
- The audio signal (after smoothing) IS the position.
- `position = signal · scale + bias`.
- Trail is drawn by `draw_dot()` interpolating between `last_position` and `position`.
- No phase accumulator, no velocity, no acceleration.

**Pattern B — Sub-pixel scroll of a persistent buffer** (bloom):
- New colour written at one fixed location (centre).
- Previous frame copied back into current frame, shifted outward by a sub-pixel offset.
- Edge multiplier (`prog²` on the last 32 LEDs) provides the persistence brake.
- Scroll velocity is a function of MOOD knob, NOT of the audio signal itself.

**Pattern C — Phase accumulator from band energy** (kaleidoscope):
- This is the only SB 4.1.0 mode where audio drives a *velocity* (`pos_r += shift_speed · sum_low`).
- The "smoothness" comes from `inoise16` being a continuous noise function — the position can jump by hundreds of integer units per frame and the rendered noise still varies smoothly.
- Critically: there is no spring/damper, no second-order dynamics. The position is the cumulative integral of band energy, full stop.

**The K1 spazz/jerk failure mode is none of A, B, or C.** ChevronWaves, Snapwave, and LGPWaveCollision use a *fourth* pattern not present in SB 4.1.0: a phase accumulator driven by raw band/onset signals into a per-LED `sin(k·x − ω·t)` plus springs/colour smoothing layers — see §6.

### 4.4 Persistence / trail

Three explicit mechanisms in SB 4.1.0, in order of prevalence:

1. **`memset(leds_16, 0, ...)` at frame start** + redraw with motion-blur trail from `draw_dot` line-fill between `last_position` and `position`. Used by: `light_mode_vu_dot`, `light_mode_chromagram_dots`, `light_mode_kaleidoscope` (no buffer reuse), `test_mode`.
2. **`leds_16_prev` sub-pixel scroll with `draw_sprite(... alpha=0.99)`** + hard edge fade (`prog²` on last 32 LEDs) + symmetric mirror. Used by: `light_mode_bloom` only.
3. **`fade_top_half`, `distort_logarithmic`, `load_leds_from_aux`** — all in the *commented-out* `light_mode_bloom(bool fast_scroll)` (lines 136–178). These are the OLD bloom mechanism; current bloom does not use them.

There is **no `fadeToBlackBy` global trail**, no `nblend` cross-fade, no per-LED individual decay rate, no spring/damped position update.

### 4.5 Beat / percussion handling

**SB 4.1.0 has no beat tracking and no percussion triggers** in any motion mode. The only "novelty" measurement is `novelty_curve[]` (`GDFT.h:202–243`), defined as the column-summed positive change in `spectrogram[]` over a 5-frame history. It is consumed only by `process_color_shift()` (`led_utilities.h:1140–1197`) to drive `hue_position` — i.e. it shifts the global colour wheel, not effect geometry.

The K1 broken-4 effects assume `ctx.controlBus.beat`, `onset`, `percussion_*` triggers exist and use them to *displace position* or *add energy to a phase accumulator*. **This audio→motion coupling does not exist in SB 4.1.0.** Modes that look beat-reactive (bloom, kaleidoscope) achieve the appearance via fast asymmetric envelope followers, not via discrete trigger events.

### 4.6 Palette mapping

Two canonical mechanisms:

1. **`note_colors[12]` table** (constants.h:106–119) — even 1/12 hue spacing. Used by: `light_mode_gdft` and `light_mode_chromagram_dots` in chromatic mode.
2. **`chroma_val + hue_position + sqrt(brightness)·0.05 + prog·0.10·hue_shifting_mix`** — the canonical non-chromatic hue formula. Used by every mode in non-chromatic mode. The `hue_shifting_mix` term flips sign as `hue_position` cycles, giving the slow palette breathing.

Saturation is forced via `force_saturation()` or `force_saturation_16()` (led_utilities.h:1085–1091, 1282–1353) — convert to HSV, set S to `CONFIG.SATURATION`, convert back. There is no per-LED hue jitter, no rainbow cycling.

---

## 5. Producer location of audio surface (so K1 can find equivalents)

| Field | Producer file:line in SB 4.1.0 | Loop location |
|---|---|---|
| `magnitudes[]` (raw int) | `GDFT.h:79–119` | per audio chunk |
| `magnitudes_normalized_avg[]` | `GDFT.h:118` | per audio chunk |
| `magnitudes_final[]` (low-passed) | `GDFT.h:148–149` | per audio chunk, cutoff = `1 + 10·MOOD_VAL` Hz |
| `spectrogram[64]` (peak-normalised) | `GDFT.h:170–199` | per audio chunk |
| `spectrogram_smooth[64]` | `lightshow_modes.h:1–16` | each LED frame, `α = 0.75` symmetric |
| `chromagram_smooth[12]` | `led_utilities.h:1199–1234` | each LED frame |
| `novelty_curve[5]` | `GDFT.h:202–243` | each LED frame |
| `audio_vu_level_average` | i2s_audio.h (not read in this audit) | per audio chunk |

The K1 ControlBus equivalent is `bands[8]`, `chroma[12]`, `bins256[]`, `rms`, `beat`, `onset`. The first three have direct SB 4.1.0 cousins (octave-summed `spectrogram_smooth`, `chromagram_smooth`, raw `spectrogram`). The latter three (`rms`, `beat`, `onset`) **have no canonical SB 4.1.0 producer** — `audio_vu_level_average` is only a proxy for `rms`, and `beat`/`onset` are K1 additions.

---

## 6. K1 broken-4 mapping — closest canonical SB 4.1.0 source

**The headline finding: there is no name-match.** SB 4.1.0 contains exactly six motion modes: `gdft`, `chromagram_gradient`, `chromagram_dots`, `bloom`, `vu_dot`, `kaleidoscope`. Nothing named "snapwave", "chevron", "wave_collision", or "waveform" exists in the canonical source. The K1 effects that claim SB lineage with those names are K1-team original implementations that drifted while citing SB.

The canonical mapping must therefore be by *motion mechanism*, not by name. Below is the closest-motion-pattern mapping for each broken K1 effect.

### 6.1 ChevronWavesEffect (K1) → closest canonical: `light_mode_kaleidoscope` band-flow + `light_mode_bloom` mirror geometry

**Why kaleidoscope:** Both share the "phase accumulator driven by band energy" pattern (Pattern C). Kaleidoscope's `pos_r += 100..600 · sum_low` is the canonical example. The K1 chevron uses a similar `phase += k · audioEnergy + ω·dt` accumulator.

**Where K1 drifts:**
- Kaleidoscope's `inoise16` is naturally smooth across phase jumps. K1's `sin(k·x − phase)` is also smooth across phase jumps **but only if the sine frequency `k` and phase `phase` are themselves smooth.** The K1 implementation tends to drive `k` (wavelength) directly from chroma/bands, which is what creates the "spazz" — sudden wavelength changes are not pre-smoothed.
- Kaleidoscope clamps band gain via per-band followers with explicit `0.1`-attack / `0.99`-decay (lines 245–271). K1 ChevronWaves audit reports show these followers are absent or too fast.

**Canonical parameters to inherit if redesigning to match SB doctrine:**
- Phase accumulator: `pos += (base_speed + audio_gain · band_sum) · 1` per frame, no `dt` term.
- Band signal: `band_sum = Σ spectrogram_smooth[range_lo..range_hi]` with each bin pre-shaped as `bin·0.5 + bin²·0.5`.
- Band follower: attack `+= 0.1·distance` if rising, decay `*= 0.99` per frame.
- Mirror geometry: write half-strip, copy `leds[N-1-i] = leds[i]`.
- No springs, no second-order dynamics, no per-LED independent state.

### 6.2 ChevronWavesEffectEnhanced (K1) → same canonical as 6.1 but with `light_mode_bloom`-style buffer scroll instead of per-pixel `sin()`

If "Enhanced" means trail-rich, the canonical mechanism is bloom's `draw_sprite(leds_16, leds_16_prev, ..., 0.99)` sub-pixel scroll outward. That gives motion blur for free without any per-pixel sine. The audio coupling becomes "centre colour from chromagram" (canonical bloom recipe), not "phase from beat".

### 6.3 SnapwaveLinearEffect (K1) → closest canonical: NONE. Closest mechanism: `light_mode_vu_dot` Pattern A (amplitude→position)

**Why no name-match:** "Snapwave" is not in SB 4.1.0.

**Closest mechanism:** A "snap" in the SB doctrine would be implemented as Pattern A — `position = audio · scale`, with the snap appearance arising from the asymmetric follower's fast-attack / slow-decay, not from a separate trigger. `light_mode_vu_dot` is the canonical example: when `audio_vu_level_smooth` jumps, `dot_pos` jumps with it; the trail is drawn by `draw_dot()` line-fill from `last_position` to `position`.

**The K1 drift:** SnapwaveLinear adds a *spring* between audio events and position, plus discrete onset/percussion triggers that displace the position non-smoothly. SB 4.1.0 has neither springs nor discrete triggers. The "snap" feel in SB comes purely from the EMA's α being high (e.g. `0.25..0.49` in vu_dot's `mix`).

**Canonical parameters to inherit:**
- `position = audio_signal · scale` (no integration, no spring).
- Two-stage cascaded EMA: input smoothing then position smoothing, both fixed-α.
- Auto-ranging via asymmetric follower (`+0.1·dist` attack, `*0.9999` decay, floor 0.0025 — see vu_dot lines 189–197).
- `draw_dot()` line-fill for trail.

### 6.4 LGPWaveCollisionEffect (K1) → closest canonical: NONE. Closest mechanism: `light_mode_bloom` Pattern B (sub-pixel scroll), but bloom is uni-directional outward, not collision

**Why no name-match:** "WaveCollision" is not in SB 4.1.0. SB never models two travelling fronts that collide.

**Closest mechanism:** Bloom's outward sub-pixel scroll IS a travelling wavefront. To emulate "collision" the canonical SB doctrine would be: render two bloom buffers (one travelling left-to-right from LED 0, one right-to-left from LED 127), additively blend them. The "collision" appearance would emerge from the additive overlap at the centre, not from per-LED collision physics.

**The K1 drift:** LGPWaveCollision computes per-LED phase-difference physics (`leftWave - rightWave`, energy = `sum(amplitude²)`, etc.) and uses that to drive position offsets and brightness modulators. None of this exists in SB. The "physics simulation" mental model is the source of the spazz — SB has no physics, only direct audio-to-pixel mappings with explicit smoothing.

**Canonical parameters to inherit:**
- Two sub-pixel scrollers using `draw_sprite(buf_a, buf_a_prev, alpha=0.99, offset)` and `draw_sprite(buf_b, buf_b_prev, alpha=0.99, -offset)`.
- Centre colour injection from `chromagram_smooth` (the bloom recipe, lines 451–479).
- Hard edge fade (`prog²` last 32 LEDs).
- Composite via `blend_buffers(BLEND_ADD, ...)` (led_utilities.h:1101–1121).
- No per-LED collision logic, no phase-difference computation, no energy field.

### 6.5 Summary table — broken K1 → canonical SB 4.1.0 mechanism (NOT name)

| K1 effect (broken) | Canonical SB 4.1.0 ancestor | Mechanism | Key SB code reference |
|---|---|---|---|
| ChevronWavesEffect | (none by name) | Pattern C: phase accumulator from band energy + symmetric mirror | `light_mode_kaleidoscope` lines 273–299; mirror at line 339 |
| ChevronWavesEffectEnhanced | (none by name) | Pattern B: `leds_16_prev` sub-pixel scroll outward | `light_mode_bloom` lines 398–402, 487–498; `draw_sprite` led_utilities.h:1237–1280 |
| SnapwaveLinearEffect | (none by name) | Pattern A: amplitude→position with cascaded EMA + auto-range follower | `light_mode_vu_dot` lines 180–222 |
| LGPWaveCollisionEffect | (none by name) | Two Pattern B scrollers blended additively | `light_mode_bloom` (twice) + `blend_buffers(BLEND_ADD)` led_utilities.h:1108–1113 |

---

## 7. Doctrine summary (one paragraph)

**SensoryBridge 4.1.0 motion is fixed-α frame-synchronous, with three patterns and no `dt`.** Every motion mode either (A) places a position directly from a smoothed amplitude and lets `draw_dot()` draw the line-fill trail, (B) sub-pixel-scrolls a persistent buffer outward from centre with a hard edge fade and a 0.99-alpha leak, or (C) integrates band-energy into a phase used by a continuous noise/sine evaluator. None use beat triggers, springs, second-order dynamics, per-LED state, or `dt`. All smoothing is asymmetric attack/decay with hard-coded alphas chosen at the LED_FPS ≈ 100 design point; the only knob is `MOOD`, which scales smoothing α via `mood_scale(centre, range)` and (in kaleidoscope) scales band-shift gain. Persistence is either zero (clear-and-redraw) or single-buffer (`leds_16_prev`) with a `0.99` leak and a `prog²` edge multiplier — no global `fadeToBlackBy`. Palette is either 12-tone `note_colors[]` (chromatic) or `chroma_val + hue_position + sqrt(brightness)·0.05 + prog·0.10·hue_shifting_mix` (non-chromatic), with saturation forced via HSV round-trip. **The K1 broken-4 effects departed from this doctrine by adopting per-LED physics, springs, beat triggers, and `dt`-scaled motion — none of which appear in canonical SB 4.1.0.**

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-30 | agent:embedded-firmware (subagent) | Created. Verbatim extraction of all motion-relevant `light_mode_*` from `SENSORY_BRIDGE_FIRMWARE/lightshow_modes.h` (498 lines), cross-referenced against `globals.h`, `GDFT.h`, `led_utilities.h`, `utilities.h`, `constants.h`, `presets.h`, and the `.ino` dispatch loop. Documents six canonical modes, three motion patterns (A/B/C), and the K1 broken-4 mapping (mechanism-only, no name match exists). |
